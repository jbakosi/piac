#pragma once

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wdouble-promotion"
  #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wsuggest-destructor-override"
  #pragma clang diagnostic ignored "-Wsuggest-override"
  #pragma clang diagnostic ignored "-Wimplicit-int-conversion"
  #pragma clang diagnostic ignored "-Wweak-vtables"
  #pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif

#include "utils/misc_log_ex.h"

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

// Logger macros: MFATAL, MERROR, MWARNING, MINFO, MDEBUG, MTRACE, etc.
// See also <monero>/contrib/epee/include/misc_log_ex.h.

namespace piac {

void setup_logging( const std::string& logfile,
                    const std::string& log_level,
                    bool console_logging,
                    const std::size_t max_log_file_size,
                    const std::size_t max_log_files );

} // ::piac
