#pragma once

#include "utils/misc_log_ex.h"

// Logger macros: MFATAL, MERROR, MWARNING, MINFO, MDEBUG, MTRACE, etc.
// See also <monero>/contrib/epee/include/misc_log_ex.h.

// *****************************************************************************
static void setup_logging( const std::string& logfile,
                           const std::string& log_level,
                           bool console_logging,
                           const std::size_t max_log_file_size,
                           const std::size_t max_log_files )
{
  mlog_configure( mlog_get_default_log_path( logfile.c_str() ),
                  console_logging, max_log_file_size, max_log_files );
  mlog_set_log( log_level.c_str() );
  MLOG_SET_THREAD_NAME( "main" );

  std::cout << "Logging to file '" << logfile << "' at log level "
            << log_level << ".\n";
  MINFO( "Max log file size: " << max_log_file_size << ", max log files: " <<
         max_log_files );
}
