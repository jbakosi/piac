#pragma once

#include <string>

namespace piac {

std::string sha256( const std::string& msg );

std::string hex( const std::string& digest );

} // ::piac
