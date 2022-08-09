#include <vector>

#include "document.hpp"

using piac::Document;
using piac::Documents;

// ****************************************************************************
bool
Document::deserialize( const rapidjson::Value& obj ) {
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

// ****************************************************************************
bool
Document::deserialize( const std::string& s ) {
  return JSONBase::deserialize( s );
}

// ****************************************************************************
bool
Document::serialize( rapidjson::Writer< rapidjson::StringBuffer >* writer )
const
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

// ****************************************************************************
std::string
Document::serialize() const {
  return JSONBase::serialize();
}

// ****************************************************************************
bool
Documents::deserialize( const std::string& s ) {
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

// ****************************************************************************
bool
Documents::serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
const
{
  writer->StartArray();
  for (const auto& d : m_documents) d.serialize(writer);
  writer->EndArray();
  return true;
}
