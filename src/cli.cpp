#include <zmqpp/zmqpp.hpp>
#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>

#include "logging.hpp"
#include "project_config.hpp"

// *****************************************************************************
void disconnect( bool& connected,
                 zmqpp::socket& server,
                 const std::string& host )
{
  NLOG(DEBUG) << "disconnect";
  if (connected) {
    server.disconnect( "tcp://" + host );
    NLOG(INFO) << "Disconnected";
  } else {
    NLOG(INFO) << "Not connected";
  }
  connected = false;
}

// *****************************************************************************
void connect( bool& connected, zmqpp::socket& server, const std::string& host )
{
  if (connected) {
    NLOG(INFO) << "You need to disconnect first";
    return;
  }
  NLOG(DEBUG) << "connect";
  NLOG(INFO) << "Connecting to " + piac::daemon_executable() << " at " << host;
  server.connect( "tcp://" + host );
  server.send( "connect" );
  // wait for reply from server
  std::string reply;
  auto res = server.receive( reply );
  NLOG(DEBUG) << reply;
  if (reply == "accept") {
    NLOG(INFO) << "Connected.";
    connected = true;
  }
}

// *****************************************************************************
void status( bool connected ) {
  NLOG(DEBUG) << "status";
  if (connected)
    NLOG(INFO) << "Connected";
  else
    NLOG(INFO) << "Not connected";
}

// *****************************************************************************
void send_cmd( bool connected, zmqpp::socket& server, const std::string& cmd ) {
  NLOG(DEBUG) << cmd;
  if (not connected) {
    NLOG(INFO) << "Not connected. Use 'connect' to connect to "
               << piac::daemon_executable() << '.';
    return;
  }
  // send message to server with command
  server.send( cmd );
  // wait for reply from server
  std::string reply;
  auto res = server.receive( reply );
  NLOG(INFO) << reply;
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
int main( int argc, char **argv ) {
  // Setup logging
  std::string logfile( piac::cli_executable() + ".log" );
  setup_logging( piac::cli_executable(), crash_handler, logfile,
                 /* file_logging = */ true, /* console_logging = */ true );

  std::string version( "piac: " + piac::cli_executable() + " v"
                       + piac::project_version() + "-"
                       + piac::build_type() );

  // Process command line arguments
  int c;
  int option_index = 0;
  static struct option long_options[] =
    { // NAME     ARGUMENT     FLAG SHORTNAME/FLAGVALUE
      {"help",    no_argument, 0,   'h'},
      {"version", no_argument, 0,   'v'},
      {0, 0, 0, 0}
    };
  while ((c = getopt_long(argc, argv, "hv",
                long_options, &option_index)) != -1)
  {
    switch (c) {
      case 'h':
        NLOG(DEBUG) << "help";
        NLOG(INFO) << "Usage: " + piac::cli_executable() + " [OPTIONS]\n\n"
                      "OPTIONS\n"
                      "  -h, --help\n"
                      "         Show help message.\n\n"
                      "  -v, --version\n"
                      "         Show version information\n";
        return EXIT_SUCCESS;

      case 'v':
        NLOG(DEBUG) << "version";
        NLOG(INFO) << version;
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

  NLOG(INFO) << version;
  std::cout <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the command line client\n"
    "of piac. It needs to connect to a " + piac::daemon_executable() + " to "
    "work correctly. Type\n'help' to list the available commands.\n";

  NLOG(INFO) << "Logging to " << logfile;

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
        "      peers\n"
        "                List peers of " + piac::daemon_executable() + ". You "
                        "must have connected to a " + piac::daemon_executable()
                         + "\n"
        "                first. See 'connect'.\n\n"
        "      status\n"
	"                Query status\n\n"
        "      version\n"
	"                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"peers")) {

      send_cmd( connected, server, "peers" );

    } else if (!strcmp(buf,"status")) {

      status( connected );

    } else if (!strcmp(buf,"version")) {

      NLOG(DEBUG) << "version";
      NLOG(INFO) << version;

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    free( buf );
  }

  disconnect( connected, server, host );
  NLOG(DEBUG) << "End " << piac::cli_executable() + '\n';

  return EXIT_SUCCESS;
}
