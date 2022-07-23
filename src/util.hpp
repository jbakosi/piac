#pragma once

// Logger macros: MFATAL, MERROR, MWARNING, MINFO, MDEBUG, MTRACE, etc.
// See also <monero>/contrib/epee/include/misc_log_ex.h.

#include "utils/misc_log_ex.h"

namespace piac {

void setup_logging( const std::string& logfile,
                    const std::string& log_level,
                    bool console_logging,
                    const std::size_t max_log_file_size,
                    const std::size_t max_log_files );

std::string sha256( const std::string& msg );

std::string hex( const std::string& digest );

void trim( std::string& s );

} // ::piac
