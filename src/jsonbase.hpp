#pragma once

#include <string>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

namespace piac {

class JSONBase {
  public:
    virtual ~JSONBase() = default;
    bool deserializeFromFile( const std::string& filePath );
    bool serializeToFile( const std::string& filePath ) const;
    [[nodiscard]] virtual std::string serialize() const;
    virtual bool deserialize( const std::string& s );
    virtual bool deserialize( const rapidjson::Value& obj ) = 0;
    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const = 0;

  protected:
    bool initDocument( const std::string & s, rapidjson::Document &doc ) const;
};

} // piac::
