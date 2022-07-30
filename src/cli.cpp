#include <zmqpp/zmqpp.hpp>
#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>
#include <unistd.h>

#define MONERO

#ifdef MONERO
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include "wallet/monero_wallet_full.h"
#endif

#include "project_config.hpp"
#include "string_util.hpp"
#include "crypto_util.hpp"
#include "logging_util.hpp"

#define REQUEST_TIMEOUT     3000   //  msecs, (> 1000!)
#define REQUEST_RETRIES     5      //  before we abandon trying to send

// *****************************************************************************
zmqpp::socket
daemon_socket( zmqpp::context& ctx, const std::string& host ) {
  MINFO( "Connecting to " << host );
  auto sock = zmqpp::socket( ctx, zmqpp::socket_type::req );
  sock.connect( "tcp://" + host );
  //  configure socket to not wait at close time
  sock.set( zmqpp::socket_option::linger, 0 );
  return sock;
}

// *****************************************************************************
std::string
pirate_send( const std::string& cmd,
            zmqpp::context& ctx,
            const std::string& host )
{
  std::string reply;
  int retries = REQUEST_RETRIES;
  auto server = daemon_socket( ctx, host );

  // send cmd from a Lazy Pirate client
  while (retries) {
    std::string request = cmd;
    server.send( request );
    if (retries != REQUEST_RETRIES) sleep(1);

    zmqpp::poller poller;
    poller.add( server );
    bool expect_reply = true;
    while (expect_reply) {
      if (poller.poll(REQUEST_TIMEOUT) && poller.has_input(server)) {
        server.receive( reply );
        if (not reply.empty()) {
          MDEBUG( "Recv reply: " + reply );
          expect_reply = false;
          retries = 0;
        } else {
          MERROR( "Recv empty msg from daemon" );
        }
      } else if (--retries == 0) {
        MERROR( "Abandoning server at " + host );
        reply = "No response from server";
        expect_reply = false;
      } else {
        MWARNING( "No response from " + host + ", retrying: " << retries );
        poller.remove( server );
        server = daemon_socket( ctx, host );
        poller.add( server );
        server.send( request );
      }
    }
  }

  MDEBUG( "Destroyed socket to " + host );
  return reply;
}

// *****************************************************************************
void send_cmd( std::string cmd,
               zmqpp::context& ctx,
               const std::string& host
               #ifdef MONERO
             , const std::unique_ptr< monero_wallet_full >& wallet
               #endif
)
{
  piac::trim( cmd );
  MDEBUG( cmd );

  #ifdef MONERO
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
  #endif

  // send message to daemon with command
  auto reply = pirate_send( cmd, ctx, host );
  std::cout << reply << '\n';
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

#ifdef MONERO
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
#endif

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

  epee::set_console_color( epee::console_color_green, /* bright = */ false );
  std::cout << version << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );
  std::cout <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the command line client\n"
    "of piac. It needs to connect to a " + piac::daemon_executable() + " to "
    "work correctly. Type\n'help' to list the available commands.\n";

  piac::setup_logging( logfile, log_level, /* console_logging = */ false,
                       max_log_file_size, max_log_files );

  std::string host = "localhost:55090";
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Will send commands to daemon at " << host << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  #ifdef MONERO
  // monero wallet = user id
  std::unique_ptr< monero_wallet_full > wallet;
  #endif

  MLOG_SET_THREAD_NAME( "cli" );

  // initialize (thread-safe) zmq context
  zmqpp::context ctx;

  char* buf;
  std::string prompt = color_string( "piac", GREEN ) +
                       color_string( "> ",YELLOW );

  while ((buf = readline( prompt.c_str() ) ) != nullptr) {

    if (buf[0]=='s' && buf[1]=='e' && buf[2]=='r' && buf[3]=='v'&&
        buf[4]=='e' && buf[5]=='r')
    {

      std::string b( buf );
      b.erase( 0, 7 );
      if (not b.empty()) host = b;
      epee::set_console_color( epee::console_color_yellow,
                               /* bright = */ false );
      std::cout << "Will send commands to daemon at " << host << '\n';
      epee::set_console_color( epee::console_color_default,
                               /* bright = */ false );

    } else if (buf[0]=='d' && buf[1]=='b') {

      #ifdef MONERO
      send_cmd( buf, ctx, host, wallet );
      #else
      send_cmd( buf, ctx, host );
      #endif

    } else if (!strcmp(buf,"exit") || !strcmp(buf,"quit") || buf[0]=='q') {

      free( buf );
      break;

    } else if (!strcmp(buf,"help")) {

      std::cout << "COMMANDS\n"

        "      server <host>[:<port>]\n"
        "                Specify server to send commands to. The <host> "
                        "argument specifies\n"
        "                a hostname or an IPv4 address in standard dot "
                         "notation. See\n"
        "                'man gethostbyname'. The optional <port> argument is "
                        "an\n"
        "                integer specifying a port. The default is " + host +
                        ".\n\n"
        "      db <command>\n"
        "                Send database command to daemon. Example db commands:\n"
        "                > db query cat - search for the word 'cat'\n"
        "                > db query race -condition - search for 'race' but "
                        "not 'condition'\n"
        "                > db add json <path-to-json-db-entry>\n"
        "                > db rm <hash> - remove document\n"
        "                > db list - list all documents\n"
        "                > db list hash - list all document hashes\n"
        "                > db list numdoc - list number of documents\n"
        "                > db list numusr - list number of users in db\n\n"
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
        "                List server peers\n\n"
        "      user [<mnemonic>]\n"
	"                Show active monero wallet mnemonic seed (user id) if "
                        "no mnemonic is\n"
        "                given. Switch to mnemonic if given.\n\n"
        "      version\n"
	"                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"keys")) {

      #ifdef MONERO
      show_wallet_keys( wallet );
      #endif

    } else if (!strcmp(buf,"new")) {

      #ifdef MONERO
      wallet = create_wallet();
      #endif

    } else if (!strcmp(buf,"peers")) {

      #ifdef MONERO
      send_cmd( "peers", ctx, host, wallet );
      #else
      send_cmd( "peers", ctx, host );
      #endif

    } else if (buf[0]=='u' && buf[1]=='s' && buf[2]=='e' && buf[3]=='r') {

      #ifdef MONERO
      std::string mnemonic( buf );
      mnemonic.erase( 0, 5 );
      if (mnemonic.empty()) {
        show_user( wallet );
      } else if (wordcount(mnemonic) != 25) {
        std::cout << "Need 25 words\n";
      } else {
        wallet = switch_user( mnemonic );
      }
      #endif

    } else if (!strcmp(buf,"version")) {

      std::cout << version << '\n';

    } else {

      std::cout << "unknown cmd\n";

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    free( buf );
  }

  MDEBUG( "graceful exit" );

  return EXIT_SUCCESS;
}
