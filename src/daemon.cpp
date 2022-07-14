#include <vector>
#include <iostream>
#include <thread>
#include <zmqpp/zmqpp.hpp>
#include <getopt.h>
#include <unistd.h>

#include "logging.hpp"
#include "project_config.hpp"
#include "db.hpp"

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
void interpret_query( const std::string& db_name, zmqpp::socket& socket,
                      std::string&& q )
{
  NLOG(DEBUG) << "Interpreting message: '" << q << "'";

  if (q[0]=='c' && q[1]=='o') {

    const std::string q( "accept" );
    NLOG(DEBUG) << "Sending message: '" << q << "'";
    socket.send( "accept" );

  } else if (q[0]=='d' && q[1]=='b') {

    q.erase( 0, 3 );
    if (q[0]=='q' && q[1]=='u' && q[2]=='e' && q[3]=='r' && q[4]=='y') {
      q.erase( 0, 5 );
      auto result = piac::db_query( db_name, std::move(q) );
      socket.send( result );
    } else if (q[0]=='a' && q[1]=='d' && q[2]=='d') {
      q.erase( 0, 4 );
      auto result = piac::db_add( db_name, std::move(q) );
      socket.send( result );
    }

  } else {

    NLOG(ERROR) << "unknown command";
    socket.send( "unknown command" );

  }
}

// *****************************************************************************
void server_thread( const std::string& db_name, int server_port ) {
  el::Helpers::setThreadName( "server" );
  NLOG(INFO) << el::Helpers::getThreadName() << " thread initialized";
  // initialize zmq context
  zmqpp::context context;
  // construct a REP (reply) socket and bind to interface
  zmqpp::socket socket{ context, zmqpp::socket_type::rep };
  socket.bind( "tcp://*:" + std::to_string(server_port) );
  NLOG(INFO) << "Server bound to port " << server_port;
  // listen for messages
  while (not g_interrupted) {
    std::string request;
    auto res = socket.receive( request );
    NLOG(DEBUG) << "Received message: '" << request << "'";
    interpret_query( db_name, socket, std::move(request) );
  }
}

// *****************************************************************************
// piac daemon main
int main( int argc, char **argv ) {

  // Setup logging
  std::string logfile( piac::daemon_executable() + ".log" );
  setup_logging( piac::daemon_executable(), crash_handler, logfile,
                 /* file_logging = */ true, /* console_logging = */ true );

  std::string version( "piac: " + piac::daemon_executable() + " v"
                       + piac::project_version() + "-" + piac::build_type() );

  // Defaults
  int server_port = 55090;
  std::string db_name( "piac.db" );
  std::string input_filename;

  // Process command line arguments
  int c;
  int option_index = 0;
  int detach = 0;
  static struct option long_options[] =
    {
      // NAME     ARGUMENT           FLAG     SHORTNAME/VALUE
      {"db",      required_argument, 0,       'd'},
      {"detach",  no_argument,       &detach,  1 },
      {"help",    no_argument,       0,       'h'},
      {"input",   required_argument, 0,       'i'},
      {"port",    required_argument, 0,       'p'},
      {"version", no_argument,       0,       'v'},
      {0, 0, 0, 0}
    };
  while ((c = getopt_long(argc, argv, "d:hi:p:v",
                long_options, &option_index)) != -1)
  {
    switch (c) {
      case 'd':
        NLOG(DEBUG) << "db " << optarg;
        db_name = optarg;
        break;

      case 'h':
        NLOG(DEBUG) << "help";
        NLOG(INFO) << "Usage: " + piac::daemon_executable() + " [OPTIONS]\n\n"
          "OPTIONS\n"
          "  -d, --db <directory>\n"
          "         Use database, default: " + db_name + "\n\n"
          "  --detach\n"
          "         Run as a daemon in the background.\n\n"
          "  -h, --help\n"
          "         Show help message.\n\n"
          "  -i, --input <filename.json>\n"
          "         Add the contents of file to database.\n\n"
          "  -p, --port <port>\n"
          "         Listen on custom port, default: "
                  + std::to_string( server_port ) + "\n\n"
          "  -v, --version\n"
          "         Show version information\n";
        return EXIT_SUCCESS;

      case 'i':
        NLOG(DEBUG) << "input " << optarg;
        input_filename = optarg;
        break;

      case 'p':
        NLOG(DEBUG) << "port " << optarg;
        server_port = atoi( optarg );
        break;

      case 'v':
        NLOG(DEBUG) << "version";
        NLOG(INFO) << version;
        return EXIT_SUCCESS;

      case '?':
        return EXIT_FAILURE;

      default:
        NLOG(INFO) << "getopt() returned character code 0" << c;
    }
  }

  if (optind < argc) {
    printf( "%s: invalid options -- ", argv[0] );
    while (optind < argc) printf( "%s ", argv[optind++] );
    printf( "\n" );
    return EXIT_FAILURE;
  }

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

  // start some threads
  std::vector< std::thread > daemon_threads;
  daemon_threads.emplace_back( server_thread, db_name, server_port );
  daemon_threads.emplace_back( database_thread, db_name, input_filename );

  // wait for all threads to finish
  for (auto& t : daemon_threads) t.join();

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
