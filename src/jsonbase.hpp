// *****************************************************************************
/*!
  \file      src/jsonbase.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac classes to interact with JSON serialization library
*/
// *****************************************************************************

#pragma once

#include <string>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

namespace piac {

//! Base class to interact with JSON serialization library
class JSONBase {
  public:
    virtual ~JSONBase() = default;

    //! Deserialize from JSON in file helper
    bool deserializeFromFile( const std::string& filePath );

    //! Serialize JSON format to file helper
    bool serializeToFile( const std::string& filePath ) const;

    //! Serialize JSON writer helper
    [[nodiscard]] virtual std::string serialize() const;

    //! Deserialize helper from JSON in string
    virtual bool deserialize( const std::string& s );

    //! Serialize JSON writer helper
    virtual bool deserialize( const rapidjson::Value& obj ) = 0;
    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const = 0;

  protected:
    //! Validate and initialize JSON formatted document
    bool initDocument( const std::string& s, rapidjson::Document& doc ) const;
};

} // piac::
