// *****************************************************************************
/*!
  \file      src/curve_keygen.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Generate a CurveZMQ keypair
*/
// *****************************************************************************

#include <iostream>
#include <zmqpp/curve.hpp>

int main() {
  auto keys = zmqpp::curve::generate_keypair();
  std::cout << "=== CURVE PUBLIC KEY ===\n" << keys.public_key << '\n';
  std::cout << "=== CURVE SECRET KEY ===\n" << keys.secret_key << '\n';
  return EXIT_SUCCESS;
}
