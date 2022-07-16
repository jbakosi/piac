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

int g_rpc_port = 65090;
std::mutex g_rpc_mtx;
std::condition_variable g_rpc_cv;
bool g_rpc_set = false;

bool use_strict_ports = false;
std::unordered_set< std::string > my_peers;
std::unordered_set< std::string > my_connected_peers;

// *****************************************************************************
void dispatch_query( const std::string& db_name,
                     zmqpp::socket& client,
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
      q.erase( 0, 5 );
      auto result = piac::db_query( db_name, std::move(q) );
      client.send( result );
    } else if (q[0]=='a' && q[1]=='d' && q[2]=='d') {
      q.erase( 0, 4 );
      auto result = piac::db_add( db_name, std::move(q) );
      client.send( result );
    }

  } else if (q == "peers") {

    std::stringstream peers_list;
    for (const auto& p : my_peers) peers_list << p << ' ';
    client.send( peers_list.str() );

  } else {

    NLOG(ERROR) << "unknown command";
    client.send( "unknown command" );

  }
}

// *****************************************************************************
void database_thread( const std::string& db_name,
                      const std::string& input_filename )
{
  el::Helpers::setThreadName( "db" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  NLOG(INFO) << "Using database: " << db_name;
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
}


// *****************************************************************************
void try_bind( zmqpp::socket& sock, int& port, int range ) {
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
void server_thread( zmqpp::context& ctx,
                    const std::string& db_name,
                    int server_port )
{
  el::Helpers::setThreadName( "server" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  zmqpp::socket client( ctx, zmqpp::socket_type::rep );
  try_bind( client, server_port, 10 );
  NLOG(INFO) << "Server bound to port " << server_port;
  // listen for messages
  while (not g_interrupted) {
    std::string request;
    auto res = client.receive( request );
    NLOG(DEBUG) << "Server received message: '" << request << "'";
    dispatch_query( db_name, client, std::move(request) );
  }
}

// *****************************************************************************
void peer_connect( zmqpp::context& ctx, const std::string& addr )
{
  if (my_connected_peers.find( addr ) != end(my_connected_peers)) return;
  // construct a DEALER socket, will send to peers
  zmqpp::socket peers( ctx, zmqpp::socket_type::dealer );
  peers.connect( "tcp://" + addr );
  my_connected_peers.insert( addr );
  NLOG(DEBUG) << "Connecting to peer at " + addr;
  zmqpp::message msg;
  msg << "ADD" << "localhost:" + std::to_string( g_rpc_port );
  peers.send( msg );
}

// *****************************************************************************
void receive_peer( zmqpp::context& ctx, zmqpp::message& request )
{
  std::string id, cmd, addr;
  request >> id >> cmd;
  if (cmd == "ADD") {
    request >> addr;
    NLOG(DEBUG) << "Daemon recv msg: " << "ADD " << addr;
    my_peers.insert( addr );
    NLOG(DEBUG) << "Number of peers: " << my_peers.size();
    peer_connect( ctx, addr );
  } else if (cmd == "REM") {
    request >> addr;
    NLOG(DEBUG) << "Daemon recv msg: " << "REM " << addr;
    my_peers.erase( addr );
  }
}

// *****************************************************************************
void peer_recv_thread( zmqpp::context& ctx ) {
  el::Helpers::setThreadName( "rpc-recv" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  zmqpp::socket peers( ctx, zmqpp::socket_type::router );
  {
    std::lock_guard lock( g_rpc_mtx );
    try_bind( peers, g_rpc_port, 10 );
    g_rpc_set = true;
    g_rpc_cv.notify_one();
  }
  NLOG(INFO) << "Daemon bound to RPC port " << g_rpc_port;
  // listen for messages
  zmqpp::poller poller;
  poller.add( peers );
  while (not g_interrupted) {
    if (poller.poll()) {
      if (poller.has_input( peers )) {
        zmqpp::message request;
        peers.receive( request );
        receive_peer( ctx, request );
      }
    }
  }
}

// *****************************************************************************
void peer_send_thread( zmqpp::context& ctx ) {
  el::Helpers::setThreadName( "rpc-send" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  {
    std::unique_lock lock( g_rpc_mtx );
    g_rpc_cv.wait( lock, []{ return g_rpc_set; } );
  }
  // connect to peers
  for (const auto& addr : my_peers) peer_connect( ctx, addr );
}

// *****************************************************************************
// piac daemon main
int main( int argc, char **argv ) {

  std::string version( "piac: " + piac::daemon_executable() + " v"
                       + piac::project_version() + "-" + piac::build_type() );

  // Defaults
  int server_port = 55090;
  std::string db_name( "piac.db" );
  std::string input_filename;
  std::string logfile = piac::daemon_executable() + ".log";

  // Process command line arguments
  int c;
  int option_index = 0;
  int detach = 0;
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
                  + std::to_string( g_rpc_port ) + "\n\n"
          "  -v, --version\n"
          "         Show version information\n";
        return EXIT_SUCCESS;

      case 'i':
        input_filename = optarg;
        break;

      case 'e':
        my_peers.insert( optarg );
        break;

      case 'p':
        server_port = atoi( optarg );
        use_strict_ports = true;
        break;

      case 'r':
        g_rpc_port = atoi( optarg );
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

  if (detach) {

    NLOG(INFO) << "Running in daemon mode";

    // Fork the current process. The parent process continues with a process ID
    // greater than 0.  A process ID lower than 0 indicates a failure in either
    // process.
    pid_t pid = fork();
    if (pid > 0) {
      NLOG(INFO) << "Forked PID: " << pid;
      return EXIT_SUCCESS;
    } else if (pid < 0) {
      return EXIT_FAILURE;
    }

    // Generate a session ID for the child process and ensure it is valid.
    if(setsid() < 0) {
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

  } else {

    NLOG(INFO) << "Running in standalone mode";

  }

  // initialize (thread-safe) zmq context
  zmqpp::context ctx;

  // start threads
  std::vector< std::thread > threads;
  threads.emplace_back( server_thread, std::ref(ctx), db_name, server_port );
  threads.emplace_back( database_thread, db_name, input_filename );
  threads.emplace_back( peer_recv_thread, std::ref(ctx) );
  threads.emplace_back( peer_send_thread, std::ref(ctx) );

  // wait for all threads to finish
  for (auto& t : threads) t.join();

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
