// Generates a CurveZMQ keypair

#include <iostream>
#include <zmqpp/curve.hpp>

int main() {
  auto keys = zmqpp::curve::generate_keypair();
  std::cout << "=== CURVE PUBLIC KEY ===\n" << keys.public_key << '\n';
  std::cout << "=== CURVE SECRET KEY ===\n" << keys.secret_key << '\n';
  return EXIT_SUCCESS;
}
