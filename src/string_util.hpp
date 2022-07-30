#pragma once

#include <string>
#include <unordered_set>

namespace piac {

void trim( std::string& s );

std::unordered_set< std::string > tokenize( std::string& s );

} // ::piac
