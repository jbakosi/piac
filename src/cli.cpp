#include <zmqpp/zmqpp.hpp>
#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>

//#define BOOST_BIND_GLOBAL_PLACEHOLDERS
//#include "wallet/monero_wallet_full.h"

#include "project_config.hpp"
#include "logging.hpp"

// *****************************************************************************
void disconnect( bool& connected,
                 zmqpp::socket& server,
                 const std::string& host )
{
  MDEBUG( "disconnect" );
  if (connected) {
    server.disconnect( "tcp://" + host );
    std::cout << "Disconnected\n";
  } else {
    std::cout << "Not connected\n";
  }
  connected = false;
}

// *****************************************************************************
void connect( bool& connected, zmqpp::socket& server, const std::string& host )
{
  MDEBUG( "connect" );
  if (connected) {
    std::cout << "You need to disconnect first\n";
    return;
  }
  std::cout << "Connecting to " + piac::daemon_executable() << " at " << host
            << '\n';
  server.connect( "tcp://" + host );
  server.send( "connect" );
  // wait for reply from server
  std::string reply;
  auto res = server.receive( reply );
  MDEBUG( reply );
  if (reply == "accept") {
    std::cout << "Connected\n";
    connected = true;
  }
}

// *****************************************************************************
void status( bool connected ) {
  MDEBUG( "status" );
  if (connected)
    std::cout << "Connected\n";
  else
    std::cout << "Not connected\n";
}

// *****************************************************************************
void send_cmd( bool connected, zmqpp::socket& server, const std::string& cmd ) {
  MDEBUG( cmd );
  if (not connected) {
    std::cout << "Not connected. Use 'connect' to connect to "
               << piac::daemon_executable() << ".\n";
    return;
  }
  // send message to server with command
  server.send( cmd );
  // wait for reply from server
  std::string reply;
  auto res = server.receive( reply );
  std::cout << reply << '\n';
  MDEBUG( reply );
}

enum COLOR { RED, GREEN, BLUE, GRAY, YELLOW };

// *****************************************************************************
std::string color_string( const std::string &s, COLOR color = GRAY ) {
  std::string ret;
  if (color == RED) ret = "\033[0;31m";
  if (color == GREEN) ret = "\033[0;32m";
  if (color == BLUE) ret = "\033[0;34m";
  if (color == GRAY) ret = "\033[0;37m";
  if (color == YELLOW) ret = "\033[0;33m";
  return ret + s + "\033[0m";
}

// *****************************************************************************
void create_new_wallet() {
//  auto w = monero_wallet_full::create_wallet_random( "", "", monero_network_type::TESTNET );
//  std::cout << "seed: " << w->get_mnemonic() << '\n';
}

// *****************************************************************************
int main( int argc, char **argv ) {

  std::string logfile( piac::cli_executable() + ".log" );
  std::string log_level( "4" );
  std::string version( "piac: " + piac::cli_executable() + " v"
                       + piac::project_version() + "-"
                       + piac::build_type() );
  std::size_t max_log_file_size = MAX_LOG_FILE_SIZE;
  std::size_t max_log_files = MAX_LOG_FILES;

  // Process command line arguments
  int c;
  int option_index = 0;
  const int ARG_HELP              = 1000;
  const int ARG_LOG_FILE          = 1001;
  const int ARG_LOG_LEVEL         = 1002;
  const int ARG_MAX_LOG_FILE_SIZE = 1003;
  const int ARG_MAX_LOG_FILES     = 1004;
  const int ARG_VERSION           = 1005;
  static struct option long_options[] =
    { // NAME               ARGUMENT           FLAG SHORTNAME/FLAGVALUE
      {"help",              no_argument,       0,   ARG_HELP             },
      {"log-file",          required_argument, 0,   ARG_LOG_FILE         },
      {"log-level",         required_argument, 0,   ARG_LOG_LEVEL        },
      {"max-log-file-size", required_argument, 0,   ARG_MAX_LOG_FILE_SIZE},
      {"max-log-files",     required_argument, 0,   ARG_MAX_LOG_FILES    },
      {"version",           no_argument,       0,   ARG_VERSION          },
      {0, 0, 0, 0}
    };
  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
      case ARG_HELP: {
        std::cout << version << "\n\n" <<
          "Usage: " + piac::cli_executable() + " [OPTIONS]\n\n"
          "OPTIONS\n"
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
          "  --version\n"
          "         Show version information\n\n";
        return EXIT_SUCCESS;
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

      default: {
        std::cout << "See --help.\n";
        return EXIT_FAILURE;
      }
    }
  }

  if (optind < argc) {
    printf( "%s: invalid options -- ", argv[0] );
    while (optind < argc) printf( "%s ", argv[optind++] );
    printf( "\n" );
    return EXIT_FAILURE;
  }

  std::cout << version << '\n' <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the command line client\n"
    "of piac. It needs to connect to a " + piac::daemon_executable() + " to "
    "work correctly. Type\n'help' to list the available commands.\n";

  setup_logging( logfile, log_level, /* console_logging = */ false,
                 max_log_file_size, max_log_files );

  std::string host = "localhost:55090";
  bool connected = false;

  // initialize the zmq context with a single IO thread
  zmqpp::context context;
  // construct a REQ (request) socket
  zmqpp::socket server{ context, zmqpp::socket_type::req };

  char* buf;
  std::string prompt = color_string( piac::cli_executable(),GREEN ) +
                       color_string( "> ",YELLOW );

  while ((buf = readline( prompt.c_str() ) ) != nullptr) {

    if (buf[0]=='c' && buf[1]=='o' && buf[2]=='n' && buf[3]=='n' &&
        buf[4]=='e' && buf[5]=='c' && buf[6]=='t')
    {
      std::string b( buf );
      b.erase( 0, 7 );
      if (not b.empty()) {
        std::stringstream ss( b );
        ss >> host;
      }
      connect( connected, server, host );

    } else if (buf[0]=='d' && buf[1]=='b') {

      send_cmd( connected, server, buf );

    } else  if (!strcmp(buf,"disconnect")) {

      disconnect( connected, server, host );

    } else if (!strcmp(buf,"exit") || !strcmp(buf,"quit") || buf[0]=='q') {

      free( buf );
      break;

    } else if (!strcmp(buf,"help")) {

      std::cout << "COMMANDS\n"

        "      connect [<host>[:<port>]]\n"
        "                Connect to a " + piac::daemon_executable() + "."
                        "The optional <host> argument specifies\n"
        "                a hostname or an IPv4 address in standard dot "
                         "notation. See\n"
        "                'man gethostbyname'. The optional <port> argument is "
                        "an\n"
        "                integer specifying a port.\n\n"
        "      db <command>\n"
        "                Send database command to " + piac::daemon_executable()
                         + ". You must have connected to\n"
        "                a " + piac::daemon_executable() + " first. See "
                        "'connect'.\n\n"
        "      disconnect\n"
	"                Disconnect from a " + piac::daemon_executable() + "\n\n"
        "      exit, quit, q\n"
        "                Exit\n\n"
        "      help\n"
        "                This help message\n\n"
        "      new\n"
        "                Create new user identity. This will generate a new "
                        "monero wallet\n"
        "                that will be used to identify you when creating an ad "
                        "or paying for\n"
        "                an item. This wallet can be used just like any other "
                        "monero wallet.\n\n"
        "      peers\n"
        "                List peers of " + piac::daemon_executable() + ". You "
                        "must have connected to a " + piac::daemon_executable()
                         + "\n"
        "                first. See 'connect'.\n\n"
        "      status\n"
	"                Query status\n\n"
        "      version\n"
	"                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"new")) {

      create_new_wallet();

    } else if (!strcmp(buf,"peers")) {

      send_cmd( connected, server, "peers" );

    } else if (!strcmp(buf,"status")) {

      status( connected );

    } else if (!strcmp(buf,"version")) {

      std::cout << version << '\n';

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    free( buf );
  }

  disconnect( connected, server, host );
  MDEBUG( "graceful exit" );

  return EXIT_SUCCESS;
}
