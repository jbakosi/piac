#include <regex>

#include "string_util.hpp"

// ****************************************************************************
void
piac::trim( std::string& s ) {
  const std::string WHITESPACE( " \n\r\t\f\v" );
  auto p = s.find_first_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( p );
  p = s.find_last_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( 0, p + 1 );
}

// ****************************************************************************
std::vector< std::string >
piac::tokenize( std::string& s ) {
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
