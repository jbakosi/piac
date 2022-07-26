#include <zmqpp/zmqpp.hpp>
#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include "wallet/monero_wallet_full.h"

#include "project_config.hpp"
#include "util.hpp"

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
  std::cout << "Connecting to '" + piac::daemon_executable() << "' at " << host
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
void send_cmd( bool connected,
               zmqpp::socket& server,
               std::string cmd,
               const std::unique_ptr< monero_wallet_full >& wallet )
{
  piac::trim( cmd );
  MDEBUG( cmd );
  if (not connected) {
    std::cout << "Not connected. Use 'connect' to connect to "
               << piac::daemon_executable() << ".\n";
    return;
  }

  // append author if cmd contains "db add/rm"
  auto npos = std::string::npos;
  if ( cmd.find("db") != npos &&
      (cmd.find("add") != npos || cmd.find("rm") != npos) )
  {
    if (not wallet) {
      std::cout << "Need active user id (wallet) to add to db. "
                   "See 'new' or 'user'.\n";
      return;
    }
    cmd += " AUTH:" + piac::sha256( wallet->get_primary_address() );
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
[[nodiscard]] std::unique_ptr< monero_wallet_full >
create_wallet() {
  MDEBUG( "new" );
  auto w = monero_wallet_full::create_wallet_random( "", "",
             monero_network_type::TESTNET );
  std::cout << "Mnemonic seed: " << w->get_mnemonic() << '\n' <<
    "This is your monero wallet mnemonic seed that identifies you within\n"
    "piac. You can use it to create your ads or pay for a product. This seed\n"
    "can be restored within your favorite monero wallet software and you can\n"
    "use this wallet just like any other monero wallet. Save this seed and\n"
    "keep it secret.\n";
  return std::unique_ptr< monero_wallet_full >( w );
}

// *****************************************************************************
void
show_wallet_keys( const std::unique_ptr< monero_wallet_full >& wallet ) {
  MDEBUG( "keys" );
  if (not wallet) {
    std::cout << "Need active user id (wallet). See 'new' or 'user'.\n";
    return;
  }
  std::cout << "Mnemonic seed: " << wallet->get_mnemonic() << '\n';
  std::cout << "Primary address: " << wallet->get_primary_address() << '\n';
  std::cout << "Secret view key: " << wallet->get_private_view_key() << '\n';
  std::cout << "Public view key: " << wallet->get_public_view_key() << '\n';
  std::cout << "Secret spend key: " << wallet->get_private_spend_key() << '\n';
  std::cout << "Public spend key: " << wallet->get_public_spend_key() << '\n';
}

// *****************************************************************************
[[nodiscard]] std::unique_ptr< monero_wallet_full >
switch_user( const std::string& mnemonic ) {
  MDEBUG( "user" );
  assert( not mnemonic.empty() );
  auto w = monero_wallet_full::create_wallet_from_mnemonic( "", "",
             monero_network_type::TESTNET, mnemonic );
  std::cout << "Switched to new user\n";
  return std::unique_ptr< monero_wallet_full >( w );
}

// *****************************************************************************
void show_user( const std::unique_ptr< monero_wallet_full >& wallet ) {
  MDEBUG( "user" );
  if (not wallet) {
    std::cout << "No active user id (wallet). See 'new' or 'user'.\n";
    return;
  }
  std::cout << "Mnemonic seed: " << wallet->get_mnemonic() << '\n';
}

// ****************************************************************************
[[nodiscard]] int wordcount( const std::string& s ) {
  auto str = s.data();
  if (str == NULL) return 0;
  bool inSpaces = true;
  int numWords = 0;
  while (*str != '\0') {
    if (std::isspace(*str)) {
      inSpaces = true;
    } else if (inSpaces) {
      numWords++;
      inSpaces = false;
    }
     ++str;
  }
  return numWords;
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

  piac::setup_logging( logfile, log_level, /* console_logging = */ false,
                       max_log_file_size, max_log_files );

  std::string host = "localhost:55090";
  bool connected = false;

  // monero wallet = user id
  std::unique_ptr< monero_wallet_full > wallet;

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
      b.erase( 0, 8 );
      if (not b.empty()) host = b;
      connect( connected, server, host );

    } else if (buf[0]=='d' && buf[1]=='b') {

      send_cmd( connected, server, buf, wallet );

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
                        "'connect'. Example db commands:\n"
        "                > db query cat - search for the word 'cat'\n"
        "                > db query race -condition - search for 'race' but "
                        "not 'condition'\n"
        "                > db add json <path-to-json-db-entry>\n"
        "                > db rm <hash> - remove document\n"
        "                > db list - list all documents\n"
        "                > db list hash - list all document hashes\n"
        "                > db list numdoc - list number of documents\n"
        "                > db list numusr - list number of users in db\n\n"
        "      disconnect\n"
	"                Disconnect from a " + piac::daemon_executable() + "\n\n"
        "      exit, quit, q\n"
        "                Exit\n\n"
        "      help\n"
        "                This help message\n\n"
        "      keys\n"
        "                Show monero wallet keys of current user\n\n"
        "      new\n"
        "                Create new user identity. This will generate a new "
                        "monero wallet\n"
        "                which will be used as a user id when creating an ad "
                        "or paying for\n"
        "                an item. This wallet can be used just like any other "
                        "monero wallet.\n"
        "                If you want to use your existing monero wallet, see "
                        "'user'.\n\n"
        "      peers\n"
        "                List peers of " + piac::daemon_executable() + ". You "
                        "must have connected to a " + piac::daemon_executable()
                         + "\n"
        "                first. See 'connect'.\n\n"
        "      status\n"
	"                Query status\n\n"
        "      user [<mnemonic>]\n"
	"                Show active monero wallet mnemonic seed (user id) if "
                        "no mnemonic is\n"
        "                given. Switch to mnemonic if given.\n\n"
        "      version\n"
	"                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"keys")) {

      show_wallet_keys( wallet );

    } else if (!strcmp(buf,"new")) {

      wallet = create_wallet();

    } else if (!strcmp(buf,"peers")) {

      send_cmd( connected, server, "peers", nullptr );

    } else if (!strcmp(buf,"status")) {

      status( connected );

    } else if (buf[0]=='u' && buf[1]=='s' && buf[2]=='e' && buf[3]=='r') {

      std::string mnemonic( buf );
      mnemonic.erase( 0, 5 );
      if (mnemonic.empty()) {
        show_user( wallet );
      } else if (wordcount(mnemonic) != 25) {
        std::cout << "Need 25 words\n";
      } else {
        wallet = switch_user( mnemonic );
      }

    } else if (!strcmp(buf,"version")) {

      std::cout << version << '\n';

    } else {

      std::cout << "unknown cmd\n";

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    free( buf );
  }

  if (connected) disconnect( connected, server, host );
  MDEBUG( "graceful exit" );

  return EXIT_SUCCESS;
}
