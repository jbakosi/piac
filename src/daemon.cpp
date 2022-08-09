#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wundef"
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
  #pragma clang diagnostic ignored "-Wdocumentation"
  #pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include <zmqpp/zmqpp.hpp>

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <zmqpp/curve.hpp>

#include <getopt.h>
#include <unistd.h>

#include "project_config.hpp"
#include "crypto_util.hpp"
#include "logging_util.hpp"
#include "db.hpp"

namespace piac {

static std::mutex g_hashes_mtx;
static std::condition_variable g_hashes_cv;
static bool g_hashes_access = false;

// *****************************************************************************
static void
db_update_hashes( const std::string& db_name,
                  std::unordered_set< std::string >& my_hashes )
{
  auto hashes = piac::db_list_hash( db_name, /* inhex = */ false );
  std::lock_guard lock( g_hashes_mtx );
  g_hashes_access = false;
  my_hashes.clear();
  for (auto&& h : hashes) my_hashes.insert( std::move(h) );
  g_hashes_access = true;
  g_hashes_cv.notify_one();
  MDEBUG( "Number of db hashes: " << my_hashes.size() );
}

// *****************************************************************************
static void db_client_op(
  zmqpp::socket& client,
  zmqpp::socket& db_p2p,
  const std::string& db_name,
  const std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_set< std::string >& my_hashes,
  zmqpp::message& msg )
{
  std::string cmd;
  msg >> cmd;

  // extract hash of user auth from cmd if any, remove from cmd (and log)
  std::string user;
  auto u = cmd.rfind( "AUTH:" );
  if (u != std::string::npos) {
    user = cmd.substr( u + 5 );
    cmd.erase( u - 1 );
  }

  MDEBUG( "Recv msg " << cmd );

  if (cmd == "connect") {

    zmqpp::message reply;
    reply << "accept";
    MDEBUG( "Sending reply " << cmd );
    client.send( reply );

  } else if (cmd[0]=='d' && cmd[1]=='b') {

    cmd.erase( 0, 3 );
    auto q = std::move( cmd );
    std::string reply;
    if (q[0]=='q' && q[1]=='u' && q[2]=='e' && q[3]=='r' && q[4]=='y') {

      q.erase( 0, 6 );
      reply = piac::db_query( db_name, std::move(q) );

    } else if (q[0]=='a' && q[1]=='d' && q[2]=='d') {

      q.erase( 0, 4 );
      assert( not user.empty() );
      reply = piac::db_add( user, db_name, std::move(q), my_hashes );
      MDEBUG( "Number of documents: " <<piac::get_doccount( db_name ) );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message note;
      note << "NEW";
      db_p2p.send( note );
      MDEBUG( "Sent note on new documents" );

    } else if (q[0]=='r' && q[1]=='m') {

      q.erase( 0, 3 );
      assert( not user.empty() );
      reply = piac::db_rm( user, db_name, std::move(q), my_hashes );
      MDEBUG( "Number of documents: " << piac::get_doccount( db_name ) );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message note;
      note << "NEW";
      db_p2p.send( note );
      MDEBUG( "Sent note on removed documents" );

    } else if (q[0]=='l' && q[1]=='i' && q[2]=='s' && q[3]=='t') {

      q.erase( 0, 5 );
      reply = piac::db_list( db_name, std::move(q) );

    } else {

      reply = "unknown command";

    }
    client.send( reply );

  } else if (cmd == "peers") {

    zmqpp::message reply;
    if (not my_peers.empty()) {
      std::stringstream peers_list;
      for (const auto& [addr,sock] : my_peers) peers_list << addr << ' ';
      reply << peers_list.str();
    } else {
      reply << "No peers";
    }
    client.send( reply );

  } else {

    MERROR( "unknown command" );
    client.send( "unknown command" );

  }
}

// *****************************************************************************
static void
db_peer_op( const std::string& db_name,
            zmqpp::message& msg,
            zmqpp::socket& db_p2p,
            std::unordered_set< std::string >& my_hashes )
{
  std::string cmd;
  msg >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "GET") {

    std::string addr, size;
    msg >> addr >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > hashes;
    while (num-- != 0) {
      std::string h;
      msg >> h;
      hashes.emplace_back( std::move(h) );
    }

    auto docs = piac::db_get_docs( db_name, hashes );
    MDEBUG( "Looked up " << docs.size() << " hashes" );

    zmqpp::message reply;
    reply << "PUT" << addr << std::to_string( docs.size() );
    for (const auto& d : docs) reply << d;
    db_p2p.send( reply );
    MDEBUG( "Sending " << docs.size() << " entries" );

  } else if (cmd == "INS") {

    std::string size;
    msg >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > docs;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      auto hash = piac::sha256( doc );
      if (my_hashes.find(hash) == end(my_hashes)) {
        docs.emplace_back( std::move(doc) );
      }
    }
    if (not docs.empty()) {
      piac::db_put_docs( db_name, docs );
      MDEBUG(  "Inserted " << docs.size() << " entries to db" );
      auto ndoc = piac::get_doccount( db_name );
      MDEBUG( "Number of documents: " << ndoc );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message reply;
      reply << "NEW";
      db_p2p.send( reply );
      MDEBUG( "Sent note on new documents" );
    }

  } else {

    MERROR( "unknown cmd" );

  }
}

// *****************************************************************************
static void
try_bind( zmqpp::socket& sock, int& port, int range, bool use_strict_ports )
{
  if (use_strict_ports) {
    sock.bind( "tcp://*:" + std::to_string(port) );
    return;
  }
  do {
    try {
      sock.bind( "tcp://*:" + std::to_string(port) );
      return;
    }
    catch ( zmqpp::exception& ) {}
  } while (++port < port+range );
  MERROR( "Could not bind to socket within range: ["
          << port << ',' << port+range << ')' );
}

// *****************************************************************************
[[noreturn]] static void
db_thread( zmqpp::context& ctx_db,
           const std::string& db_name,
           int rpc_port,
           bool use_strict_ports,
           const std::unordered_map< std::string, zmqpp::socket >& my_peers,
           std::unordered_set< std::string >& my_hashes,
           int rpc_secure,
           const zmqpp::curve::keypair& rpc_server_keys,
           const std::vector< std::string >& rpc_authorized_clients )
{
  MLOG_SET_THREAD_NAME( "db" );
  MINFO( "db thread initialized" );
  MINFO( "Using database: " << db_name );

  // initially optionally populate database
  auto ndoc = piac::get_doccount( db_name );
  MINFO( "Initial number of documents: " << ndoc );

  // initially query hashes of db entries
  db_update_hashes( db_name, my_hashes );

  zmqpp::context ctx_rpc;

  // configure secure socket that will listen to clients and bind to RPC port
  zmqpp::auth authenticator( ctx_rpc );
  authenticator.set_verbose( true );
  authenticator.configure_domain( "*" );
  authenticator.allow( "127.0.0.1" );
  if (rpc_secure) {
    if (rpc_authorized_clients.empty()) {    // stonehouse
      authenticator.configure_curve( "CURVE_ALLOW_ANY" );
    } else {                                  // ironhouse
      for (const auto& cpk : rpc_authorized_clients) {
        authenticator.configure_curve( cpk );
      }
    }
  }
  // create socket that will listen to clients via RPC
  zmqpp::socket client( ctx_rpc, zmqpp::socket_type::reply );
  if (rpc_secure) {
    client.set( zmqpp::socket_option::identity, "IDENT" );
    int as_server = 1;
    client.set( zmqpp::socket_option::curve_server, as_server );
    client.set( zmqpp::socket_option::curve_secret_key,
                rpc_server_keys.secret_key );
  }
  try_bind( client, rpc_port, 10, use_strict_ports );

  MINFO( "Bound to RPC port " << rpc_port );
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Bound to RPC port " << rpc_port << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  // create socket that will listen to requests for db lookups from peers
  zmqpp::socket db_p2p( ctx_db, zmqpp::socket_type::pair );
  db_p2p.bind( "inproc://db_p2p" );
  MDEBUG( "Bound to inproc:://db_p2p" );

  // listen to messages
  zmqpp::poller poller;
  poller.add( db_p2p );
  while (1) {

    zmqpp::message msg;
    if (client.receive( msg, /* dont_block = */ true )) {
      db_client_op( client, db_p2p, db_name, my_peers, my_hashes, msg );
    }

    if (poller.poll(100)) {
      if (poller.has_input( db_p2p )) {
        zmqpp::message m;
        db_p2p.receive( m );
        db_peer_op( db_name, m, db_p2p, my_hashes );
      }
    }
  }
}

// *****************************************************************************
static zmqpp::socket
connect_peer( zmqpp::context& ctx, const std::string& addr ) {
  // create socket to connect to peers
  zmqpp::socket dealer( ctx, zmqpp::socket_type::dealer );
  dealer.connect( "tcp://" + addr );
  MDEBUG( "Connecting to peer at " + addr );
  return dealer;
}

// *****************************************************************************
static void
p2p_bcast_peers( int p2p_port,
                 std::unordered_map< std::string, zmqpp::socket >& my_peers,
                 bool& to_bcast_peers )
{
  if (not to_bcast_peers) return;

  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "PEER";
    msg << std::to_string( my_peers.size() + 1 );
    msg << "localhost:" + std::to_string(p2p_port);
    for (const auto& [taddr,sock] : my_peers) msg << taddr;
    sock.send( msg );
  }

  to_bcast_peers = false;
}

// *****************************************************************************
static void
p2p_bcast_hashes( int p2p_port,
                  std::unordered_map< std::string, zmqpp::socket >& my_peers,
                  const std::unordered_set< std::string >& my_hashes,
                  bool& to_bcast_hashes )
{
  if (not to_bcast_hashes) return;

  {
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
  }

  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "HASH";
    msg << "localhost:" + std::to_string(p2p_port);
    msg << std::to_string( my_hashes.size() );
    for (const auto& h : my_hashes) msg << h;
    sock.send( msg );
    MDEBUG( "Broadcasting " << my_hashes.size() << " hashes to " << addr );
  }

  to_bcast_hashes = false;
}

// *****************************************************************************
static void
p2p_send_db_requests(
  int p2p_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_map< std::string,
    std::unordered_set< std::string > >& db_requests,
  bool& to_send_db_requests )
{
  if (not to_send_db_requests) return;

  for (auto&& [addr,hashes] : db_requests) {
    zmqpp::message msg;
    msg << "REQ";
    msg << "localhost:" + std::to_string(p2p_port);
    msg << std::to_string( hashes.size() );
    for (const auto& h : hashes) msg << h;
    hashes.clear();
    my_peers.at( addr ).send( msg );
  }

  db_requests.clear();
  to_send_db_requests = false;
}

// *****************************************************************************
static void
p2p_answer_p2p( zmqpp::context& ctx_p2p,
                zmqpp::socket& db_p2p,
                zmqpp::message& msg,
                std::unordered_map< std::string, zmqpp::socket >& my_peers,
                const std::unordered_set< std::string >& my_hashes,
                std::unordered_map< std::string,
                  std::unordered_set< std::string > >& db_requests,
                int p2p_port,
                bool& to_bcast_peers,
                bool& to_bcast_hashes,
                bool& to_send_db_requests )
{
  std::string id, cmd;
  msg >> id >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "PEER") {

    std::string size;
    msg >> size;
    std::size_t num = stoul( size );
    while (num-- != 0) {
      std::string addr;
      msg >> addr;
      if (addr != "localhost:" + std::to_string(p2p_port) &&
          my_peers.find(addr) == end(my_peers))
      {
        my_peers.emplace( addr, connect_peer( ctx_p2p, addr ) );
        to_bcast_peers = true;
        to_bcast_hashes = true;
      }
    }
    MDEBUG( "Number of peers: " << my_peers.size() );

  } else if (cmd == "HASH") {

    {
      std::unique_lock lock( g_hashes_mtx );
      g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    }
    std::string from, size;
    msg >> from >> size;
    std::size_t num = stoul( size );
    while (num-- != 0) {
      std::string hash;
      msg >> hash;
      if (my_hashes.find(hash) == end(my_hashes)) {
        db_requests[ from ].insert( hash );
        to_send_db_requests = true;
      }
    }
    auto r = db_requests.find( from );
    MDEBUG( "Recv " << (r != end(db_requests) ? r->second.size() : 0)
            << " hashes from " << from );

  } else if (cmd == "REQ") {

    std::string addr, size;
    msg >> addr >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    zmqpp::message req;
    req << "GET" << addr << size;
    while (num-- != 0) {
      std::string hash;
      msg >> hash;
      req << hash;
    }
    db_p2p.send( req );
    MDEBUG( "Will prepare " << size << " db entries for " << addr );

  } else if (cmd == "DOC") {

    {
      std::unique_lock lock( g_hashes_mtx );
      g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    }
    std::string size;
    msg >> size;
    MDEBUG( "Recv " << size << " db entries" );
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > docs_to_insert;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      auto hash = piac::sha256( doc );
      if (my_hashes.find(hash) == end(my_hashes)) {
        docs_to_insert.emplace_back( std::move(doc) );
      }
    }
    zmqpp::message req;
    req << "INS" << std::to_string( docs_to_insert.size() );
    for (const auto& doc : docs_to_insert) req << doc;
    db_p2p.send( req );
    MDEBUG( "Attempting to insert " << docs_to_insert.size() << " db entries" );

  } else {

    MERROR( "unknown cmd" );

  }
}

// *****************************************************************************
static void
p2p_answer_ipc( zmqpp::message& msg,
                std::unordered_map< std::string, zmqpp::socket >& my_peers,
                bool& to_bcast_hashes )
{
  std::string cmd;
  msg >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "PUT") {

    std::string addr, size;
    msg >> addr >> size;
    MDEBUG( "Prepared " << size << " db entries for " << addr );
    std::size_t num = stoul( size );
    assert( num > 0 );
    zmqpp::message rep;
    rep << "DOC" << size;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      rep << doc;
    }
    my_peers.at( addr ).send( rep );
    MDEBUG( "Sent back " << size << " db entries to " << addr );

  } else if (cmd == "NEW") {

    to_bcast_hashes = true;

  } else {

    MERROR( "unknown cmd" );

  }
}

// *****************************************************************************
[[noreturn]] static void
p2p_thread( zmqpp::context& ctx_p2p,
            zmqpp::context& ctx_db,
            std::unordered_map< std::string, zmqpp::socket >& my_peers,
            const std::unordered_set< std::string >& my_hashes,
            int default_p2p_port,
            int p2p_port,
            bool use_strict_ports )
{
  MLOG_SET_THREAD_NAME( "rpc" );
  MINFO( "rpc thread initialized" );

  // create socket that will listen to peers and bind to p2p port
  zmqpp::socket router( ctx_p2p, zmqpp::socket_type::router );
  try_bind( router, p2p_port, 10, use_strict_ports );
  MINFO( "Bound to P2P port " << p2p_port );

  // remove our address from peer list
  my_peers.erase( "localhost:" + std::to_string(p2p_port) );
  // add default peers
  for (int p = default_p2p_port; p < p2p_port; ++p)
    my_peers.emplace( "localhost:" + std::to_string( p ),
                      zmqpp::socket( ctx_p2p, zmqpp::socket_type::dealer ) );
  // initially connect to peers
  for (auto& [addr,sock] : my_peers) sock = connect_peer( ctx_p2p, addr );
  MDEBUG( "Initial number of peers: " << my_peers.size() );

  { // log initial number of hashes (populated by db thread)
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
  }
  MDEBUG( "Initial number of db hashes: " << my_hashes.size() );

  // create socket to send requests for db lookups from peers
  zmqpp::socket db_p2p( ctx_db, zmqpp::socket_type::pair );
  db_p2p.connect( "inproc://db_p2p" );
  MDEBUG( "Connected to inproc:://db_p2p" );

  std::unordered_map< std::string, std::unordered_set< std::string > >
    db_requests;

  // listen to peers
  zmqpp::poller poller;
  poller.add( router );
  poller.add( db_p2p );
  bool to_bcast_peers = true;
  bool to_bcast_hashes = true;
  bool to_send_db_requests = false;

  while (1) {
    p2p_bcast_peers( p2p_port, my_peers, to_bcast_peers );
    p2p_bcast_hashes( p2p_port, my_peers, my_hashes, to_bcast_hashes );
    p2p_send_db_requests( p2p_port, my_peers, db_requests,
                          to_send_db_requests );

    if (poller.poll()) {
      if (poller.has_input( router )) {
        zmqpp::message msg;
        router.receive( msg );
        p2p_answer_p2p( ctx_p2p, db_p2p, msg, my_peers, my_hashes, db_requests,
                        p2p_port, to_bcast_peers, to_bcast_hashes,
                        to_send_db_requests );
      }
      if (poller.has_input( db_p2p )) {
        zmqpp::message msg;
        db_p2p.receive( msg );
        p2p_answer_ipc( msg, my_peers, to_bcast_hashes );
      }
    }
  }
}

// *****************************************************************************
static void
save_public_key( const std::string& filename, const std::string& public_key ) {
  if (filename.empty()) return;
  std::ofstream f( filename );
  if (f.is_open()) {
    f << public_key;
    f.close();
    MINFO( "Public key saved to file: " << filename );
  } else {
    std::cerr << "Cannot open file for writing: " << filename << '\n';
  }
}

// *****************************************************************************
static void
load_server_key( const std::string& filename, std::string& key ) {
  if (filename.empty()) return;
  std::ifstream f( filename );
  if (not f.good()) {
    epee::set_console_color( epee::console_color_red, /*bright=*/ false );
    std::cerr <<  "Cannot open file for reading: " << filename << '\n';
    epee::set_console_color( epee::console_color_default, /*bright=*/ false );
    return;
  }
  f >> key;
  assert( key.size() == 40 );
  f.close();
  MINFO( "Read key from file " + filename );
}

// *****************************************************************************
static void
load_client_keys( const std::string& filename,
                  std::vector< std::string >& keys )
{
  if (filename.empty()) return;
  std::ifstream f( filename );
  if (not f.good()) {
    epee::set_console_color( epee::console_color_red, /*bright=*/ false );
    std::cerr <<  "Cannot open file for reading: " << filename << '\n';
    epee::set_console_color( epee::console_color_default, /*bright=*/ false );
    exit( EXIT_FAILURE );
  }
  while (not f.eof()) {
    std::string key;
    f >> key;
    if (key.size() != 40) continue;
    keys.emplace_back( std::move(key) );
  }
  f.close();
  MINFO("Read " << keys.size() << " client keys from file " + filename);
}

// *****************************************************************************
static std::string
usage( const std::string& db_name,
       const std::string& logfile,
       const std::string& rpc_server_save_public_key_file,
       int rpc_port,
       int p2p_port )
{
  return "Usage: " + piac::daemon_executable() + " [OPTIONS]\n\n"
          "OPTIONS\n"
          "  --db <directory>\n"
          "         Use database, default: " + db_name + "\n\n"
          "  --detach\n"
          "         Run as a daemon in the background\n\n"
          "  --help\n"
          "         Show help message\n\n"
          "  --log-file <filename.log>\n"
          "         Specify log filename, default: " + logfile + "\n\n"
          "  --log-level <[0-4]>\n"
          "         Specify log level: 0: minimum, 4: maximum\n\n"
          "  --max-log-file-size <size-in-bytes> \n"
          "         Specify maximum log file size in bytes. Default: " +
          std::to_string( MAX_LOG_FILE_SIZE ) + ". Once the log file\n"
          "         grows past that limit, the next log file is created with "
                   "a UTC timestamp postfix\n"
          "         -YYYY-MM-DD-HH-MM-SS. Set --max-log-file-size 0 to prevent "
                  + piac::cli_executable() + " from managing\n"
          "         the log files.\n\n"
          "  --max-log-files <num> \n"
          "         Specify a limit on the number of log files. Default: " +
          std::to_string( MAX_LOG_FILES ) + ". The oldest log files\n"
          "         are removed. In production deployments, you would "
                   "probably prefer to use\n"
          "         established solutions like logrotate instead.\n\n"
          "  --peer <hostname>[:port]\n"
          "         Specify a peer to connect to\n\n"
          "  --rpc-bind-port <port>\n"
          "         Listen on RPC port given, default: "
                  + std::to_string( rpc_port ) + "\n\n"
          "  --rpc-secure\n"
          "         Enable secure connection to clients.\n\n"
          "  --rpc-server-public-key-file <filename>\n"
          "         Load server public key from file. Need to also set "
                   "--rpc-secure.\n\n"
          "  --rpc-server-secret-key-file <filename>\n"
          "         Load server secret key from file. Need to also set "
                   "--rpc-secure.\n\n"
          "  --rpc-authorized-clients-file <filename>\n"
          "         Only allow client connections with public keys read "
                   "from this file. Need to also\n"
          "         set --rpc-secure.\n\n"
          "  --rpc-server-save-public-key-file <filename>\n"
          "         Save self-generated server public key to file. Default: "
                  + rpc_server_save_public_key_file + ". Need to\n"
          "         also set --rpc-secure.\n\n"
          "  --p2p-bind-port <port>\n"
          "         Listen on P2P port given, default: "
                  + std::to_string( p2p_port ) + "\n\n"
          "  --version\n"
          "         Show version information\n\n";
}

} // piac::

// *****************************************************************************
// piac daemon main
int main( int argc, char **argv ) {

  // save command line
  std::vector< std::string > args( argv, argv+argc );
  std::stringstream cmdline;
  for (const auto& a : args) cmdline << a << ' ';

  // defaults
  int rpc_port = 55090;         // for client/server communication
  int default_p2p_port = 65090; // for peer-to-peer communication
  int p2p_port = default_p2p_port;
  bool use_strict_ports = false;
  std::string db_name( "piac.db" );
  std::string logfile( piac::daemon_executable() + ".log" );
  std::string log_level( "4" );
  std::size_t max_log_file_size = MAX_LOG_FILE_SIZE;
  std::size_t max_log_files = MAX_LOG_FILES;
  std::string version( "piac: " + piac::daemon_executable() + " v"
                       + piac::project_version() + "-" + piac::build_type() );
  std::vector< std::string > peers;
  std::string rpc_server_public_key_file;
  std::string rpc_server_secret_key_file;
  std::string rpc_authorized_clients_file;
  std::vector< std::string > rpc_authorized_clients;
  std::string rpc_server_save_public_key_file = "stonehouse.pub";

  // Process command line arguments
  int c;
  int option_index = 0;
  int detach = 0;
  int rpc_secure = 0;
  int num_err = 0;
  const int ARG_DB                              = 1000;
  const int ARG_HELP                            = 1001;
  const int ARG_LOG_FILE                        = 1002;
  const int ARG_LOG_LEVEL                       = 1003;
  const int ARG_MAX_LOG_FILE_SIZE               = 1004;
  const int ARG_MAX_LOG_FILES                   = 1005;
  const int ARG_PEER                            = 1006;
  const int ARG_RPC_PORT                        = 1007;
  const int ARG_RPC_SERVER_PUBLIC_KEY_FILE      = 1008;
  const int ARG_RPC_SERVER_SECRET_KEY_FILE      = 1009;
  const int ARG_RPC_AUTHORIZED_CLIENTS_FILE     = 1010;
  const int ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE = 1011;
  const int ARG_P2P_PORT                        = 1012;
  const int ARG_VERSION                         = 1013;
  static struct option long_options[] =
    {
      { "db", required_argument, nullptr, ARG_DB },
      { "detach", no_argument, &detach, 1 },
      { "help", no_argument, nullptr, ARG_HELP },
      { "log-file", required_argument, nullptr, ARG_LOG_FILE },
      { "log-level", required_argument, nullptr, ARG_LOG_LEVEL },
      { "max-log-file-size", required_argument, nullptr, ARG_MAX_LOG_FILE_SIZE },
      { "max-log-files", required_argument, nullptr, ARG_MAX_LOG_FILES },
      { "peer", required_argument, nullptr, ARG_PEER },
      { "rpc-bind-port", required_argument, nullptr, ARG_RPC_PORT },
      { "rpc-secure", no_argument, &rpc_secure, 1 },
      { "rpc-server-public-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_PUBLIC_KEY_FILE },
      { "rpc-server-secret-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_SECRET_KEY_FILE },
      { "rpc-authorized-clients-file", required_argument, nullptr,
        ARG_RPC_AUTHORIZED_CLIENTS_FILE },
      { "rpc-server-save-public-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE },
      { "p2p-bind-port", required_argument, nullptr, ARG_P2P_PORT },
      { "version", no_argument, nullptr, ARG_VERSION },
      { nullptr, 0, nullptr, 0 }
    };

  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {

      case ARG_DB: {
        db_name = optarg;
        break;
      }

      case ARG_HELP: {
        std::cout << version << "\n\n" <<
          piac::usage( db_name, logfile, rpc_server_save_public_key_file,
                       rpc_port, p2p_port );
        return EXIT_SUCCESS;
      }

      case ARG_PEER: {
        peers.push_back( optarg );
        break;
      }

      case ARG_RPC_PORT: {
        rpc_port = atoi( optarg );
        use_strict_ports = true;
        break;
      }

      case ARG_RPC_SERVER_PUBLIC_KEY_FILE: {
        rpc_server_public_key_file = optarg;
        break;
      }

      case ARG_RPC_SERVER_SECRET_KEY_FILE: {
        rpc_server_secret_key_file = optarg;
        break;
      }

      case ARG_RPC_AUTHORIZED_CLIENTS_FILE: {
        rpc_authorized_clients_file = optarg;
        break;
      }

      case ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE: {
        rpc_server_save_public_key_file = optarg;
        break;
      }

      case ARG_P2P_PORT: {
        default_p2p_port = atoi( optarg );
        p2p_port = default_p2p_port;
        use_strict_ports = true;
        break;
      }

      case ARG_LOG_FILE: {
        logfile = optarg;
        break;
      }

      case ARG_LOG_LEVEL: {
        std::stringstream s;
        s << optarg;
        int level;
        s >> level;
        if (level < 0) level = 0;
        if (level > 4) level = 4;
        log_level = std::to_string( level );
        break;
      }

      case ARG_MAX_LOG_FILE_SIZE: {
        std::stringstream s;
        s << optarg;
        s >> max_log_file_size;
        break;
      }

      case ARG_MAX_LOG_FILES: {
        std::stringstream s;
        s << optarg;
        s >> max_log_files;
        break;
      }

      case ARG_VERSION: {
        std::cout << version << '\n';
        return EXIT_SUCCESS;
      }

      case '?': {
        ++num_err;
        break;
      }
    }
  }

  if (optind < argc) {
    printf( "%s: invalid options -- ", argv[0] );
    while (optind < argc) printf( "%s ", argv[optind++] );
    printf( "\n" );
    return EXIT_FAILURE;
  }

  if (num_err) {
    std::cerr << "Erros during parsing command line\n"
              << "Command line: " + cmdline.str() << '\n'
              << piac::usage( db_name, logfile,rpc_server_save_public_key_file,
                              rpc_port, p2p_port );
    return EXIT_FAILURE;
  }

  if ((not rpc_server_public_key_file.empty() ||
       not rpc_server_secret_key_file.empty() ||
       not rpc_authorized_clients_file.empty()) &&
       not rpc_secure)
  {
    std::cerr << "Need --rpc-secure to secure RPC channel.\n";
    return EXIT_FAILURE;
  }

  if ((not rpc_server_public_key_file.empty() &&
       rpc_server_secret_key_file.empty()) ||
      (rpc_server_public_key_file.empty() &&
       not rpc_server_secret_key_file.empty()))
  {
    std::cerr << "Need --rpc-authorized-clients-file for authorized RPC "
                 "clients.\n";
    return EXIT_FAILURE;
  }

  if (detach) {
    // Fork the current process. The parent process continues with a process ID
    // greater than 0.  A process ID lower than 0 indicates a failure in either
    // process.
    pid_t pid = fork();
    if (pid > 0) {
      std::cout << "Running in daemon mode. Forked PID: " << pid
                << ". See the log file for details." << std::endl;
      return EXIT_SUCCESS;
    } else if (pid < 0) {
      return EXIT_FAILURE;
    }

    // Generate a session ID for the child process and ensure it is valid.
    if (setsid() < 0) {
      // Log failure and exit
      std::cerr << "Could not generate session ID for child process\n";
      // If a new session ID could not be generated, we must terminate the child
      // process or it will be orphaned.
      return EXIT_FAILURE;
    }

    // A daemon cannot use the terminal, so close standard file descriptors for
    // security reasons and also because ctest hangs in daemon mode waiting for
    // stdout and stderr to be closed.
    close( STDIN_FILENO );
    close( STDOUT_FILENO );
    close( STDERR_FILENO );
  }

  epee::set_console_color( epee::console_color_green, /* bright = */ false );
  std::cout << version << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );
  std::cout <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the server of piac. It\n"
    "can run standalone or as a daemon in the background using --detach.\n"
    "You can use " + piac::cli_executable() + " to interact with it.\n";

  piac::setup_logging( logfile, log_level, /* console_logging = */ false,
                       max_log_file_size, max_log_files );

  MINFO( "Command line: " + cmdline.str() );

  // setup RPC security
  int rpc_ironhouse = 1;
  zmqpp::curve::keypair rpc_server_keys;
  if (rpc_secure) {
    piac::load_server_key( rpc_server_public_key_file,
                           rpc_server_keys.public_key );
    piac::load_server_key( rpc_server_secret_key_file,
                           rpc_server_keys.secret_key );
    piac::load_client_keys( rpc_authorized_clients_file,
                            rpc_authorized_clients );
    // fallback to stonehouse if needed
    if (rpc_server_keys.secret_key.empty() ||
        rpc_server_keys.public_key.empty() ||
        rpc_authorized_clients.empty())
    {
      rpc_ironhouse = 0;
      if (rpc_server_keys.secret_key.empty() ||
          rpc_server_keys.public_key.empty())
      {
        rpc_server_keys = zmqpp::curve::generate_keypair();
        piac::save_public_key( rpc_server_save_public_key_file,
                               rpc_server_keys.public_key );
      }
      rpc_authorized_clients.clear();
    }
  }

  // echo RPC security configured
  if (rpc_secure) {
    if (rpc_ironhouse) {
      epee::set_console_color( epee::console_color_green, /*bright=*/ false );
      std::string ironhouse( "Connections to this server are secure: "
        "Only authenticated connections are accepted." );
      std::cout << ironhouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( ironhouse );
    } else {
      std::string stonehouse( "Connections to this server are secure but not "
        "authenticated: ALL client connections are accepted." );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      std::cout << stonehouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( stonehouse );
      std::cout << "RPC server public key: "
                << rpc_server_keys.public_key << '\n';
    }
  } else {
    epee::set_console_color( epee::console_color_red, /* bright = */ false );
    std::string grasslands( "WARNING: Connections to this server are not "
      "secure" );
    std::cout << grasslands << '\n';
    MWARNING( grasslands );
  }
  epee::set_console_color( epee::console_color_default, /* bright = */false );

  if (rpc_secure) {
    MINFO( "RPC server public key: " << rpc_server_keys.public_key );
  }

  if (detach) {
    MINFO( "Forked PID: " << getpid() );
  }

  // initialize (thread-safe) zmq contexts
  zmqpp::context ctx_p2p;       // for p2p comm
  zmqpp::context ctx_db;        // for inproc comm

  // add initial peers
  std::unordered_map< std::string, zmqpp::socket > my_peers;
  for (const auto& p : peers)
    my_peers.emplace( p, zmqpp::socket( ctx_p2p, zmqpp::socket_type::dealer ) );

  // will store db entry hashes
  std::unordered_set< std::string > my_hashes;

  // start threads
  std::vector< std::thread > threads;

  threads.emplace_back( piac::p2p_thread,
    std::ref(ctx_p2p), std::ref(ctx_db), std::ref(my_peers),
    std::ref(my_hashes), default_p2p_port, p2p_port, use_strict_ports );

  threads.emplace_back( piac::db_thread,
    std::ref(ctx_db), db_name, rpc_port, use_strict_ports, std::ref(my_peers),
    std::ref(my_hashes), rpc_secure, std::ref(rpc_server_keys),
    std::ref(rpc_authorized_clients) );

  // wait for all threads to finish
  for (auto& t : threads) t.join();

  MDEBUG( "graceful exit" );

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
