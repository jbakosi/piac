#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/loopscheduler.h>
#include <zmqpp/zmqpp.hpp>

#include "logging.hpp"
#include "project_config.hpp"

// *****************************************************************************
void connect( zmqpp::socket& socket, const std::string& host, int port )
{
  NLOG(DEBUG) << "connect";
  NLOG(INFO) << "Connecting to piac daemon " << host << ':' << port << " ... ";
  socket.connect( "tcp://" + host + ':' + std::to_string(port) );
  socket.send( "connect" );
  // wait for reply from server
  std::string reply;
  auto res = socket.receive( reply );
  NLOG(DEBUG) << reply;
  if (reply == "accept") NLOG(INFO) << "Connected.";
}

// *****************************************************************************
void disconnect( bool& connected, zmqpp::socket& socket,
                 const std::string& host, int port )
{
  NLOG(DEBUG) << "disconnect";
  if (connected) {
    socket.disconnect( "tcp://" + host + ':' + std::to_string(port) );
    NLOG(INFO) << "Disconnected";
  } else {
    NLOG(INFO) << "Not connected";
  }
  connected = false;
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
  NLOG(DEBUG) << "db";
  if (not connected) {
    NLOG(INFO) << "Not connected. Use 'connect' to connect to a piac daemon.";
    return;
  }

  // send message with db command
  socket.send( "db " + cmd );
  // wait for reply from server
  std::string reply;
  auto res = socket.receive( reply );
  NLOG(INFO) << reply;
}

// *****************************************************************************
int main( int argc, char **argv ) {
  try {
    // Setup logging
    std::string logfile( piac::cli_executable() + ".log" );
    setup_logging( piac::cli_executable(), crash_handler, logfile,
                   /* file_logging = */ true, /* console_logging = */ true );

    if (argc > 1) {
      NLOG(ERROR) << "No command line arguments allowed";
      return EXIT_FAILURE;
    }

    std::string version( "piac: " + piac::cli_executable() + " v"
                         + piac::project_version() + "-"
                         + piac::build_type() );

    // Display initial info
    NLOG(INFO) << version;
    std::cout << "Welcome to piac, a peer-to-peer marketplace for "
      "monero users. On piac\nanyone can buy and sell products using "
      "the private digital cash, monero. For more\ninformation on monero, see "
      "https://getmonero.org. This is the command line\nclient of piac. "
      "It needs to connect to a piac daemon to work correctly.\n"
      "Type 'help' to list the available commands.\n";
    NLOG(INFO) << "Logging to " << logfile;

    std::string host = "localhost";
    int port = 55090;
    bool connected = false;

    // initialize the zmq context with a single IO thread
    zmqpp::context context;

    // construct a REQ (request) socket and connect to interface
    zmqpp::socket socket{ context, zmqpp::socket_type::req };

    auto rootMenu = std::make_unique< cli::Menu >( "piac" );

    auto connectCmd = rootMenu -> Insert(
      "connect", { "hostname", "port" },
      [&](std::ostream& out, const std::string& h, int p ) {
        host = h;
        port = p;
        connected = true;
        connect( socket, host, port );
      },
      "Connect to a piac daemon. The first argument specifies a\n"
      "\thostname or an IPv4 address in standard dot notation. See also \n"
      "\t'man gethostbyname'. The second argument is an integer specifying\n"
      "\t a port." );

    auto connectDefaultCmd = rootMenu -> Insert(
      "connect",
      [&](std::ostream& out ) {
        connected = true;
        connect( socket, host, port );
      },
      "Connect to a piac daemon using default hostname and port at\n"
      "\tlocalhost:55090." );

    auto disconnectCmd = rootMenu -> Insert(
      "disconnect",
      [&](std::ostream& out ) { disconnect( connected, socket, host, port ); },
      "Disconnect from a piac daemon." );

    auto dbCmd = rootMenu -> Insert(
      "db", { "command" },
      [&](std::ostream& out, const std::string& query) {
        db_cmd( connected, socket, query );
      },
      "Send database command to server. You must have connected to a\n"
      "\tpiac daemon first. See 'connect'." );

    auto statusCmd = rootMenu -> Insert(
      "status",
      [&](std::ostream& out) { status( connected ); },
      "Query " + piac::cli_executable() + " status" );

    auto versionCmd = rootMenu -> Insert(
      "version",
      [&](std::ostream& out) {
        NLOG(DEBUG) << "version";
        NLOG(INFO) << version;
      },
      "Display piac-cli version" );

    cli::SetColor();
    cli::Cli cli( std::move(rootMenu) );
    // global exit action
    cli.ExitAction( [&](auto& out){
      disconnect( connected, socket, host, port );
      NLOG(DEBUG) << "End " << piac::cli_executable() + '\n'; } );

    cli::LoopScheduler scheduler;
    cli::CliLocalTerminalSession localSession(cli, scheduler, std::cout, 200);
    localSession.ExitAction( [&scheduler](auto& out) { scheduler.Stop(); } );
    scheduler.Run();

    return EXIT_SUCCESS;
  }
  catch (const std::exception& e) {
    NLOG(ERROR) << "Exception in " + piac::cli_executable() + ':'
                << e.what();
  }
  catch (...) {
    NLOG(ERROR) << "Unknown exception in " + piac::cli_executable();
  }

  return EXIT_FAILURE;
}
