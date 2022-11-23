#include <fstream>
#include <iostream>
#include <signal.h>

int main( int argc, char** argv )
{
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <filename.log>\n";
    return EXIT_FAILURE;
  }

  // Find last PID in log
  std::ifstream cin( argv[1] );
  if (not cin.good()) {
    std::cout << "Cannot open file " << argv[1] << "\n";
    return EXIT_FAILURE;
  }

  int pid = -1;
  while (not cin.eof()) {
    std::string line;
    std::getline( cin, line );
    auto n = line.find( "PID" );
    if (n != std::string::npos) {
      pid = std::stoi( line.substr( n+5 ) );
    }
  }

  // Kill PID
  std::cout << "Killing PID: " << pid << std::endl;
  auto ret = kill( pid, SIGTERM );
  if (ret == -1) {
    perror("kill");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
