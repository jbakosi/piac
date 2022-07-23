#include <cryptopp/sha.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>

#include "utils/misc_log_ex.h"
#include "util.hpp"

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

  std::cout << "Logging to file '" << logfile << "' at log level "
            << log_level << ".\n";
  MINFO( "Max log file size: " << max_log_file_size << ", max log files: " <<
         max_log_files );
}

// ****************************************************************************
std::string
piac::sha256( const std::string& msg ) {
  using namespace CryptoPP;
  std::string digest;
  SHA256 hash;
  hash.Update(( const byte*)msg.data(), msg.size() );
  digest.resize( hash.DigestSize() );
  hash.Final( (byte*)&digest[0] );
  return digest;
}

// ****************************************************************************
std::string
piac::hex( const std::string& digest ) {
  using namespace CryptoPP;
  std::string hex;
  HexEncoder encoder( new StringSink( hex ) );
  StringSource( digest, true, new Redirector( encoder ) );
  return hex;
}

// ****************************************************************************
void
piac::trim( std::string& s ) {
  const std::string WHITESPACE( " \n\r\t\f\v" );
  auto p = s.find_first_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( p );
  p = s.find_last_not_of( WHITESPACE );
  s = p == std::string::npos ? "" : s.substr( 0, p + 1 );
}
