#include <cryptopp/sha.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>

#include "crypto_util.hpp"

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
