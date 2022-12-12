// *****************************************************************************
/*!
  \file      src/daemon.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac daemon
*/
// *****************************************************************************

#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <getopt.h>
#include <unistd.h>

#include "macro.hpp"
#include "project_config.hpp"
#include "logging_util.hpp"
#include "daemon_p2p_thread.hpp"
#include "daemon_db_thread.hpp"

[[noreturn]] static void s_signal_handler( int /*signal_value*/ ) {
  MDEBUG( "interrupted" );
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

namespace piac {

std::mutex g_hashes_mtx;
std::condition_variable g_hashes_cv;
bool g_hashes_access = false;

static void
save_public_key( const std::string& filename, const std::string& public_key )
// *****************************************************************************
//! Save generated public key to file
//! \param[in] filename File to save keys to
//! \param[in] public_key Public key to save
// *****************************************************************************
{
  if (filename.empty()) return;
  std::ofstream f( filename );
  if (f.is_open()) {
    f << public_key;
    f.close();
    MINFO( "Public key saved to file: " << filename );
  } else {
    std::cerr << "Cannot open file for writing: " << filename << '\n';
  }
}

static void
load_server_key( const std::string& filename, std::string& key )
// *****************************************************************************
//! Load server key from file
//! \param[in] filename File to load key from
//! \param[in] key String to store key in
// *****************************************************************************
{
  if (filename.empty()) return;
  std::ifstream f( filename );
  if (not f.good()) {
    epee::set_console_color( epee::console_color_red, /*bright=*/ false );
    std::cerr <<  "Cannot open file for reading: " << filename << '\n';
    epee::set_console_color( epee::console_color_default, /*bright=*/ false );
    return;
  }
  f >> key;
  assert( key.size() == 40 );
  f.close();
  MINFO( "Read key from file " + filename );
}

static void
load_client_keys( const std::string& filename,
                  std::vector< std::string >& keys )
// *****************************************************************************
//! Load keys from file
//! \param[in] filename File to load keys from
//! \param[in] keys Vector of strings to store key in
// *****************************************************************************
{
  if (filename.empty()) return;
  std::ifstream f( filename );
  if (not f.good()) {
    epee::set_console_color( epee::console_color_red, /*bright=*/ false );
    std::cerr <<  "Cannot open file for reading: " << filename << '\n';
    epee::set_console_color( epee::console_color_default, /*bright=*/ false );
    exit( EXIT_FAILURE );
  }
  while (not f.eof()) {
    std::string key;
    f >> key;
    if (key.size() != 40) continue;
    keys.emplace_back( std::move(key) );
  }
  f.close();
  MINFO("Read " << keys.size() << " client keys from file " + filename);
}

static std::string
usage( const std::string& db_name,
       const std::string& logfile,
       const std::string& rpc_server_save_public_key_file,
       int rpc_port,
       int p2p_port )
// *****************************************************************************
//! Return program usage information
//! \param[in] db_name Name of database to use to store ads
//! \param[in] rpc_server_save_public_key_file File to save generated public key
//! \param[in] rpc_port Port to use for client communication
//! \param[in] p2p_port Port to use for peer-to-peer communication
//! \param[in] logfile Logfile name
//! \return String containing usage information
// *****************************************************************************
{
  return "Usage: " + piac::daemon_executable() + " [OPTIONS]\n\n"
          "OPTIONS\n"
          "  --db <directory>\n"
          "         Use database, default: " + db_name + ".\n\n"
          "  --detach\n"
          "         Run as a daemon in the background.\n\n"
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
          "  --peer <hostname>[:port]\n"
          "         Specify a peer to connect to.\n\n"
          "  --rpc-bind-port <port>\n"
          "         Listen on RPC port given, default: "
                  + std::to_string( rpc_port ) + ".\n\n"
          "  --rpc-secure\n"
          "         Enable secure connection to clients.\n\n"
          "  --rpc-server-public-key-file <filename>\n"
          "         Load server public key from file. Need to also set "
                   "--rpc-secure.\n\n"
          "  --rpc-server-secret-key-file <filename>\n"
          "         Load server secret key from file. Need to also set "
                   "--rpc-secure.\n\n"
          "  --rpc-authorized-clients-file <filename>\n"
          "         Only allow client connections with public keys read "
                   "from this file. Need to also\n"
          "         set --rpc-secure.\n\n"
          "  --rpc-server-save-public-key-file <filename>\n"
          "         Save self-generated server public key to file. Default: "
                  + rpc_server_save_public_key_file + ". Need to\n"
          "         also set --rpc-secure.\n\n"
          "  --p2p-bind-port <port>\n"
          "         Listen on P2P port given, default: "
                  + std::to_string( p2p_port ) + ".\n\n"
          "  --version\n"
          "         Show version information.\n\n";
}

} // piac::

int
main( int argc, char **argv )
// *****************************************************************************
//! Piac daemon main function
//! \param[in] argc Number of command line arguments passed from shell
//! \param[in] argv List of command line arguments passed from shell
//! \return Error code to return to shell
// *****************************************************************************
{
  // save command line
  std::vector< std::string > args( argv, argv+argc );
  std::stringstream cmdline;
  for (const auto& a : args) cmdline << a << ' ';

  // defaults
  int rpc_port = 55090;         // for client/server communication
  int default_p2p_port = 65090; // for peer-to-peer communication
  int p2p_port = default_p2p_port;
  bool use_strict_ports = false;
  std::string db_name( "piac.db" );
  std::string logfile( piac::daemon_executable() + ".log" );
  std::string log_level( "4" );
  std::size_t max_log_file_size = MAX_LOG_FILE_SIZE;
  std::size_t max_log_files = MAX_LOG_FILES;
  std::string version( "piac: " + piac::daemon_executable() + " v"
                       + piac::project_version() + "-" + piac::build_type() );
  std::vector< std::string > peers;
  std::string rpc_server_public_key_file;
  std::string rpc_server_secret_key_file;
  std::string rpc_authorized_clients_file;
  std::vector< std::string > rpc_authorized_clients;
  std::string rpc_server_save_public_key_file = "stonehouse.pub";

  // Process command line arguments
  int c;
  int option_index = 0;
  int detach = 0;
  int rpc_secure = 0;
  int num_err = 0;
  const int ARG_DB                              = 1000;
  const int ARG_HELP                            = 1001;
  const int ARG_LOG_FILE                        = 1002;
  const int ARG_LOG_LEVEL                       = 1003;
  const int ARG_MAX_LOG_FILE_SIZE               = 1004;
  const int ARG_MAX_LOG_FILES                   = 1005;
  const int ARG_PEER                            = 1006;
  const int ARG_RPC_PORT                        = 1007;
  const int ARG_RPC_SERVER_PUBLIC_KEY_FILE      = 1008;
  const int ARG_RPC_SERVER_SECRET_KEY_FILE      = 1009;
  const int ARG_RPC_AUTHORIZED_CLIENTS_FILE     = 1010;
  const int ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE = 1011;
  const int ARG_P2P_PORT                        = 1012;
  const int ARG_VERSION                         = 1013;
  static struct option long_options[] =
    {
      { "db", required_argument, nullptr, ARG_DB },
      { "detach", no_argument, &detach, 1 },
      { "help", no_argument, nullptr, ARG_HELP },
      { "log-file", required_argument, nullptr, ARG_LOG_FILE },
      { "log-level", required_argument, nullptr, ARG_LOG_LEVEL },
      { "max-log-file-size", required_argument, nullptr, ARG_MAX_LOG_FILE_SIZE },
      { "max-log-files", required_argument, nullptr, ARG_MAX_LOG_FILES },
      { "peer", required_argument, nullptr, ARG_PEER },
      { "rpc-bind-port", required_argument, nullptr, ARG_RPC_PORT },
      { "rpc-secure", no_argument, &rpc_secure, 1 },
      { "rpc-server-public-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_PUBLIC_KEY_FILE },
      { "rpc-server-secret-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_SECRET_KEY_FILE },
      { "rpc-authorized-clients-file", required_argument, nullptr,
        ARG_RPC_AUTHORIZED_CLIENTS_FILE },
      { "rpc-server-save-public-key-file", required_argument, nullptr,
        ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE },
      { "p2p-bind-port", required_argument, nullptr, ARG_P2P_PORT },
      { "version", no_argument, nullptr, ARG_VERSION },
      { nullptr, 0, nullptr, 0 }
    };

  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {

      case ARG_DB: {
        db_name = optarg;
        break;
      }

      case ARG_HELP: {
        std::cout << version << "\n\n" <<
          piac::usage( db_name, logfile, rpc_server_save_public_key_file,
                       rpc_port, p2p_port );
        return EXIT_SUCCESS;
      }

      case ARG_PEER: {
        peers.push_back( optarg );
        break;
      }

      case ARG_RPC_PORT: {
        rpc_port = atoi( optarg );
        use_strict_ports = true;
        break;
      }

      case ARG_RPC_SERVER_PUBLIC_KEY_FILE: {
        rpc_server_public_key_file = optarg;
        break;
      }

      case ARG_RPC_SERVER_SECRET_KEY_FILE: {
        rpc_server_secret_key_file = optarg;
        break;
      }

      case ARG_RPC_AUTHORIZED_CLIENTS_FILE: {
        rpc_authorized_clients_file = optarg;
        break;
      }

      case ARG_RPC_SERVER_SAVE_PUBLIC_KEY_FILE: {
        rpc_server_save_public_key_file = optarg;
        break;
      }

      case ARG_P2P_PORT: {
        default_p2p_port = atoi( optarg );
        p2p_port = default_p2p_port;
        use_strict_ports = true;
        break;
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
              << piac::usage( db_name, logfile,rpc_server_save_public_key_file,
                              rpc_port, p2p_port );
    return EXIT_FAILURE;
  }

  if ((not rpc_server_public_key_file.empty() ||
       not rpc_server_secret_key_file.empty() ||
       not rpc_authorized_clients_file.empty()) &&
       not rpc_secure)
  {
    std::cerr << "Need --rpc-secure to secure RPC channel.\n";
    return EXIT_FAILURE;
  }

  if ((not rpc_server_public_key_file.empty() &&
       rpc_server_secret_key_file.empty()) ||
      (rpc_server_public_key_file.empty() &&
       not rpc_server_secret_key_file.empty()))
  {
    std::cerr << "Need --rpc-authorized-clients-file for authorized RPC "
                 "clients.\n";
    return EXIT_FAILURE;
  }

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

    s_catch_signals();
  }

  epee::set_console_color( epee::console_color_green, /* bright = */ false );
  std::cout << version << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );
  std::cout <<
    "Welcome to piac, where anyone can buy and sell anything privately and\n"
    "securely using the private digital cash, monero. For more information\n"
    "on monero, see https://getmonero.org. This is the server of piac. It\n"
    "can run standalone or as a daemon in the background using --detach.\n"
    "You can use " + piac::cli_executable() + " to interact with it.\n";

  piac::setup_logging( logfile, log_level, /* console_logging = */ false,
                       max_log_file_size, max_log_files );

  MINFO( "Command line: " + cmdline.str() );

  // setup RPC security
  int rpc_ironhouse = 1;
  zmqpp::curve::keypair rpc_server_keys;
  if (rpc_secure) {
    piac::load_server_key( rpc_server_public_key_file,
                           rpc_server_keys.public_key );
    piac::load_server_key( rpc_server_secret_key_file,
                           rpc_server_keys.secret_key );
    piac::load_client_keys( rpc_authorized_clients_file,
                            rpc_authorized_clients );
    // fallback to stonehouse if needed
    if (rpc_server_keys.secret_key.empty() ||
        rpc_server_keys.public_key.empty() ||
        rpc_authorized_clients.empty())
    {
      rpc_ironhouse = 0;
      if (rpc_server_keys.secret_key.empty() ||
          rpc_server_keys.public_key.empty())
      {
        rpc_server_keys = zmqpp::curve::generate_keypair();
        piac::save_public_key( rpc_server_save_public_key_file,
                               rpc_server_keys.public_key );
      }
      rpc_authorized_clients.clear();
    }
  }

  // echo RPC security configured
  if (rpc_secure) {
    if (rpc_ironhouse) {
      epee::set_console_color( epee::console_color_green, /*bright=*/ false );
      std::string ironhouse( "Connections to this server are secure: "
        "Only authenticated connections are accepted." );
      std::cout << ironhouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( ironhouse );
    } else {
      std::string stonehouse( "Connections to this server are secure but not "
        "authenticated: ALL client connections are accepted." );
      epee::set_console_color( epee::console_color_yellow, /*bright=*/ false );
      std::cout << stonehouse << '\n';
      epee::set_console_color( epee::console_color_white, /*bright=*/false );
      MINFO( stonehouse );
      std::cout << "RPC server public key: "
                << rpc_server_keys.public_key << '\n';
    }
  } else {
    epee::set_console_color( epee::console_color_red, /* bright = */ false );
    std::string grasslands( "WARNING: Connections to this server are not "
      "secure" );
    std::cout << grasslands << '\n';
    MWARNING( grasslands );
  }
  epee::set_console_color( epee::console_color_default, /* bright = */false );

  if (rpc_secure) {
    MINFO( "RPC server public key: " << rpc_server_keys.public_key );
  }

  if (detach) {
    MINFO( "Forked PID: " << getpid() );
  }

  // initialize (thread-safe) zmq contexts
  zmqpp::context ctx_p2p;       // for p2p comm
  zmqpp::context ctx_db;        // for inproc comm

  // add initial peers
  std::unordered_map< std::string, zmqpp::socket > my_peers;
  for (const auto& p : peers)
    my_peers.emplace( p, zmqpp::socket( ctx_p2p, zmqpp::socket_type::dealer ) );

  // will store db entry hashes
  std::unordered_set< std::string > my_hashes;

  // start threads
  std::vector< std::thread > threads;

  threads.emplace_back( piac::p2p_thread,
    std::ref(ctx_p2p), std::ref(ctx_db), std::ref(my_peers),
    std::ref(my_hashes), default_p2p_port, p2p_port, use_strict_ports );

  threads.emplace_back( piac::db_thread,
    std::ref(ctx_db), db_name, rpc_port, use_strict_ports, std::ref(my_peers),
    std::ref(my_hashes), rpc_secure, std::ref(rpc_server_keys),
    std::ref(rpc_authorized_clients) );

  // wait for all threads to finish
  for (auto& t : threads) t.join();

  MDEBUG( "graceful exit" );

  // Terminate the child process when the daemon completes
  return EXIT_SUCCESS;
}
