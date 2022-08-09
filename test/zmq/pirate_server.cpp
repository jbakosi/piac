// Server that simulates connection problems for a Lazy Pirate client

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wundef"
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
  #pragma clang diagnostic ignored "-Wdocumentation"
  #pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include <zmqpp/zmqpp.hpp>

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <unistd.h>

#include "logging_util.hpp"

// Provide random number from 0..(num-1)
#define within(num) (int)((float) (num) * random () / (RAND_MAX + 1.0))

int main( int argc, char** argv ) {

  int detach = 0;
  std::vector< std::string > args( argv, argv+argc );
  for (std::size_t i=1; i<args.size(); ++i)
    if (args[i] == "--detach")
      detach = 1;

  if (detach) {

    // Fork the current process. The parent process continues with a process ID
    // greater than 0.  A process ID lower than 0 indicates a failure in either
    // process.
    pid_t pid = fork();
    if (pid > 0) {
      std::cout << "Running in daemon mode. Forked PID: " << pid << std::endl;
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

  }

  std::string logfile( args[0] + ".log" );
  std::string log_level( "4" );
  std::size_t max_log_file_size = MAX_LOG_FILE_SIZE;
  std::size_t max_log_files = MAX_LOG_FILES;
  piac::setup_logging( logfile, log_level, /* console_logging = */ false,
                       max_log_file_size, max_log_files );

  if (detach) {
    auto pid = getpid();
    std::cout << "Forked PID: " << pid << '\n';
    MINFO( "Forked PID: " << pid );
  }

  srandom( static_cast<unsigned>( time(nullptr) ) );

  std::string server_port = "5555";
  zmqpp::context ctx;
  zmqpp::socket server( ctx, zmqpp::socket_type::rep );
  server.bind( "tcp://*:" + server_port );
  MINFO( "Bound to port " << server_port );  
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Server bound to port " << server_port << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  #if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wold-style-cast"
    #pragma clang diagnostic ignored "-Wdouble-promotion"
    #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
  #endif

  int cycles = 0;
  while (1) {
    zmqpp::message msg;
    server.receive( msg );
    std::string request;
    msg >> request;
    cycles++;

    // Simulate various problems, after a few cycles
    if (cycles > 2 && within(2) == 0) {
      MINFO( "Simulating a crash" );
      break;
    } else if (cycles > 2 && within(2) == 0) {
      MINFO( "Simulating CPU overload" );
      sleep(2);
    }
    MINFO( "Normal request (" << request << ")" );
    sleep(1); // do some heavy work
    server.send( request );
  }

  #if defined(__clang__)
    #pragma clang diagnostic pop
  #endif

  return EXIT_SUCCESS;
}
