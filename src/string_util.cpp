// *****************************************************************************
/*!
  \file      src/string_util.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac string utilities
*/
// *****************************************************************************

#include <regex>

#include "string_util.hpp"

void
piac::trim( std::string& s )
// ****************************************************************************
// Trim a string to remove white space from front and back
//! \param[in,out] s String to remove white spaces from
// ****************************************************************************
{
  const std::string WHITESPACE( " \n\r\t\f\v" );
  auto p = s.find_first_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( p );
  p = s.find_last_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( 0, p + 1 );
}

std::vector< std::string >
piac::tokenize( const std::string& s )
// ****************************************************************************
//  Tokenize, i.e., break up, a string along white space into words
//! \param[in] s String to tokenize
//! \return Tokenized string
// ****************************************************************************
{
  if (s.empty()) return {};
  using std::regex;
  using std::string;
  using std::sregex_token_iterator;
  regex re( "[ \n\r\t\f\v]" );
  sregex_token_iterator it( begin(s), end(s), re, -1 );
  sregex_token_iterator reg_end;
  std::vector< std::string > tokens;
  for (; it != reg_end; ++it) tokens.push_back( it->str() );
  return tokens;
}

[[nodiscard]] int
piac::wordcount( const std::string& s )
// ****************************************************************************
// Count the number of words in a string
//! \param[in] s String to count words in
//! \return Number of words found in string
// ****************************************************************************
{
  auto str = s.data();
  if (str == nullptr) return 0;
  bool inSpaces = true;
  int numWords = 0;
  while (*str != '\0') {
    if (std::isspace(*str)) {
      inSpaces = true;
    } else if (inSpaces) {
      numWords++;
      inSpaces = false;
    }
     ++str;
  }
  return numWords;
}
