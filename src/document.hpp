#pragma once

#include "jsonbase.hpp"

namespace piac {

class Document : public JSONBase {
  public:
    virtual bool deserialize( const rapidjson::Value& obj ) override;

    bool deserialize( const std::string& s ) override;

    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const override;

    [[nodiscard]] std::string serialize() const override;

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
    virtual bool deserialize( const std::string& s ) override;

    virtual bool deserialize( const rapidjson::Value& ) override {
      return false;
    }

    virtual bool serialize( rapidjson::Writer<rapidjson::StringBuffer>* writer )
      const override;

    [[nodiscard]] std::string serialize() const override {
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

} // piac::
