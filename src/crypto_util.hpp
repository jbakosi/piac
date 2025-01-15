// *****************************************************************************
/*!
  \file      src/crypto_util.hpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac cryptography utilities
*/
// *****************************************************************************

#pragma once

#include <string>

namespace piac {

//! Compute sha256 hash of a string
std::string sha256( const std::string& msg );

//! Compute hex encoding of a string
std::string hex( const std::string& digest );

} // ::piac
