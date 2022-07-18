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
void dispatch_query(
  const std::string& db_name,
  zmqpp::socket& client,
  const std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::string&& q )
{
  NLOG(DEBUG) << "Interpreting message: '" << q << "'";

  if (q == "connect") {

    const std::string q( "accept" );
    NLOG(DEBUG) << "Sending message: '" << q << "'";
    client.send( "accept" );

  } else if (q[0]=='d' && q[1]=='b') {

    q.erase( 0, 3 );
    if (q[0]=='q' && q[1]=='u' && q[2]=='e' && q[3]=='r' && q[4]=='y') {
      q.erase( 0, 6 );
      auto result = piac::db_query( db_name, std::move(q) );
      client.send( result );
    } else if (q[0]=='a' && q[1]=='d' && q[2]=='d') {
      q.erase( 0, 4 );
      auto result = piac::db_add( db_name, std::move(q) );
      client.send( result );
    } else if (q[0]=='l' && q[1]=='i' && q[2]=='s' && q[3]=='t') {
      q.erase( 0, 5 );
      auto result = piac::db_list( db_name, std::move(q) );
      client.send( result );
    }

  } else if (q == "peers") {

    std::stringstream peers_list;
    for (const auto& [addr,sock] : my_peers) peers_list << addr << ' ';
    client.send( peers_list.str() );

  } else {

    NLOG(ERROR) << "unknown command";
    client.send( "unknown command" );

  }
}

// *****************************************************************************
void db_thread( const std::string& db_name,
                const std::string& input_filename,
                std::unordered_set< std::string >& my_hashes )
{
  el::Helpers::setThreadName( "db" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  NLOG(INFO) << "Using database: " << db_name;

  // initially optionally populate database
  if (input_filename.empty()) {
    auto ndoc = piac::get_doccount( db_name );
    NLOG(INFO) << "Number of documents: " << ndoc;
  } else {
    std::ifstream f( input_filename );
    if (not f.good()) {
      NLOG(ERROR) << "Database input file does not exist: " << input_filename;
    } else {
      NLOG(INFO) << "Populating database using: " << input_filename;
      piac::index_db( db_name, input_filename );
    }
  }

  { // initially query hashes of db entries
    auto hashes = piac::db_list_hash( db_name );
    std::lock_guard lock( g_hashes_mtx );
    g_hashes_access = false;
    for (auto&& h : hashes) my_hashes.insert( std::move(h) );
    g_hashes_access = true;
    g_hashes_cv.notify_one();
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
server_thread( zmqpp::context& ctx,
               const std::unordered_map< std::string, zmqpp::socket >& my_peers,
               const std::string& db_name,
               int server_port,
               bool use_strict_ports )
{
  el::Helpers::setThreadName( "server" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";

  zmqpp::socket client( ctx, zmqpp::socket_type::rep );
  try_bind( client, server_port, 10, use_strict_ports );
  NLOG(INFO) << "Server bound to port " << server_port;

  // listen for messages
  while (not g_interrupted) {
    std::string request;
    auto res = client.receive( request );
    NLOG(DEBUG) << "Server received message: '" << request << "'";
    dispatch_query( db_name, client, my_peers, std::move(request) );
  }
}

// *****************************************************************************
zmqpp::socket
connect( zmqpp::context& ctx, const std::string& addr, int rpc_port ) {
  zmqpp::socket dealer( ctx, zmqpp::socket_type::dealer );
  dealer.connect( "tcp://" + addr );
  NLOG(DEBUG) << "Connecting to peer at " + addr;
  return dealer;
}

// *****************************************************************************
void bcast_peers( int rpc_port,
                  std::unordered_map< std::string, zmqpp::socket >& my_peers )
{
  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "PEERS";
    msg << std::to_string( my_peers.size() + 1 );
    msg << "localhost:" + std::to_string(rpc_port);
    for (const auto& [taddr,sock] : my_peers) msg << taddr;
    sock.send( msg );
  }
}

// *****************************************************************************
bool
answer( zmqpp::context& ctx,
        zmqpp::message& request,
        std::unordered_map< std::string, zmqpp::socket >& my_peers,
        int rpc_port )
{
  std::string id, cmd;
  request >> id >> cmd;

  bool new_peers = false;
  if (cmd == "PEERS") {
    std::string size;
    request >> size;
    std::size_t num = stoul( size );
    NLOG(DEBUG) << "Daemon recv msg: PEERS ";
    while (num-- != 0) {
      std::string addr;
      request >> addr;
      if (addr != "localhost:" + std::to_string(rpc_port) &&
          my_peers.find(addr) == end(my_peers))
      {
        my_peers.emplace( addr, connect( ctx, addr, rpc_port ) );
        new_peers = true;
      }
    }
    NLOG(DEBUG) << "Number of peers: " << my_peers.size();
  }

  return new_peers;
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

  zmqpp::socket router( ctx, zmqpp::socket_type::router );
  try_bind( router, rpc_port, 10, use_strict_ports );
  NLOG(INFO) << "Daemon bound to RPC port " << rpc_port;

  // remove our address from peer list
  my_peers.erase( "localhost:" + std::to_string(rpc_port) );
  // add default peer
  for (int p = default_rpc_port; p < rpc_port; ++p)
    my_peers.emplace( "localhost:" + std::to_string( p ),
                      zmqpp::socket( ctx, zmqpp::socket_type::dealer ) );
  // initially connect to peers
  for (auto& [addr,sock] : my_peers) sock = connect( ctx, addr, rpc_port );
  NLOG(DEBUG) << "Initial number of peers: " << my_peers.size();

  { // log initial number of hashes (populated by db thread)
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    NLOG(DEBUG) << "Initial number of db hashes: " << my_hashes.size();
  }

  // listen to peers
  zmqpp::poller poller;
  poller.add( router );
  bool to_bcast_peers = true;
  while (not g_interrupted) {
    // broadcast to new peers
    if (to_bcast_peers) {
      bcast_peers( rpc_port, my_peers );
      to_bcast_peers = false;
    }
    // listen to peers
    if (poller.poll(500)) {
      if (poller.has_input( router )) {
        zmqpp::message request;
        router.receive( request );
        to_bcast_peers = answer( ctx, request, my_peers, rpc_port );
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
        rpc_port = atoi( optarg );
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
  threads.emplace_back( server_thread, std::ref(ctx), std::ref(my_peers),
                        db_name, server_port, use_strict_ports );
  threads.emplace_back( rpc_thread, std::ref(ctx), std::ref(my_peers),
                        std::ref(my_hashes), default_rpc_port, rpc_port,
                        use_strict_ports );
  threads.emplace_back( db_thread, db_name, input_filename,
                        std::ref(my_hashes) );

  // wait for all threads to finish
  for (auto& t : threads) t.join();

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
