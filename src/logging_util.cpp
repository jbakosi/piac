// *****************************************************************************
/*!
  \file      src/logging_util.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac logging utilities, hookiing up with that of monero
*/
// *****************************************************************************

#include "logging_util.hpp"

void
piac::setup_logging( const std::string& logfile,
                     const std::string& log_level,
                     bool console_logging,
                     const std::size_t max_log_file_size,
                     const std::size_t max_log_files )
// *****************************************************************************
//  Hook up to monero's logging infrastructure
//! \param[in] logfile Log file name
//! \param[in] log_level Log level to set
//! \param[in] console_logging True: enable logging to console, false: file only
//! \param[in] max_log_file_size Set maximum log file size in bytes
//! \param[in] max_log_files Set limit on number of log files
// *****************************************************************************
{
  mlog_configure( mlog_get_default_log_path( logfile.c_str() ),
                  console_logging, max_log_file_size, max_log_files );
  mlog_set_log( log_level.c_str() );
  MLOG_SET_THREAD_NAME( "main" );

  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Logging to file '" << logfile << "' at log level "
            << log_level << ".\n";
  epee::set_console_color( epee::console_color_default, /* bright = */ false );
  MINFO( "Max log file size: " << max_log_file_size << ", max log files: " <<
         max_log_files );
}
