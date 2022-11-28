// *****************************************************************************
/*!
  \file      src/jsonbase.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac functionality to interact with JSON serialization library
*/
// *****************************************************************************

#include <fstream>
#include <sstream>

#include "jsonbase.hpp"

using piac::JSONBase;

std::string
JSONBase::serialize() const
// *****************************************************************************
//  Serialize JSON writer helper
//! \return Serialized JSON data
// *****************************************************************************
{
  rapidjson::StringBuffer ss;
  rapidjson::Writer< rapidjson::StringBuffer > writer( ss );
  if (serialize(&writer)) return ss.GetString();
  return {};
}

bool
JSONBase::deserialize( const std::string& s )
// *****************************************************************************
//  Deserialize helper from JSON in string
//! \param[in] s String containing JSON format to deserialize
//! \return True if successful
// *****************************************************************************
{
  rapidjson::Document doc;
  if (not initDocument( s, doc )) return false;
  deserialize( doc );
  return true;
}

bool
JSONBase::deserializeFromFile( const std::string& filePath )
// *****************************************************************************
//  Deserialize from JSON in file helper
//! \param[in] filePath Filename containing JSON format to deserialize
//! \return True if successful
// *****************************************************************************
{
  std::ifstream f( filePath );
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();
  return deserialize( buffer.str() );
}

bool
JSONBase::serializeToFile( const std::string& filePath ) const
// *****************************************************************************
//  Serialize JSON format to file helper
//! \param[in] filePath Filename to write JSON to
//! \return True if successful
// *****************************************************************************
{
  std::ofstream f( filePath );
  f << serialize();
  f.flush();
  f.close();
  return true;
}

bool
JSONBase::initDocument( const std::string& s, rapidjson::Document& doc ) const
// *****************************************************************************
//  Validate and initialize JSON formatted document
//! \param[in] s String containing JSON formatted data
//! \param[in,out] doc JSON document to store data in
//! \return True if JSON is valid
// *****************************************************************************
{
  if (s.empty()) return false;
  std::string validJson( s );
  return not doc.Parse( validJson.c_str() ).HasParseError() ? true : false;
}
