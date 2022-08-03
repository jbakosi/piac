#pragma once

#include <string>
#include <vector>

namespace piac {

void trim( std::string& s );

[[nodiscard]] std::vector< std::string > tokenize( std::string& s );

} // ::piac
