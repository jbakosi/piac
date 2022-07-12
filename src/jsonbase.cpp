#include <fstream>
#include <sstream>

#include "jsonbase.hpp"

using piac::JSONBase;

// *****************************************************************************
std::string
JSONBase::serialize() const {
  rapidjson::StringBuffer ss;
  rapidjson::Writer< rapidjson::StringBuffer > writer( ss );
  if (serialize( &writer)) return ss.GetString();
  return {};
}

// *****************************************************************************
bool
JSONBase::deserialize( const std::string& s ) {
  rapidjson::Document doc;
  if (not initDocument( s, doc )) return false;
  deserialize( doc );
  return true;
}

// *****************************************************************************
bool
JSONBase::deserializeFromFile( const std::string& filePath ) {
  std::ifstream f( filePath );
  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();
  return deserialize( buffer.str() );
}

// *****************************************************************************
bool
JSONBase::serializeToFile( const std::string& filePath ) const {
  std::ofstream f( filePath );
  f << serialize();
  f.flush();
  f.close();
  return true;
}

// *****************************************************************************
bool
JSONBase::initDocument( const std::string& s, rapidjson::Document& doc ) const {
  if (s.empty()) return false;
  std::string validJson( s );
  return not doc.Parse( validJson.c_str() ).HasParseError() ? true : false;
}
