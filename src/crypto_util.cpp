// *****************************************************************************
/*!
  \file      src/crypto_util.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac cryptography utilities
*/
// *****************************************************************************

#include <cryptopp/sha.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>

#include "crypto_util.hpp"

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wold-style-cast"
#endif

std::string
piac::sha256( const std::string& msg )
// ****************************************************************************
//  Compute sha256 hash of a string
//! \param[in] msg String whose hash to compute
//! \return Hash computed
// ****************************************************************************
{
  using namespace CryptoPP;
  std::string digest;
  SHA256 hash;
  hash.Update( (const byte*)msg.data(), msg.size() );
  digest.resize( hash.DigestSize() );
  hash.Final( (byte*)&digest[0] );
  return digest;
}

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

std::string
piac::hex( const std::string& digest )
// ****************************************************************************
//  Compute hex encoding of a string
//! \param[in] digest String to encode in hex
//! \return Hex encoding
// ****************************************************************************
{
  using namespace CryptoPP;
  std::string hex;
  HexEncoder encoder( new StringSink( hex ) );
  StringSource( digest, true, new Redirector( encoder ) );
  return hex;
}
