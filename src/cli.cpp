// *****************************************************************************
/*!
  \file      src/cli.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac command line interface
*/
// *****************************************************************************

#include <readline/history.h>
#include <readline/readline.h>
#include <getopt.h>

#include "macro.hpp"
#include "project_config.hpp"
#include "string_util.hpp"
#include "logging_util.hpp"
#include "zmq_util.hpp"
#include "monero_util.hpp"

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
         "  --version\n"
         "         Show version information\n\n";
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

  std::string host = "localhost:55090";
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Will connect to piac daemon at " << host << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  std::string monerod_host = "localhost:38089";
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Wallet connecting to monero daemon at " << monerod_host << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  // monero wallet = user id
  std::unique_ptr< monero_wallet_full > wallet;
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

  // initialize (thread-safe) zmq context
  zmqpp::context ctx;

  char* buf;
  std::string prompt = color_string( "piac", piac::GREEN ) +
                       color_string( "> ", piac::YELLOW );

  while ((buf = readline( prompt.c_str() ) ) != nullptr) {

    if (!strcmp(buf,"balance")) {

      piac::show_wallet_balance( wallet, listener );

    } else if (buf[0]=='d' && buf[1]=='b') {

      piac::send_cmd( buf, ctx, host, rpc_server_public_key, rpc_client_keys,
                      wallet );

    } else if (!strcmp(buf,"exit") || !strcmp(buf,"quit") || buf[0]=='q') {

      wallet.release();
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
      "      monerod [<host>[:<port>]]\n"
      "                Specify monero node to connect to. The <host> argument specifies\n"
      "                a hostname or an IPv4 address in standard dot notation. The\n"
      "                optional <port> argument is an integer specifying a port. The\n"
      "                default is " + monerod_host + ". Without an argument, show current\n"
      "                setting. Use "" to clear the setting and use an offline wallet.\n\n"
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
      "                default is " + host + ". The optional public-key is the\n"
      "                server's public key to use for authenticated and secure\n"
      "                connections. Without an argument, show current setting. Use "" to\n"
      "                clear the setting and use " + piac::cli_executable() + " offline.\n\n"
      "      user [<mnemonic>]\n"
      "                Show active monero wallet mnemonic seed (user id) if no mnemonic is\n"
      "                given. Switch to mnemonic if given.\n\n"
      "      version\n"
      "                Display " + piac::cli_executable() + " version\n";

    } else if (!strcmp(buf,"keys")) {

      piac::show_wallet_keys( wallet );

    } else if (buf[0]=='m' && buf[1]=='o' && buf[2]=='n' && buf[3]=='e'&&
               buf[4]=='r' && buf[5]=='o' && buf[6]=='d') {

      std::string b( buf );
      auto t = piac::tokenize( b );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      if (t.size() == 1) {
        if (monerod_host.empty())
          std::cout << "Wallet offline\n";
        else
          std::cout << "Wallet connecting to monero daemon at "
                    << monerod_host << '\n';
      } else {
        monerod_host = t[1];
        if (monerod_host == "\"\"") {
          monerod_host.clear();
          std::cout << "Offline\n";
        } else {
          std::cout << "Wallet connecting to monero daemon at "
                    << monerod_host << '\n';
        }
      }
      epee::set_console_color( epee::console_color_default, /*bright=*/ false );

    } else if (!strcmp(buf,"new")) {

      wallet = piac::create_wallet( monerod_host, listener );

    } else if (!strcmp(buf,"peers")) {

      piac::send_cmd( "peers", ctx, host, rpc_server_public_key,
                      rpc_client_keys, wallet );

    } else if (buf[0]=='s' && buf[1]=='e' && buf[2]=='r' && buf[3]=='v'&&
               buf[4]=='e' && buf[5]=='r') {

      std::string b( buf );
      auto t = piac::tokenize( b );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      if (t.size() == 1) {
        if (host.empty())
          std::cout << "Offline\n";
        else
          std::cout << "Will connect to piac daemon at " << host << '\n';
      } else {
        host = t[1];
        if (host == "\"\"") {
          host.clear();
          std::cout << "Offline\n";
        } else {
          std::cout << "Will connect to piac daemon at " << host;
          if (t.size() > 2) {
            std::cout << ", using public key: " << t[2];
            rpc_server_public_key = t[2];
          }
          std::cout << '\n';
        }
      }
      epee::set_console_color( epee::console_color_default, /*bright=*/ false );

    } else if (buf[0]=='u' && buf[1]=='s' && buf[2]=='e' && buf[3]=='r') {

      std::string mnemonic( buf );
      mnemonic.erase( 0, 5 );
      if (mnemonic.empty()) {
        piac::show_user( wallet );
      } else if (piac::wordcount(mnemonic) != 25) {
        std::cout << "Need 25 words\n";
      } else {
        wallet = piac::switch_user( mnemonic, monerod_host, listener );
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

  MDEBUG( "graceful exit" );

  return EXIT_SUCCESS;
}
