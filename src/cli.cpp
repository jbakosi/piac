// *****************************************************************************
/*!
  \file      src/cli.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac command line interface
*/
// *****************************************************************************

#include <thread>

#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>

#include "macro.hpp"
#include "project_config.hpp"
#include "string_util.hpp"
#include "logging_util.hpp"
#include "zmq_util.hpp"
#include "monero_util.hpp"
#include "cli_matrix_thread.hpp"
#include "cli_message_thread.hpp"

static std::unique_ptr< monero_wallet_full > g_wallet;
static std::vector< std::thread > g_threads;

static void graceful_exit() {
  MDEBUG( "graceful exit" );
  g_wallet.release();
  if (piac::g_matrix_connected) {
    piac::g_matrix_shutdown = true;
    std::cout << "Waiting for matrix thread to quit ...\n";
    for (auto& t : g_threads) t.join();
    g_threads.clear();
  }
}

[[noreturn]] static void s_signal_handler( int /*signal_value*/ ) {
  MDEBUG( "interrupted" );
  graceful_exit();
  exit( EXIT_SUCCESS );
}

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif

static void s_catch_signals() {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset( &action.sa_mask );
  sigaction( SIGINT, &action, nullptr );
  sigaction( SIGTERM, &action, nullptr );
}

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

//! Piac declarations and definitions
namespace piac {

enum COLOR { RED, GREEN, BLUE, GRAY, YELLOW };

static std::string
color_string( const std::string &s, COLOR color = GRAY )
// *****************************************************************************
//! Insert ASCI color string code to a string
//! \param[in] s String to append
//! \param[in] color Color code to insert
//! \return String postfixed with ASCII color code
// *****************************************************************************
{
  std::string ret;
  if (color == RED) ret = "\033[0;31m";
  if (color == GREEN) ret = "\033[0;32m";
  if (color == BLUE) ret = "\033[0;34m";
  if (color == GRAY) ret = "\033[0;37m";
  if (color == YELLOW) ret = "\033[0;33m";
  return ret + s + "\033[0m";
}

static void
load_key( const std::string& filename, std::string& key )
// *****************************************************************************
//! Load keys from file
//! \param[in] filename File to load keys from
//! \param[in] key String to store key in
// *****************************************************************************
{
  if (filename.empty() || not key.empty()) return;
  std::ifstream f( filename );
  if (not f.good()) {
    epee::set_console_color( epee::console_color_red, /*bright=*/ false );
    std::cerr <<  "Cannot open file for reading: " << filename << '\n';
    epee::set_console_color( epee::console_color_default, /*bright=*/ false );
    exit( EXIT_FAILURE );
  }
  f >> key;
  assert( key.size() == 40 );
  f.close();
  MINFO( "Read key from file " << filename );
}

static std::string
usage( const std::string& logfile )
// *****************************************************************************
//! Return program usage information
//! \param[in] logfile Logfile name
//! \return String containing usage information
// *****************************************************************************
{
  return "Usage: " + piac::cli_executable() + " [OPTIONS]\n\n"
         "OPTIONS\n"
         "  --help\n"
         "         Show help message.\n\n"
         "  --log-file <filename.log>\n"
         "         Specify log filename, default: " + logfile + ".\n\n"
         "  --log-level <[0-4]>\n"
         "         Specify log level: 0: minimum, 4: maximum.\n\n"
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
         "  --rpc-secure\n"
         "         Enable secure connection to server.\n\n"
         "  --rpc-client-public-key-file <filename>\n"
         "         Load client public key from file. Need to also set "
                  "--rpc-secure.\n\n"
         "  --rpc-client-secret-key-file <filename>\n"
         "         Load client secret key from file. Need to also set "
                  "--rpc-secure.\n\n"
         "  --rpc-server-public-key <key>\n"
         "         Specify server public key. Need to also set "
                  "--rpc-secure.\n\n"
         "  --rpc-server-public-key-file <filename>\n"
         "         Load server public key from file. Neet to also set "
                  "--rpc-secure. If given,\n"
         "         --rpc-server-public-key takes precedence.\n\n"
         "  --matrix-sync-timeout\n"
         "         Timeout in milliseconds for matrix sync calls. The default "
                 "is " + std::to_string(piac::g_matrix_sync_timeout) + ".\n\n"
         "  --version\n"
         "         Show version information.\n\n";
}

static void
echo_connection( const std::string& info, const std::string& server )
// *****************************************************************************
//!  Echo connection information to screen
//! \param[in] info Introductory info message about connection
//! \param[in] server Hostname that will be addressed
// *****************************************************************************
{
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << info;
  epee::set_console_color( epee::console_color_white, /* bright = */ false );
  std::cout << server << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );
}

} // piac::

int
main( int argc, char **argv )
// *****************************************************************************
//! Piac command line interface main function
//! \param[in] argc Number of command line arguments passed from shell
//! \param[in] argv List of command line arguments passed from shell
//! \return Error code to return to shell
// *****************************************************************************
{
  // save command line
  std::vector< std::string > args( argv, argv+argc );
  std::stringstream cmdline;
  for (const auto& a : args) cmdline << a << ' ';

  std::string logfile( piac::cli_executable() + ".log" );
  std::string log_level( "4" );
  std::string version( "piac: " + piac::cli_executable() + " v"
                       + piac::project_version() + "-"
                       + piac::build_type() );
  std::size_t max_log_file_size = MAX_LOG_FILE_SIZE;
  std::size_t max_log_files = MAX_LOG_FILES;

  std::string rpc_client_public_key_file;
  std::string rpc_client_secret_key_file;
  std::string rpc_server_public_key;
  std::string rpc_server_public_key_file;

  // Process command line arguments
  int c;
  int option_index = 0;
  int rpc_secure = 0;
  int num_err = 0;
  const int ARG_HELP                       = 1000;
  const int ARG_LOG_FILE                   = 1001;
  const int ARG_LOG_LEVEL                  = 1002;
  const int ARG_MAX_LOG_FILE_SIZE          = 1003;
  const int ARG_MAX_LOG_FILES              = 1004;
  const int ARG_VERSION                    = 1005;
  const int ARG_RPC_CLIENT_PUBLIC_KEY_FILE = 1006;
  const int ARG_RPC_CLIENT_SECRET_KEY_FILE = 1007;
  const int ARG_RPC_SERVER_PUBLIC_KEY      = 1008;
  const int ARG_RPC_SERVER_PUBLIC_KEY_FILE = 1009;
  const int ARG_MATRIX_SYNC_TIMEOUT        = 1010;
  static struct option long_options[] =
    {
      { "help", no_argument, nullptr, ARG_HELP },
      { "log-file", required_argument, nullptr, ARG_LOG_FILE },
      { "log-level", required_argument, nullptr, ARG_LOG_LEVEL },
      { "max-log-file-size", required_argument, nullptr, ARG_MAX_LOG_FILE_SIZE },
      { "max-log-files", required_argument, nullptr, ARG_MAX_LOG_FILES },
      { "rpc-secure", no_argument, &rpc_secure, 1 },
      { "rpc-client-public-key-file", required_argument, nullptr,
        ARG_RPC_CLIENT_PUBLIC_KEY_FILE},
      { "rpc-client-secret-key-file", required_argument, nullptr,
        ARG_RPC_CLIENT_SECRET_KEY_FILE},
      { "rpc-server-public-key", required_argument, nullptr,
        ARG_RPC_SERVER_PUBLIC_KEY},
      { "rpc-server-public-key-file",required_argument, nullptr,
        ARG_RPC_SERVER_PUBLIC_KEY_FILE},
      { "matrix-sync-timeout",required_argument, nullptr,
        ARG_MATRIX_SYNC_TIMEOUT},
      { "version", no_argument, nullptr, ARG_VERSION },
      { nullptr, 0, nullptr, 0 }
    };
  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
      case ARG_HELP: {
        std::cout << version << "\n\n" << piac::usage( logfile );
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

      case ARG_RPC_CLIENT_PUBLIC_KEY_FILE: {
        rpc_client_public_key_file = optarg;
        break;
      }

      case ARG_RPC_CLIENT_SECRET_KEY_FILE: {
        rpc_client_secret_key_file = optarg;
        break;
      }

      case ARG_RPC_SERVER_PUBLIC_KEY: {
        rpc_server_public_key = optarg;
        break;
      }

      case ARG_RPC_SERVER_PUBLIC_KEY_FILE: {
        rpc_server_public_key_file = optarg;
        break;
      }

      case ARG_MATRIX_SYNC_TIMEOUT: {
        std::stringstream s;
        s << optarg;
        s >> piac::g_matrix_sync_timeout;
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
              << piac::usage( logfile );
    return EXIT_FAILURE;
  }

  if ((not rpc_client_public_key_file.empty() ||
       not rpc_client_secret_key_file.empty() ||
       not rpc_server_public_key.empty() ||
       not rpc_server_public_key_file.empty()) &&
       not rpc_secure)
  {
    std::cerr << "Need --rpc-secure to secure RPC channel.\n";
    return EXIT_FAILURE;
  }

  if (rpc_secure &&
      rpc_server_public_key.empty() &&
      rpc_server_public_key_file.empty())
  {
    std::cerr << "Need --rpc-server-public-key or --rpc-server-public-key-file "
                 "to secure RPC channel.\n";
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

  MINFO( "Command line: " + cmdline.str() );

  std::string piac_host = "localhost:55090";
  std::string monerod_host = "localhost:38089";

  piac::echo_connection( "Will connect to piac daemon at ", piac_host );
  piac::echo_connection( "Wallet connecting to monero daemon at ",
                         monerod_host );

  piac::WalletListener listener;

  MLOG_SET_THREAD_NAME( "cli" );

  // setup RPC security
  int rpc_ironhouse = 1;
  zmqpp::curve::keypair rpc_client_keys;
  if (rpc_secure) {
    piac::load_key( rpc_server_public_key_file, rpc_server_public_key );
    assert( not rpc_server_public_key.empty() );
    assert( rpc_server_public_key.size() == 40 );
    piac::load_key( rpc_client_public_key_file, rpc_client_keys.public_key );
    piac::load_key( rpc_client_secret_key_file, rpc_client_keys.secret_key );
    // fallback to stonehouse if needed
    if (rpc_client_keys.secret_key.empty() ||
        rpc_client_keys.public_key.empty())
    {
      rpc_ironhouse = 0;
      rpc_client_keys = zmqpp::curve::generate_keypair();
    }
  }

  // echo RPC security configured
  if (rpc_secure) {
    if (rpc_ironhouse) {  // ironhouse
      epee::set_console_color( epee::console_color_green, /*bright=*/ false );
      std::string ironhouse( "Connection to piac daemon is secure and "
        "authenticated." );
      std::cout << ironhouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( ironhouse );
    } else {              // stonehouse
      std::string stonehouse( "Connection to piac daemon is secure but not "
        "authenticated." );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      std::cout << stonehouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( stonehouse );
    }
  } else {                // grasslands
    epee::set_console_color( epee::console_color_red, /* bright = */ false );
    std::string grasslands( "WARNING: Connection to piac daemon is not secure" );
    std::cout << grasslands << '\n';
    MWARNING( grasslands );
  }
  epee::set_console_color( epee::console_color_default, /* bright = */false );
  MINFO( "RPC client public key: " << rpc_client_keys.public_key );

  std::string matrix_host, matrix_user, matrix_password;

  // initialize (thread-safe) zmq context used to communicate with daemon
  zmqpp::context ctx_rpc;

  char* buf;
  std::string prompt = color_string( "piac", piac::GREEN ) +
                       color_string( "> ", piac::YELLOW );

  s_catch_signals();

  while ((buf = readline( prompt.c_str() ) ) != nullptr) {

    if (!strcmp(buf,"balance")) {

      piac::show_wallet_balance( g_wallet, listener );

    } else if (buf[0]=='d' && buf[1]=='b') {

      piac::send_cmd( buf, ctx_rpc, piac_host, rpc_server_public_key,
                      rpc_client_keys, g_wallet );

    } else if (!strcmp(buf,"exit") || !strcmp(buf,"quit") || buf[0]=='q') {

      break;

    } else if (!strcmp(buf,"help")) {

      std::cout << "COMMANDS\n"
      "      balance\n"
      "                Show monero wallet balance and sync status\n\n"
      "      db <command>\n"
      "                Send database command to piac daemon. Example db commands:\n"
      "                > db query cat - search for the word 'cat'\n"
      "                > db query race -condition - search for 'race' but not 'condition'\n"
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
      "      matrix <host>[:<port>] <username> <password>]\n"
      "                Specify matrix server login information to connect to. The <host>\n"
      "                argument specifies a hostname or an IPv4 address in standard dot\n"
      "                notation. The optional <port> argument is an integer specifying a\n"
      "                port. If unspecified, the default port is 443. The <host>[:<port>]\n"
      "                argument must be followed by <username> and <password>. Use\n"
      "                'matrix \"\"' to clear the setting and disable communication via the\n"
      "                built-in matrix client. Note that connecting to a matrix server also\n"
      "                requires an active user id, see also 'new' or 'user'.\n\n"
      "      msg <user> <message>\n"
      "                Send a message to user, where <user> is a matrix user id and message\n"
      "                is either a single word or a doubly-quoted multi-word message.\n\n"
      "      monerod [<host>[:<port>]]\n"
      "                Specify monero node to connect to. The <host> argument specifies\n"
      "                a hostname or an IPv4 address in standard dot notation. The\n"
      "                optional <port> argument is an integer specifying a port. The\n"
      "                default is " + monerod_host + ". Without an argument, show current\n"
      "                setting. Use 'monerod \"\"' to clear the setting and use an\n"
      "                offline wallet.\n\n"
      "      new\n"
      "                Create new user identity. This will generate a new monero wallet\n"
      "                which will be used as a user id when creating an ad or paying for\n"
      "                an item. This wallet can be used just like any other monero wallet.\n"
      "                If you want to use your existing monero wallet, see 'user'.\n\n"
      "      peers\n"
      "                List server peers\n\n"
      "      server [<host>[:<port>]] [<public-key>]\n"
      "                Specify server to send commands to. The <host> argument specifies\n"
      "                a hostname or an IPv4 address in standard dot notation. The\n"
      "                optional <port> argument is an integer specifying a port. The\n"
      "                default is " + piac_host + ". The optional public-key is the\n"
      "                server's public key to use for authenticated and secure\n"
      "                connections. Without an argument, show current setting. Use\n"
      "                'server \"\"' to clear the setting and to not communicate with\n"
      "                the peer-to-peer piac network.\n\n"
      "      user [<mnemonic>]\n"
      "                Show active monero wallet mnemonic seed (user id) if no mnemonic is\n"
      "                given. Switch to mnemonic if given.\n\n"
      "      version\n"
      "                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"keys")) {

      piac::show_wallet_keys( g_wallet );

    } else if (buf[0]=='m' && buf[1]=='s' && buf[2]=='g') {

      if (not g_wallet) {
        std::cerr << "Need active user id (wallet). See 'new' or 'user'.\n";
      }
      std::string b( buf );
      auto t = piac::tokenize( b );
      if (t.size() == 3) {
        auto other_user = t[1];
        auto msg = t[2];
        piac::matrix_message( other_user, "ad", msg );
      } else {
        std::cerr << "Need exactly 3 tokens.\n";
      }

    } else if (buf[0]=='m' && buf[1]=='a' && buf[2]=='t' && buf[3]=='r' &&
               buf[4]=='i' && buf[5]=='x') {

      if (not g_wallet) {
        std::cerr << "Need active user id (wallet). See 'new' or 'user'.\n";
      }
      std::string b( buf );
      auto t = piac::tokenize( b );
      if (t.size() == 1) {
        if (piac::g_matrix_connected) {
          piac::echo_connection( "Connected to matrix server as ",
                                 '@' + matrix_user + ':' + matrix_host );
        } else {
          std::cout << "Offline\n";
        }
      } else if (t.size() == 2) {
        matrix_host = t[1];
        if (matrix_host == "disconnect") {
          if (piac::g_matrix_connected) {
            piac::g_matrix_shutdown = true;
            std::cout << "Waiting for matrix thread to quit...\n";
            for (auto& h : g_threads) h.join();
            g_threads.clear();
            std::cout << "Disconnected\n";
          }
        } else if (matrix_host == "\"\"") {
          matrix_host.clear();
          std::cout << "Offline\n";
        }
      } else if (t.size() == 4) {
        matrix_host = t[1];
        matrix_user = t[2];
        matrix_password = t[3];
        piac::echo_connection( "Connecting to matrix server as ",
                               '@' + matrix_user + ':' + matrix_host );
        g_threads.emplace_back( piac::matrix_thread, matrix_host, matrix_user,
          matrix_password, g_wallet->get_private_spend_key() );
        g_threads.emplace_back( piac::message_thread );
      } else {
        std::cout << "The command 'matrix' must be followed by 3 arguments "
                     "separated by spaces. See 'help'.\n";
      }

    } else if (buf[0]=='m' && buf[1]=='o' && buf[2]=='n' && buf[3]=='e'&&
               buf[4]=='r' && buf[5]=='o' && buf[6]=='d') {

      std::string b( buf );
      auto t = piac::tokenize( b );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      if (t.size() == 1) {
        if (monerod_host.empty()) {
          std::cout << "Wallet offline\n";
        } else {
          piac::echo_connection( "Wallet connecting to monero daemon at ",
                                 monerod_host );
        }
      } else {
        monerod_host = t[1];
        if (monerod_host == "\"\"") {
          monerod_host.clear();
          std::cout << "Wallet will not connect to monero daemon\n";
        } else {
          piac::echo_connection( "Wallet connecting to monero daemon at ",
                                 monerod_host );
        }
      }
      epee::set_console_color( epee::console_color_default, /*bright=*/ false );

    } else if (!strcmp(buf,"new")) {

      g_wallet = piac::create_wallet( monerod_host, listener );

    } else if (!strcmp(buf,"peers")) {

      piac::send_cmd( "peers", ctx_rpc, piac_host, rpc_server_public_key,
                      rpc_client_keys, g_wallet );

    } else if (buf[0]=='s' && buf[1]=='e' && buf[2]=='r' && buf[3]=='v'&&
               buf[4]=='e' && buf[5]=='r') {

      std::string b( buf );
      auto t = piac::tokenize( b );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      if (t.size() == 1) {
        if (piac_host.empty()) {
          std::cout << "Offline\n";
        } else {
         piac::echo_connection( "Will connect to piac daemon at ", piac_host );
        }
      } else {
        piac_host = t[1];
        if (piac_host == "\"\"") {
          piac_host.clear();
          std::cout << "Offline\n";
        } else {
          piac::echo_connection( "Will connect to piac daemon at ", piac_host );
          if (t.size() > 2) {
            std::cout << ", using public key: " << t[2];
            rpc_server_public_key = t[2];
          }
          std::cout << '\n';
        }
      }
      epee::set_console_color( epee::console_color_default, /*bright=*/ false );

    } else if (buf[0]=='s' && buf[1]=='l' && buf[2]=='e' && buf[3]=='e' &&
               buf[4]=='p')
    {

      std::string sec( buf );
      sec.erase( 0, 6 );

      auto start = std::chrono::high_resolution_clock::now();
      std::this_thread::sleep_for( std::chrono::seconds( std::atoi(sec.c_str()) ) );
      auto end = std::chrono::high_resolution_clock::now();
      std::chrono::duration< double, std::milli > elapsed = end - start;
      MDEBUG( "Waited " << elapsed.count() << " ms" );

    } else if (buf[0]=='u' && buf[1]=='s' && buf[2]=='e' && buf[3]=='r') {

      std::string mnemonic( buf );
      mnemonic.erase( 0, 5 );
      if (mnemonic.empty()) {
        piac::show_user( g_wallet );
      } else if (piac::wordcount(mnemonic) != 25) {
        std::cout << "Need 25 words\n";
      } else {
        g_wallet = piac::switch_user( mnemonic, monerod_host, listener );
      }

    } else if (!strcmp(buf,"version")) {

      std::cout << version << '\n';

    } else {

      std::cout << "unknown cmd\n";

    }

    if (strlen( buf ) > 0) add_history( buf );

    // readline malloc's a new buffer every time
    if (buf) free( buf );
  }

  graceful_exit();
  return EXIT_SUCCESS;
}
