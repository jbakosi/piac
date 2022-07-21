#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <zmqpp/zmqpp.hpp>
#include <getopt.h>
#include <unistd.h>

#include "logging.hpp"
#include "project_config.hpp"
#include "db.hpp"

std::mutex g_hashes_mtx;
std::condition_variable g_hashes_cv;
bool g_hashes_access = false;

// *****************************************************************************
void
db_update_hashes( const std::string& db_name,
                  std::unordered_set< std::string >& my_hashes )
{
  auto hashes = piac::db_list_hash( db_name, /* inhex = */ false );
  std::lock_guard lock( g_hashes_mtx );
  g_hashes_access = false;
  for (auto&& h : hashes) my_hashes.insert( std::move(h) );
  g_hashes_access = true;
  g_hashes_cv.notify_one();
  NLOG(DEBUG) << "Number of db hashes: " << my_hashes.size();
}

// *****************************************************************************
void db_client_op(
  zmqpp::socket& client,
  zmqpp::socket& db_rpc,
  const std::string& db_name,
  const std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_set< std::string >& my_hashes,
  zmqpp::message& msg )
{
  std::string cmd;
  msg >> cmd;
  NLOG(DEBUG) << "Recv msg " << cmd;

  if (cmd == "connect") {

    zmqpp::message reply;
    std::string accept( "accept" );
    reply << accept;
    NLOG(DEBUG) << "Sending reply '" << accept << "'";
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
      reply = piac::db_add( db_name, std::move(q), my_hashes );
      auto ndoc = piac::get_doccount( db_name );
      NLOG(DEBUG) << "Number of documents: " << ndoc;
      db_update_hashes( db_name, my_hashes );
      zmqpp::message note;
      note << "NEW";
      db_rpc.send( note );
      NLOG(DEBUG) << "Sent note on new documents";

    } else if (q[0]=='l' && q[1]=='i' && q[2]=='s' && q[3]=='t') {

      q.erase( 0, 5 );
      reply = piac::db_list( db_name, std::move(q) );

    }
    client.send( reply );

  } else if (cmd == "peers") {

    std::stringstream peers_list;
    for (const auto& [addr,sock] : my_peers) peers_list << addr << ' ';
    zmqpp::message reply;
    reply << peers_list.str();
    client.send( reply );

  } else {

    NLOG(ERROR) << "unknown command";
    client.send( "unknown command" );

  }
}

// *****************************************************************************
void
db_peer_op( const std::string& db_name,
            zmqpp::message& msg,
            zmqpp::socket& db_rpc,
            std::unordered_set< std::string >& my_hashes )
{
  std::string cmd;
  msg >> cmd;
  NLOG(DEBUG) << "Recv msg: " << cmd;

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
    NLOG(DEBUG) << "Looked up " << docs.size() << " hashes";

    zmqpp::message reply;
    reply << "PUT" << addr << std::to_string( docs.size() );
    for (const auto& d : docs) reply << d;
    db_rpc.send( reply );
    NLOG(DEBUG) << "Sending " << docs.size() << " entries";

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
      NLOG(DEBUG) << "Inserted " << docs.size() << " entries to db";
      auto ndoc = piac::get_doccount( db_name );
      NLOG(DEBUG) << "Number of documents: " << ndoc;
      db_update_hashes( db_name, my_hashes );
      zmqpp::message reply;
      reply << "NEW";
      db_rpc.send( reply );
      NLOG(DEBUG) << "Sent note on new documents";
    }

  } else {

    NLOG(ERROR) << "unknown cmd";

  }
}

// *****************************************************************************
void
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
    catch ( zmqpp::exception& e ) {}
  } while (++port < port+range );
  NLOG(ERROR) << "Could not bind to socket within range: ["
              << port << ',' << port+range << ')';
}

// *****************************************************************************
void
db_thread( zmqpp::context& ctx,
           const std::string& db_name,
           const std::string& input_filename,
           int server_port,
           bool use_strict_ports,
           const std::unordered_map< std::string, zmqpp::socket >& my_peers,
           std::unordered_set< std::string >& my_hashes )
{
  el::Helpers::setThreadName( "db" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  NLOG(INFO) << "Using database: " << db_name;

  // initially optionally populate database
  if (input_filename.empty()) {
    auto ndoc = piac::get_doccount( db_name );
    NLOG(INFO) << "Initial number of documents: " << ndoc;
  } else {
    std::ifstream f( input_filename );
    if (not f.good()) {
      NLOG(ERROR) << "Database input file does not exist: " << input_filename;
    } else {
      NLOG(INFO) << "Populating database using: " << input_filename;
      piac::index_db( db_name, input_filename );
    }
  }

  // initially query hashes of db entries
  db_update_hashes( db_name, my_hashes );

  // create socket that will listen to clients and bind to server port
  zmqpp::socket client( ctx, zmqpp::socket_type::reply );
  try_bind( client, server_port, 10, use_strict_ports );
  NLOG(INFO) << "Bound to port " << server_port;

  // create socket that will listen to requests for db lookups from peers
  zmqpp::socket db_rpc( ctx, zmqpp::socket_type::pair );
  db_rpc.bind( "inproc://db_rpc" );
  NLOG(DEBUG) << "Bound to inproc:://db_rpc";

  // listen to messages
  zmqpp::poller poller;
  poller.add( db_rpc );
  while (not g_interrupted) {

    zmqpp::message msg;
    if (client.receive( msg, /* dont_block = */ true )) {
      db_client_op( client, db_rpc, db_name, my_peers, my_hashes, msg );
    }

    if (poller.poll(100)) {
      if (poller.has_input( db_rpc )) {
        zmqpp::message msg;
        db_rpc.receive( msg );
        db_peer_op( db_name, msg, db_rpc, my_hashes );
      }
    }
  }
}

// *****************************************************************************
zmqpp::socket
connect( zmqpp::context& ctx, const std::string& addr, int rpc_port ) {
  // create socket to connect to peers at their rpc port
  zmqpp::socket dealer( ctx, zmqpp::socket_type::dealer );
  dealer.connect( "tcp://" + addr );
  NLOG(DEBUG) << "Connecting to peer at " + addr;
  return dealer;
}

// *****************************************************************************
void
rpc_bcast_peers( int rpc_port,
                 std::unordered_map< std::string, zmqpp::socket >& my_peers,
                 bool& to_bcast_peers )
{
  if (not to_bcast_peers) return;

  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "PEER";
    msg << std::to_string( my_peers.size() + 1 );
    msg << "localhost:" + std::to_string(rpc_port);
    for (const auto& [taddr,sock] : my_peers) msg << taddr;
    sock.send( msg );
  }

  to_bcast_peers = false;
}

// *****************************************************************************
void
rpc_bcast_hashes( int rpc_port,
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
    msg << "localhost:" + std::to_string(rpc_port);
    msg << std::to_string( my_hashes.size() );
    for (const auto& h : my_hashes) msg << h;
    sock.send( msg );
    NLOG(DEBUG) << "Broadcasting " << my_hashes.size() << " hashes to " << addr;
  }

  to_bcast_hashes = false;
}

// *****************************************************************************
void
rpc_send_db_requests(
  int rpc_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_map< std::string,
    std::unordered_set< std::string > >& db_requests,
  bool& to_send_db_requests )
{
  if (not to_send_db_requests) return;

  for (auto&& [addr,hashes] : db_requests) {
    zmqpp::message msg;
    msg << "REQ";
    msg << "localhost:" + std::to_string(rpc_port);
    msg << std::to_string( hashes.size() );
    for (const auto& h : hashes) msg << h;
    hashes.clear();
    my_peers.at( addr ).send( msg );
  }

  db_requests.clear();
  to_send_db_requests = false;
}

// *****************************************************************************
void
rpc_answer_rpc( zmqpp::context& ctx,
                zmqpp::message& msg,
                zmqpp::socket& db_rpc,
                std::unordered_map< std::string, zmqpp::socket >& my_peers,
                const std::unordered_set< std::string >& my_hashes,
                std::unordered_map< std::string,
                  std::unordered_set< std::string > >& db_requests,
                int rpc_port,
                bool& to_bcast_peers,
                bool& to_bcast_hashes,
                bool& to_send_db_requests )
{
  std::string id, cmd;
  msg >> id >> cmd;
  NLOG(DEBUG) << "Recv msg: " << cmd;

  if (cmd == "PEER") {

    std::string size;
    msg >> size;
    std::size_t num = stoul( size );
    while (num-- != 0) {
      std::string addr;
      msg >> addr;
      if (addr != "localhost:" + std::to_string(rpc_port) &&
          my_peers.find(addr) == end(my_peers))
      {
        my_peers.emplace( addr, connect( ctx, addr, rpc_port ) );
        to_bcast_peers = true;
        to_bcast_hashes = true;
      }
    }
    NLOG(DEBUG) << "Number of peers: " << my_peers.size();

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
    NLOG(DEBUG) << "Recv " << (r != end(db_requests) ? r->second.size() : 0)
                << " hashes from " << from;

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
    db_rpc.send( req );
    NLOG(DEBUG) << "Will prepare " << size << " db entries for " << addr;

  } else if (cmd == "DOC") {

    {
      std::unique_lock lock( g_hashes_mtx );
      g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    }
    std::string size;
    msg >> size;
    NLOG(DEBUG) << "Recv " << size << " db entries";
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
    db_rpc.send( req );
    NLOG(DEBUG) << "Attempting to insert " << docs_to_insert.size()
                << " db entries";

  } else {

    NLOG(WARNING) << "unknown cmd";

  }
}

void
rpc_answer_ipc( zmqpp::message& msg,
                std::unordered_map< std::string, zmqpp::socket >& my_peers,
                bool& to_bcast_hashes )
{
  std::string cmd;
  msg >> cmd;
  NLOG(DEBUG) << "Recv msg: " << cmd;

  if (cmd == "PUT") {

    std::string addr, size;
    msg >> addr >> size;
    NLOG(DEBUG) << "Prepared " << size << " db entries for " << addr;
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
    NLOG(DEBUG) << "Sent back " << size << " db entries to " << addr;

  } else if (cmd == "NEW") {

    to_bcast_hashes = true;

  } else {

    NLOG(WARNING) << "Recv unknown msg: " << cmd;

  }
}

// *****************************************************************************
void rpc_thread( zmqpp::context& ctx,
                 std::unordered_map< std::string, zmqpp::socket >& my_peers,
                 const std::unordered_set< std::string >& my_hashes,
                 int default_rpc_port,
                 int rpc_port,
                 bool use_strict_ports )
{
  el::Helpers::setThreadName( "rpc" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";

  // create socket that will listen to peers and bind to rpc port
  zmqpp::socket router( ctx, zmqpp::socket_type::router );
  try_bind( router, rpc_port, 10, use_strict_ports );
  NLOG(INFO) << "Bound to RPC port " << rpc_port;

  // remove our address from peer list
  my_peers.erase( "localhost:" + std::to_string(rpc_port) );
  // add default peers
  for (int p = default_rpc_port; p < rpc_port; ++p)
    my_peers.emplace( "localhost:" + std::to_string( p ),
                      zmqpp::socket( ctx, zmqpp::socket_type::dealer ) );
  // initially connect to peers
  for (auto& [addr,sock] : my_peers) sock = connect( ctx, addr, rpc_port );
  NLOG(DEBUG) << "Initial number of peers: " << my_peers.size();

  { // log initial number of hashes (populated by db thread)
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
  }
  NLOG(DEBUG) << "Initial number of db hashes: " << my_hashes.size();

  // create socket to send requests for db lookups from peers
  zmqpp::socket db_rpc( ctx, zmqpp::socket_type::pair );
  db_rpc.connect( "inproc://db_rpc" );
  NLOG(DEBUG) << "Connected to inproc:://db_rpc";

  std::unordered_map< std::string, std::unordered_set< std::string > >
    db_requests;

  // listen to peers
  zmqpp::poller poller;
  poller.add( router );
  poller.add( db_rpc );
  bool to_bcast_peers = true;
  bool to_bcast_hashes = true;
  bool to_send_db_requests = false;

  while (not g_interrupted) {
    rpc_bcast_peers( rpc_port, my_peers, to_bcast_peers );
    rpc_bcast_hashes( rpc_port, my_peers, my_hashes, to_bcast_hashes );
    rpc_send_db_requests( rpc_port, my_peers, db_requests,
                          to_send_db_requests );

    if (poller.poll()) {
      if (poller.has_input( router )) {
        zmqpp::message msg;
        router.receive( msg );
        rpc_answer_rpc( ctx, msg, db_rpc, my_peers, my_hashes, db_requests,
                        rpc_port, to_bcast_peers, to_bcast_hashes,
                        to_send_db_requests );
      }
      if (poller.has_input( db_rpc )) {
        zmqpp::message msg;
        db_rpc.receive( msg );
        rpc_answer_ipc( msg, my_peers, to_bcast_hashes );
      }
    }
  }
}

// *****************************************************************************
// piac daemon main
int main( int argc, char **argv ) {

  int detach = 0;
  std::vector< std::string > args( argv, argv+argc );
  for (std::size_t i=1; i<args.size(); ++i)
    if (args[i] == "--detach")
      detach = 1;

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

  std::string version( "piac: " + piac::daemon_executable() + " v"
                       + piac::project_version() + "-" + piac::build_type() );

  // Defaults
  int server_port = 55090;
  int default_rpc_port = 65090;
  int rpc_port = default_rpc_port;
  bool use_strict_ports = false;
  std::string db_name( "piac.db" );
  std::string input_filename;
  std::string logfile = piac::daemon_executable() + ".log";
  std::vector< std::string > peers;

  // Process command line arguments
  int c;
  int option_index = 0;
  static struct option long_options[] =
    { // NAME      ARGUMENT           FLAG     SHORTNAME/FLAGVALUE
      {"db",       required_argument, 0,       'd'},
      {"detach",   no_argument,       &detach,  1 },
      {"help",     no_argument,       0,       'h'},
      {"input",    required_argument, 0,       'i'},
      {"log",      required_argument, 0,       'l'},
      {"peer",     required_argument, 0,       'e'},
      {"port",     required_argument, 0,       'p'},
      {"rpc-port", required_argument, 0,       'r'},
      {"version",  no_argument,       0,       'v'},
      {0, 0, 0, 0}
    };
  while ((c = getopt_long( argc, argv, "d:e:hi:l:p:r:v",
                           long_options, &option_index )) != -1)
  {
    switch (c) {
      case 'd':
        db_name = optarg;
        break;

      case 'h':
        std::cout << "Usage: " + piac::daemon_executable() + " [OPTIONS]\n\n"
          "OPTIONS\n"
          "  -d, --db <directory>\n"
          "         Use database, default: " + db_name + "\n\n"
          "  --detach\n"
          "         Run as a daemon in the background\n\n"
          "  -h, --help\n"
          "         Show help message\n\n"
          "  -i, --input <filename.json>\n"
          "         Add the contents of file to database\n\n"
          "  -l, --log <filename.log>\n"
          "         Specify Log filename, default: " + logfile + "\n\n"
          "  -e, --peer <hostname>[:port]\n"
          "         Specify a peer to connect to\n\n"
          "  -p, --port <port>\n"
          "         Listen on server port given, default: "
                  + std::to_string( server_port ) + "\n\n"
          "  -r, --rpc-port <port>\n"
          "         Listen on RPC port given, default: "
                  + std::to_string( rpc_port ) + "\n\n"
          "  -v, --version\n"
          "         Show version information\n";
        return EXIT_SUCCESS;

      case 'i':
        input_filename = optarg;
        break;

      case 'e':
        peers.push_back( optarg );
        break;

      case 'p':
        server_port = atoi( optarg );
        use_strict_ports = true;
        break;

      case 'r':
        default_rpc_port = atoi( optarg );
        rpc_port = default_rpc_port;
        use_strict_ports = true;
        break;

      case 'l':
        logfile = optarg;
        break;

      case 'v':
        std::cout << version << '\n';
        return EXIT_SUCCESS;

      case '?':
        return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    printf( "%s: invalid options -- ", argv[0] );
    while (optind < argc) printf( "%s ", argv[optind++] );
    printf( "\n" );
    return EXIT_FAILURE;
  }

  // Setup logging
  setup_logging( piac::daemon_executable(), crash_handler, logfile,
                 /* file_logging = */ true, /* console_logging = */ true );

  NLOG(INFO) << version;
  std::cout <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the server of piac. It\n"
    "can run standalone or as a daemon in the background using --detach.\n"
    "You can use " + piac::cli_executable() + " to interact with it.\n";

  NLOG(INFO) << "Logging to " << logfile;
  if (detach) NLOG(INFO) << "Forked PID: " << getpid();

  // initialize (thread-safe) zmq context
  zmqpp::context ctx;

  // add initial peers
  std::unordered_map< std::string, zmqpp::socket > my_peers;
  for (const auto& p : peers)
    my_peers.emplace( p, zmqpp::socket( ctx, zmqpp::socket_type::dealer ) );

  // will store db entry hashes
  std::unordered_set< std::string > my_hashes;

  // start threads
  std::vector< std::thread > threads;

  threads.emplace_back( rpc_thread,
    std::ref(ctx), std::ref(my_peers), std::ref(my_hashes), default_rpc_port,
    rpc_port, use_strict_ports );

  threads.emplace_back( db_thread,
    std::ref(ctx), db_name, input_filename, server_port, use_strict_ports,
    std::ref(my_peers), std::ref(my_hashes) );

  // wait for all threads to finish
  for (auto& t : threads) t.join();

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
