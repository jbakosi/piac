#include "logging_util.hpp"

// *****************************************************************************
void
piac::setup_logging( const std::string& logfile,
                     const std::string& log_level,
                     bool console_logging,
                     const std::size_t max_log_file_size,
                     const std::size_t max_log_files )
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
