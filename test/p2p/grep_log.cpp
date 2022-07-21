#include <fstream>
#include <iostream>

int main( int argc, char** argv )
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <pattern> <filename.log>\n";
    return EXIT_FAILURE;
  }

  // Find last PID in log
  std::ifstream cin( argv[2] );
  if (not cin.good()) {
    std::cout << "Cannot open file " << argv[2] << "\n";
    return EXIT_FAILURE;
  }

//  system("sleep 1");    // wait for daemons to communicate

  int pid = -1;
  while (not cin.eof()) {
    std::string line;
    std::getline( cin, line );
    auto n = line.find( argv[1] );
    if (n != std::string::npos) {
      std::cout << "Found: '" << argv[1] << "'\n";
      return EXIT_SUCCESS;
    }
  }

  return EXIT_FAILURE;
}
