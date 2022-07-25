#pragma once

#include <vector>
#include <unordered_set>

#include "xapian.h"
#include "jsonbase.hpp"

namespace piac {

class Document : public JSONBase {
  public:
    virtual bool deserialize( const rapidjson::Value& obj ) override {
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

    bool deserialize( const std::string& s ) {
      return JSONBase::deserialize( s );
    }

    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const override
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

    [[nodiscard]] std::string serialize() const {
      return JSONBase::serialize();
    }

    int id() const { return m_id; }
    void id( int i ) { m_id = i; }

    const std::string& title() const { return m_title; }
    void title( const std::string& t ) { m_title = t; }

    const std::string& author() const { return m_author; }
    void author( const std::string& a ) { m_author = a; }

    const std::string& description() const { return m_description; }
    void description( const std::string& d ) { m_description = d; }

    double price() const { return m_price; }
    void price( double p ) { m_price = p; }

    const std::string& category() const { return m_category; }
    void category( const std::string& t ) { m_category = t; }

    const std::string& condition() const { return m_condition; }
    void condition( const std::string& t ) { m_condition = t; }

    const std::string& shipping() const { return m_shipping; }
    void shipping( const std::string& t ) { m_shipping = t; }

    const std::string& format() const { return m_format; }
    void format( const std::string& t ) { m_format = t; }

    const std::string& location() const { return m_location; }
    void location( const std::string& t ) { m_location = t; }

    const std::string& keywords() const { return m_keywords; }
    void keywords( const std::string& t ) { m_keywords = t; }

    // SHA is not serialized, but regenerated when needed
    const std::string& sha() const { return m_sha; }
    void sha( const std::string& s ) { m_sha = s; }

  private:
    int m_id;
    std::string m_title;
    std::string m_author;
    std::string m_description;
    double m_price;
    std::string m_category;
    std::string m_condition;
    std::string m_shipping;
    std::string m_format;
    std::string m_location;
    std::string m_keywords;
    std::string m_sha;
};

class Documents : public JSONBase {
  public:
    virtual bool deserialize( const std::string& s ) {
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

    virtual bool deserialize( const rapidjson::Value& obj ) override {
      return false;
    }

    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const override
    {
       writer->StartArray();
       for (const auto& d : m_documents) d.serialize(writer);
       writer->EndArray();
       return true;
    }

    [[nodiscard]] std::string serialize() {
      return JSONBase::serialize();
    }

    const std::vector< Document >& documents() const { return m_documents; }
    std::vector< Document >& documents() { return m_documents; }

    const Document& operator[]( std::size_t i ) const { return m_documents[i]; }
    Document& operator[]( std::size_t i ) {
      return const_cast< Document& >(
               static_cast< const Documents& >( *this ).operator[]( i ) );
    }

  private:
    std::vector< Document > m_documents;
};

namespace XapianValues {
  Xapian::valueno ID = 0;
  Xapian::valueno PRICE = 1;
}

Xapian::doccount get_doccount( const std::string db_name );

std::string
add_document( const std::string& author,
              Xapian::TermGenerator& indexer,
              Xapian::WritableDatabase& db,
              Document& ndoc );

std::string
index_db( const std::string& author,
          const std::string& db_name,
          const std::string& input_filename,
          const std::unordered_set< std::string >& my_hashes = {} );

[[nodiscard]] std::string
db_query( const std::string& db_name, std::string&& query_string );

[[nodiscard]] std::vector< std::string >
db_get_docs( const std::string& db_name,
             const std::vector< std::string >& hashes );

std::size_t
db_put_docs( const std::string& db_name,
             const std::vector< std::string >& docs );

std::string
db_rm_docs( const std::string& author,
            const std::string& db_name,
            const std::unordered_set< std::string >& hashes_to_delete,
            const std::unordered_set< std::string >& my_hashes = {} );

[[nodiscard]] std::vector< std::string >
db_list_hash( const std::string& db_name, bool inhex );

[[nodiscard]] std::vector< std::string >
db_list_doc( const std::string& db_name );

[[nodiscard]] std::size_t
db_list_numuser( const std::string& db_name );

std::string
db_add( const std::string& author,
        const std::string& db_name,
        std::string&& cmd_string,
        const std::unordered_set< std::string >& my_hashes = {} );

std::string
db_rm( const std::string& author,
       const std::string& db_name,
       std::string&& cmd_string,
       const std::unordered_set< std::string >& my_hashes );

std::string db_list( const std::string& db_name, std::string&& cmd_string );

} // piac::
