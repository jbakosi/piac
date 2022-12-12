// *****************************************************************************
/*!
  \file      src/string_util.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac string utilities
*/
// *****************************************************************************
#pragma once

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <string>
#include <vector>
#include <utility>

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

namespace piac {

//! Trim a string to remove white space from front and back
void trim( std::string& s );

//! Tokenize, i.e., break up, a string along white space into words
[[nodiscard]] std::vector< std::string >
tokenize( const std::string& s );

//! Count the number of words in a string
[[nodiscard]] int
wordcount( const std::string& s );

//! Split string into two substrings at delimiter
[[nodiscard]] std::pair< std::string, std::string >
split( std::string s, const std::string& delim );

} // ::piac
