// *****************************************************************************
/*!
  \file      src/string_util.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac string utilities
*/
// *****************************************************************************

#pragma once

#include <string>
#include <vector>

namespace piac {

//! Trim a string to remove white space from front and back
void trim( std::string& s );

//! Tokenize, i.e., break up, a string along white space into words
[[nodiscard]] std::vector< std::string > tokenize( const std::string& s );

//! Count the number of words in a string
[[nodiscard]] int wordcount( const std::string& s );

} // ::piac
