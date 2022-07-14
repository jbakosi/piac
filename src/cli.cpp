#include <zmqpp/zmqpp.hpp>
#include <readline/history.h>
#include <readline/readline.h>
#include <boost/program_options.hpp>

#include "logging.hpp"
#include "project_config.hpp"

// *****************************************************************************
void disconnect( bool& connected,
                 zmqpp::socket& socket,
                 const std::string& host )
{
  NLOG(DEBUG) << "disconnect";
  if (connected) {
    socket.disconnect( "tcp://" + host );
    NLOG(INFO) << "Disconnected";
  } else {
    NLOG(INFO) << "Not connected";
  }
  connected = false;
}

// *****************************************************************************
void connect( bool& connected, zmqpp::socket& socket, const std::string& host )
{
  if (connected) {
    NLOG(INFO) << "You need to disconnect first";
    return;
  }
  NLOG(DEBUG) << "connect";
  NLOG(INFO) << "Connecting to " + piac::daemon_executable() << " at " << host;
  socket.connect( "tcp://" + host );
  socket.send( "connect" );
  // wait for reply from server
  std::string reply;
  auto res = socket.receive( reply );
  NLOG(DEBUG) << reply;
  if (reply == "accept") {
    NLOG(INFO) << "Connected.";
    connected = true;
  }
}

// *****************************************************************************
void status( bool connected )
{
  NLOG(DEBUG) << "status";
  if (connected)
    NLOG(INFO) << "Connected";
  else
    NLOG(INFO) << "Not connected";
}

// *****************************************************************************
void db_cmd( bool connected, zmqpp::socket& socket, const std::string& cmd )
{
  NLOG(DEBUG) << cmd;
  if (not connected) {
    NLOG(INFO) << "Not connected. Use 'connect' to connect to "
               << piac::daemon_executable() << '.';
    return;
  }

  // send message with db command
  socket.send( cmd );
  // wait for reply from server
  std::string reply;
  auto res = socket.receive( reply );
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

  // Supported command line arguments
  namespace po = boost::program_options;
  po::options_description desc( "Options" );
  desc.add_options()
    ("help", "Show help message")
    ("version", "Show version information")
  ;

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).
              options(desc).positional({}).run(),
              vm);
  } catch (po::too_many_positional_options_error &e) {
    NLOG(ERROR) << "Command line only accepts options with '--': " << e.what();
    return EXIT_FAILURE;
  } catch (po::error_with_option_name &e) {
    NLOG(ERROR) << "Command line: " << e.what();
    return EXIT_FAILURE;
  }
  po::notify( vm );

  if (vm.count( "help" )) {

    NLOG(DEBUG) << "help";
    NLOG(INFO) << "Usage: " + piac::cli_executable() + " [OPTIONS]\n";

    std::stringstream ss;
    ss << desc;
    NLOG(INFO) << ss.str();
    return EXIT_SUCCESS;

  } else if (vm.count( "version" )) {

    NLOG(DEBUG) << "version";
    NLOG(INFO) << version;
    return EXIT_SUCCESS;

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
  // construct a REQ (request) socket and connect to interface
  zmqpp::socket socket{ context, zmqpp::socket_type::req };

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
        std::cout << host << '\n';
      }
      connect( connected, socket, host );

    } else  if (!strcmp(buf,"disconnect")) {

      disconnect( connected, socket, host );

    } else if (buf[0]=='d' && buf[1]=='b') {

      db_cmd( connected, socket, buf );

    } else if (!strcmp(buf,"status")) {

      status( connected );

    } else if (!strcmp(buf,"version")) {

      NLOG(DEBUG) << "version";
      NLOG(INFO) << version;

    } else if (!strcmp(buf,"help")) {

      std::cout << "Commands:\n"

        "      help\n"
        "                This help message\n\n"

        "      exit\n"
        "                Exit\n\n"

        "      connect [<host>[:<port>]]\n"
        "                Connect to a " + piac::daemon_executable() + "."
                        "The optional <host> argument specifies\n"
        "                a hostname or an IPv4 address in standard dot "
                         "notation. See\n"
        "                'man gethostbyname'. The optional <port> argument is "
                        "an\n"
        "                integer specifying a port.\n\n"

        "      disconnect\n"
	"                Disconnect from a " + piac::daemon_executable() + "\n\n"

        "      db <command>\n"
        "                Send database command to " + piac::daemon_executable()
                         + ". You must have connected to\n"
        "                a " + piac::daemon_executable() + " first. See "
                        "'connect'.\n\n"

        "      status\n"
	"                Query status\n\n"

        "      version\n"
	"                Display neroshop-cli version\n";

    } else if (!strcmp(buf,"exit") && !strcmp(buf,"quit")) {

      free( buf );
      break;

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    free( buf );
  }

  disconnect( connected, socket, host );
  NLOG(DEBUG) << "End " << piac::cli_executable() + '\n';

  return EXIT_SUCCESS;
}
