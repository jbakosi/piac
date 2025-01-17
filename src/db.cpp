// *****************************************************************************
/*!
  \file      src/db.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac database functionality
*/
// *****************************************************************************

#include <string>

#include "string_util.hpp"
#include "logging_util.hpp"
#include "crypto_util.hpp"
#include "db.hpp"
#include "document.hpp"

Xapian::doccount
piac::get_doccount( const std::string db_name )
// *****************************************************************************
//  Get number of documents in Xapian database
//! \param[in] db_name Name of Xapian db to operate on
//! \return Number of documents in database
// *****************************************************************************
{
  try {
    Xapian::Database db( db_name );
    return db.get_doccount();
  } catch ( const Xapian::Error &e ) {
    MWARNING( e.get_description() );
  }
  return {};
}

std::string
piac::add_document( const std::string& author,
                    Xapian::TermGenerator& indexer,
                    Xapian::WritableDatabase& db,
                    Document& ndoc )
// ****************************************************************************
//  Add document to Xapian database
//! \param[in] author Author of the database document
//! \param[in,out] indexer Xapian indexer to use for database indexing
//! \param[in,out] db Xapian database object to add document to
//! \param[in,out] ndoc Json document to add
//! \return Hash of the document added
// ****************************************************************************
{
  assert( not author.empty() );
  Xapian::Document doc;
  indexer.set_document( doc );
  // Index each field with a suitable prefix
  indexer.index_text( ndoc.title(), 1, "S" );
  indexer.index_text( ndoc.description(), 1, "XD" );
  indexer.index_text( ndoc.category(), 1, "XC" );
  indexer.index_text( ndoc.condition(), 1, "XO" );
  indexer.index_text( ndoc.shipping(), 1, "XS" );
  indexer.index_text( ndoc.format(), 1, "XF" );
  indexer.index_text( ndoc.location(), 1, "XL" );
  indexer.index_text( ndoc.keywords(), 1, "K" );
  // Index fields without prefixes for general search
  indexer.index_text( ndoc.title() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.description() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.category() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.condition() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.shipping() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.format() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.location() );
  indexer.increase_termpos();
  indexer.index_text( ndoc.keywords() );
  // Add value fields
  doc.add_value( 1, std::to_string( ndoc.price() ) );
  // Generate a hash of the doc fields and store it in the document
  ndoc.author( author );
  auto entry = ndoc.serialize();
  ndoc.sha( sha256( entry ) );
  auto sha = ndoc.sha();
  // Ensure each object ends up in the database only once no matter how
  // many times we run the indexer
  doc.add_boolean_term( std::to_string( ndoc.id() ) );
  doc.set_data( entry );
  doc.add_term( 'Q' + sha );
  // Add Xapian doc to db
  db.replace_document( 'Q' + sha, doc );
  return sha;
}

std::string
piac::index_db( const std::string& author,
                const std::string& db_name,
                const std::string& input_filename,
                const std::unordered_set< std::string >& my_hashes )
// ****************************************************************************
//  Index Xapian database
//! \param[in] author Author of the database document
//! \param[in] db_name Name of the Xapian database object
//! \param[in] input_filename File to read JSON data from
//! \param[in] my_hashes Hashes to check for duplicates when adding documents
//! \return Info string showing how many documents have been added
// ****************************************************************************
{
  assert( not author.empty() );

  std::ifstream f( input_filename );
  if (not f.good()) {
    return "Cannot open database input file: " + input_filename;
  }

  MDEBUG( "Indexing " << input_filename );
  Xapian::WritableDatabase db( db_name, Xapian::DB_CREATE_OR_OPEN );
  Xapian::TermGenerator indexer;
  Xapian::Stem stemmer( "english" );
  indexer.set_stemmer( stemmer );
  indexer.set_stemming_strategy( indexer.STEM_SOME_FULL_POS );
  std::size_t numins = 0;
  try {
    // Read json db from file
    Documents ndoc;
    ndoc.deserializeFromFile( input_filename );
    // Insert documents we do not yet have from json into xapian db
    for (auto& d : ndoc.documents()) {
      d.author( author );
      auto entry = d.serialize();
      if (my_hashes.find( sha256( entry ) ) == end(my_hashes)) {
        add_document( author, indexer, db, d );
        ++numins;
      }
    }
    MDEBUG( "Indexed " << numins << " entries" );
    // Explicitly commit so that we get to see any errors. WritableDatabase's
    // destructor will commit implicitly (unless we're in a transaction) but
    // will swallow any exceptions produced.
    db.commit();

  } catch ( const Xapian::Error &e ) {
    MERROR( e.get_description() );
  }
  return "Added " + std::to_string( numins ) + " entries";
}

[[nodiscard]] std::string
piac::db_query( const std::string& db_name, std::string&& cmd )
// *****************************************************************************
//  Query Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \param[in,out] cmd Query command
//! \return Result of the database query
// *****************************************************************************
{
  try {

    MDEBUG( "db query: '" << cmd << "'" );
    // Open the database for searching
    Xapian::Database db( db_name );
    // Start an enquire session
    Xapian::Enquire enquire( db );
    // Parse the query string to produce a Xapian::Query object
    Xapian::QueryParser qp;
    Xapian::Stem stemmer("english");
    qp.set_stemmer( stemmer );
    qp.set_database( db );
    qp.set_stemming_strategy( Xapian::QueryParser::STEM_SOME );
    Xapian::Query query = qp.parse_query( cmd );
    MDEBUG( "parsed query: '" << query.get_description() << "'" );
    // Find the top 10 results for the query
    enquire.set_query( query );
    MDEBUG( "set query: '" << query.get_description() << "'" );
    Xapian::MSet matches = enquire.get_mset( 0, 10 );
    MDEBUG( "got matches" );
    // Construct the results
    auto nr = matches.get_matches_estimated();
    MDEBUG( "got estimated matches: " << nr );
    std::stringstream result;
    result << nr << " results found.";
    if (nr) {
      result << "\nmatches 1-" << matches.size() << ":\n\n";
      for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i) {
        MDEBUG( "getting match: " << i.get_rank() );
        result << i.get_rank() + 1 << ": " << i.get_weight() << " docid=" << *i
                 << " [" << i.get_document().get_data() << "]\n";
      }
    }
    //MDEBUG( "results: " + result.str() );
    return result.str();

  } catch ( const Xapian::Error &e ) {
    MERROR( e.get_description() );
  }
  return {};
}

[[nodiscard]] std::vector< std::string >
piac::db_get_docs( const std::string& db_name,
                   const std::vector< std::string >& hashes )
// *****************************************************************************
//  Get documents from Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \param[in] hashes Hashes of database documents to get retrieve
//! \return Result of the database query
// *****************************************************************************
{
  std::vector< std::string > docs;
  try {

    Xapian::Database db( db_name );
    Xapian::doccount dbsize = db.get_doccount();
    if (dbsize == 0) return {};

    for (const auto& h : hashes) {
      assert( h.size() == 32 );
      auto p = db.postlist_begin( 'Q' + h );
      if (p != db.postlist_end( 'Q' + h ))
        docs.push_back( db.get_document( *p ).get_data() );
      else
        MWARNING( "Document not found: " << hex(h) );
    }

  } catch ( const Xapian::Error &e ) {
    if (e.get_description().find("No such file") == std::string::npos)
      MERROR( e.get_description() );
  }

  return docs;
}

std::size_t
piac::db_put_docs( const std::string& db_name,
                   const std::vector< std::string >& docs )
// *****************************************************************************
//  Put documents to Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \param[in] docs Documents to insert to Xapian database
//! \return Number of documents inserted
// *****************************************************************************
{
  try {
    MDEBUG( "Inserting & indexing " << docs.size() << " new entries" );
    Xapian::WritableDatabase db( db_name, Xapian::DB_CREATE_OR_OPEN );
    Xapian::TermGenerator indexer;
    Xapian::Stem stemmer( "english" );
    indexer.set_stemmer( stemmer );
    indexer.set_stemming_strategy( indexer.STEM_SOME_FULL_POS );

    // Insert all documents into xapian db
    for (const auto& d : docs) {
      Document ndoc;
      ndoc.deserialize( d );
      // refuse doc without author
      auto author = ndoc.author();
      if (not author.empty()) add_document( author, indexer, db, ndoc );
    }

    MDEBUG( "Finished indexing " << docs.size() <<
            " new entries, commit to db" );
    // Explicitly commit so that we get to see any errors. WritableDatabase's
    // destructor will commit implicitly (unless we're in a transaction) but
    // will swallow any exceptions produced.
    db.commit();
    return docs.size();

  } catch ( const Xapian::Error &e ) {
    MERROR( e.get_description() );
  }

  return 0;
}

std::string
piac::db_rm_docs( const std::string& author,
                  const std::string& db_name,
                  const std::unordered_set< std::string >& hashes_to_delete,
                  const std::unordered_set< std::string >& my_hashes )
// *****************************************************************************
//  Remove documents from Xapian database
//! \param[in] author Author of the database document
//! \param[in] db_name Name of the Xapian database object
//! \param[in] hashes_to_delete Hashes of documents to delete
//! \param[in] my_hashes Hashes to check for duplicates when removing documents
//! \return Info on number of documents removed
// *****************************************************************************
{
  std::size_t numrm = 0;
  try {

    Xapian::WritableDatabase db( db_name );
    Xapian::doccount dbsize = db.get_doccount();
    if (dbsize == 0) return "no docs";

    for (const auto& h : my_hashes) {
      assert( h.size() == 32 );
      auto p = db.postlist_begin( 'Q' + h );
      if ( p != db.postlist_end( 'Q' + h ) &&
           hashes_to_delete.find(hex(h)) != end(hashes_to_delete) )
      {
        auto entry = db.get_document( *p ).get_data();
        Document ndoc;
        ndoc.deserialize( entry );
        if (author == ndoc.author()) {
          MDEBUG( "db rm" + sha256(h) );
          db.delete_document( 'Q' + h );
          ++numrm;
        } else {
          MDEBUG( "db rm auth: " + hex(author) + " != " + hex(ndoc.author()) );
          return "db rm: author != user";
        }
      }
    }

  } catch ( const Xapian::Error &e ) {
    if (e.get_description().find("No such file") == std::string::npos)
      MERROR( e.get_description() );
  }

  return "Removed " + std::to_string( numrm ) + " entries";
}

[[nodiscard]] std::vector< std::string >
piac::db_list_hash( const std::string& db_name, bool inhex )
// *****************************************************************************
//  List hashes from Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \param[in] inhex True to list hashes hex-encoded
//! \return List of hashes
// *****************************************************************************
{
  std::vector< std::string > hashes;
  try {

    Xapian::Database db( db_name );
    Xapian::doccount dbsize = db.get_doccount();
    if (dbsize == 0) return {};

    for (auto it = db.postlist_begin({}); it != db.postlist_end({}); ++it) {
      auto entry = db.get_document( *it ).get_data();
      auto digest = sha256( entry );
      hashes.emplace_back( inhex ? hex(digest) : digest );
    }

  } catch ( const Xapian::Error &e ) {
    if (e.get_description().find("No such file") == std::string::npos)
      MERROR( e.get_description() );
  }

  return hashes;
}

[[nodiscard]] std::vector< std::string >
piac::db_list_doc( const std::string& db_name )
// *****************************************************************************
//  List documents from Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \return List of documents
// *****************************************************************************
{
  std::vector< std::string > docs;
  try {

    Xapian::Database db( db_name );
    Xapian::doccount dbsize = db.get_doccount();
    if (dbsize == 0) return {};

    for (auto it = db.postlist_begin({}); it != db.postlist_end({}); ++it) {
      auto entry = db.get_document( *it ).get_data();
      auto digest = sha256( entry );
      Document d;
      d.deserialize( entry );
      d.author( hex( d.author() ) );
      docs.emplace_back( hex( digest ) + ": " + d.serialize() );
    }

  } catch ( const Xapian::Error &e ) {
    if (e.get_description().find("No such file") == std::string::npos)
      MERROR( e.get_description() );
  }

  return docs;
}

[[nodiscard]] std::size_t
piac::db_list_numuser( const std::string& db_name )
// *****************************************************************************
//  List number of unique users in Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \return Number of unique users created documents in database
// *****************************************************************************
{
  try {

    Xapian::Database db( db_name );
    Xapian::doccount dbsize = db.get_doccount();
    if (dbsize == 0) return {};

    std::unordered_set< std::string > user;
    for (auto it = db.postlist_begin({}); it != db.postlist_end({}); ++it) {
      auto entry = db.get_document( *it ).get_data();
      Document d;
      d.deserialize( entry );
      user.insert( d.author() );
    }
    return user.size();


  } catch ( const Xapian::Error &e ) {
    if (e.get_description().find("No such file") == std::string::npos)
      MERROR( e.get_description() );
  }

  return {};
}

std::string
piac::db_add( const std::string& author,
              const std::string& db_name,
              std::string&& cmd,
              const std::unordered_set< std::string >& my_hashes )
// *****************************************************************************
//  Add documents to Xapian database
//! \param[in] author Author of the database document
//! \param[in] db_name Name of the Xapian database object
//! \param[in,out] cmd Add command
//! \param[in] my_hashes Hashes to check for duplicates when adding documents
//! \return Info string after add database operation
// *****************************************************************************
{
  trim( cmd );
  MDEBUG( "db add " + cmd );
  assert( not author.empty() );
  if (cmd[0]=='j' && cmd[1]=='s' && cmd[2]=='o' && cmd[3]=='n') {
    cmd.erase( 0, 5 );
    MDEBUG( "Add json file: '" << cmd << "' to db" );
    return index_db( author, db_name, cmd, my_hashes );
  }
  return "unknown cmd";
}

std::string
piac::db_rm( const std::string& author,
             const std::string& db_name,
             std::string&& cmd,
             const std::unordered_set< std::string >& my_hashes )
// *****************************************************************************
//  Remove documents from Xapian database
//! \param[in] author Author of the database document
//! \param[in] db_name Name of the Xapian database object
//! \param[in,out] cmd Remove command
//! \param[in] my_hashes Hashes to check for duplicates when removing documents
//! \return Info string after remove database operation
// *****************************************************************************
{
  trim( cmd );
  MDEBUG( "db rm " + cmd );
  assert( not author.empty() );
  if (not cmd.empty()) {
    auto h = tokenize( cmd );
    std::unordered_set< std::string > hashes_to_delete( begin(h), end(h) );
    return db_rm_docs( author, db_name, hashes_to_delete, my_hashes );
  }
  return "unknown cmd";
}

std::string
piac::db_list( const std::string& db_name, std::string&& cmd )
// *****************************************************************************
//  List Xapian database
//! \param[in] db_name Name of the Xapian database object
//! \param[in,out] cmd List command
//! \return List of items queried from database
// *****************************************************************************
{
  trim( cmd );
  MDEBUG( "db list " + cmd );

  if (cmd.empty()) {

    auto docs = db_list_doc( db_name );
    std::string result( "Number of documents: " +
                        std::to_string( docs.size() ) + '\n' );
    for (auto&& d : docs) result += std::move(d) + '\n';
    result.pop_back();
    return result;

  } else if (cmd[0]=='n' && cmd[1]=='u' && cmd[2]=='m' && cmd[3]=='d' &&
             cmd[4]=='o' && cmd[5]=='c')
  {

    return "Number of documents: " + std::to_string( get_doccount( db_name ) );

  } else if (cmd[0]=='n' && cmd[1]=='u' && cmd[2]=='m' && cmd[3]=='u' &&
             cmd[4]=='s' && cmd[5]=='r')
  {

    return "Number of users: " + std::to_string( db_list_numuser( db_name ) );

  } else if (cmd[0]=='h' && cmd[1]=='a' && cmd[2]=='s' && cmd[3]=='h') {

    cmd.erase( 0, 5 );
    auto hashes = db_list_hash( db_name, /* inhex = */ true );
    std::string result( "Number of documents: " +
                        std::to_string( hashes.size() ) + '\n' );
    for (auto&& h : hashes) result += std::move(h) + '\n';
    result.pop_back();
    return result;

  }

  return "unknown cmd";
}
