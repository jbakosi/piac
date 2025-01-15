// *****************************************************************************
/*!
  \file      src/document.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac document functionality to interact with database library
*/
// *****************************************************************************

#include <vector>

#include "document.hpp"

using piac::Document;
using piac::Documents;

bool
Document::deserialize( const rapidjson::Value& obj )
// ****************************************************************************
//  Deserialize (piac) document state from JSON object
//! \param[in] obj Input JSON object
//! \return True if no error occurred
// ****************************************************************************
{
  id( obj["id"].GetInt() );
  title( obj["title"].GetString() );
  author( obj["author"].GetString() );
  description( obj["description"].GetString() );
  price( obj["price"].GetDouble() );
  category( obj["category"].GetString() );
  condition( obj["condition"].GetString() );
  shipping( obj["shipping"].GetString() );
  format( obj["format"].GetString() );
  location( obj["location"].GetString() );
  keywords( obj["keywords"].GetString() );
  return true;
}

bool
Document::serialize( rapidjson::Writer< rapidjson::StringBuffer >* writer )
const
// ****************************************************************************
//  Serialize (piac) document state to JSON format
//! \param[in] writer Pointer to rapidjson write object to write object to
//! \return True if no error occurred
// ****************************************************************************
{
  writer->StartObject();
  writer->String( "id" );        writer->Int( m_id );
  writer->String("title");       writer->String( m_title.c_str() );
  writer->String("author");      writer->String( m_author.c_str() );
  writer->String("description"); writer->String( m_description.c_str() );
  writer->String( "price" );     writer->Double( m_price );
  writer->String("category");    writer->String( m_category.c_str() );
  writer->String("condition");   writer->String( m_condition.c_str() );
  writer->String("shipping");    writer->String( m_shipping.c_str() );
  writer->String("format");      writer->String( m_format.c_str() );
  writer->String("location");    writer->String( m_location.c_str() );
  writer->String("keywords");    writer->String( m_keywords.c_str() );
  writer->EndObject();
  return true;
}

bool
Documents::deserialize( const std::string& s )
// ****************************************************************************
//  Deserialize multiple (piac) ddocuments from JSON format
//! \param[in] s String containing multiple JSON formatted documents
//! \return True if no error occurred
// ****************************************************************************
{
  rapidjson::Document jdoc;
  if (not initDocument( s, jdoc )) return false;
  if (jdoc.IsArray()) {
    for (auto itr = jdoc.Begin(); itr != jdoc.End(); ++itr) {
      Document p;
      p.deserialize( *itr );
      m_documents.push_back( p );
    }
  } else {
    Document p;
    p.deserialize( jdoc );
    m_documents.push_back( p );
  }
  return true;
}

bool
Documents::serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
const
// ****************************************************************************
//  Serialize multiple (piac) documents to JSON format
//! \param[in] writer Pointer to rapidjson write object to write object to
//! \return True if no error occurred
// ****************************************************************************
{
  writer->StartArray();
  for (const auto& d : m_documents) d.serialize(writer);
  writer->EndArray();
  return true;
}
