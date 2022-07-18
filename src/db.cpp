#include <cryptopp/sha.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>

#include "logging.hpp"
#include "db.hpp"

// ****************************************************************************
std::string
piac::sha256( const std::string& msg ) {
  using namespace CryptoPP;

  std::string digest;
  SHA256 hash;
  hash.Update(( const byte*)msg.data(), msg.size() );
  digest.resize( hash.DigestSize() );
  hash.Final( (byte*)&digest[0] );

  std::string result;
  HexEncoder encoder( new StringSink( result ) );
  StringSource( digest, true, new Redirector( encoder ) );

  return result;
}

// ****************************************************************************
Xapian::doccount
piac::get_doccount( const std::string db_name ) {
  try {
    Xapian::Database db( db_name );
    return db.get_doccount();
  } catch ( const Xapian::Error &e ) {
    NLOG(ERROR) << e.get_description();
  }
  return {};
}

// ****************************************************************************
std::string
piac::add_document( Xapian::TermGenerator& indexer,
                    Xapian::WritableDatabase& db,
                    Document& ndoc )
{
  Xapian::Document doc;
  indexer.set_document( doc );
  // Index each field with a suitable prefix
  indexer.index_text( ndoc.title(), 1, "S" );
  indexer.index_text( ndoc.author(), 1, "A" );
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
  indexer.index_text( ndoc.author() );
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
  doc.add_value( XapianValues::PRICE, std::to_string( ndoc.price() ) );
  // Generate a hash of the above fields and store it in the document
  auto entry = ndoc.serialize();
  ndoc.sha( sha256( entry ) );
  auto sha = ndoc.sha();
  // Ensure each object ends up in the database only once no matter how
  // many times we run the indexer
  doc.add_boolean_term( std::to_string( ndoc.id() ) );
  doc.set_data( entry );
  doc.add_term( sha );
  // Add Xapian doc to db
  db.replace_document( sha, doc );
  return sha;
}

// ****************************************************************************
std::size_t
piac::index_db( const std::string& db_name, const std::string& input_filename )
{
  if (input_filename.empty()) return 0;
  NLOG(DEBUG) << "Indexing " << input_filename;
  Xapian::WritableDatabase db( db_name, Xapian::DB_CREATE_OR_OPEN );
  Xapian::TermGenerator indexer;
  Xapian::Stem stemmer( "english" );
  indexer.set_stemmer( stemmer );
  indexer.set_stemming_strategy( indexer.STEM_SOME_FULL_POS );
  try {

    // Read json db from file
    Documents ndoc;
    ndoc.deserializeFromFile( input_filename );
    // Insert all documents from json into xapian db
    for (auto& d : ndoc.documents()) add_document( indexer, db, d );

    NLOG(INFO) << "Finished indexing " << ndoc.documents().size()
               << " entries, commit to db";
    // Explicitly commit so that we get to see any errors. WritableDatabase's
    // destructor will commit implicitly (unless we're in a transaction) but
    // will swallow any exceptions produced.
    db.commit();
    return ndoc.documents().size();

  } catch ( const Xapian::Error &e ) {
    NLOG(ERROR) << e.get_description();
  }
  return 0;
}

// *****************************************************************************
std::string
piac::db_query( const std::string& db_name, std::string&& cmd ) {
  try {

    NLOG(DEBUG) << "db query: '" << cmd << "'";
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
    NLOG(DEBUG) << "parsed query: '" << query.get_description() << "'";
    // Find the top 10 results for the query
    enquire.set_query( query );
    NLOG(DEBUG) << "set query: '" << query.get_description() << "'";
    Xapian::MSet matches = enquire.get_mset( 0, 10 );
    NLOG(DEBUG) << "got matches";
    // Construct the results
    auto nr = matches.get_matches_estimated();
    NLOG(DEBUG) << "got estimated matches: " << nr;
    std::stringstream result;
    result << nr << " results found.";
    if (nr) {
      result << "\nmatches 1-" << matches.size() << ":\n\n";
      for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i) {
        NLOG(DEBUG) << "getting match: " << i.get_rank();
        result << i.get_rank() + 1 << ": " << i.get_weight() << " docid=" << *i
                 << " [" << i.get_document().get_data() << "]\n";
      }
    }
    NLOG(DEBUG) << "results: " + result.str();
    return result.str();

  } catch ( const Xapian::Error &e ) {
    NLOG(ERROR) << e.get_description();
  }
  return {};
}

// *****************************************************************************
std::string
piac::db_list_hash( const std::string& db_name ) {
  try {

    Xapian::Database db( db_name );
    Xapian::doccount dbsize = db.get_doccount();

    std::stringstream result;
    result << "Number of documents: " << dbsize;
    if (dbsize == 0) return result.str();

    for (auto it = db.postlist_begin({}); it != db.postlist_end({}); ++it) {
      auto entry = db.get_document( *it ).get_data();
      result << '\n' << sha256( entry );
    }
    return result.str();

  } catch ( const Xapian::Error &e ) {
    NLOG(ERROR) << e.get_description();
  }
  return {};
}

// *****************************************************************************
std::string
piac::db_add( const std::string& db_name, std::string&& cmd ) {
  NLOG(DEBUG) << "db add: '" << cmd << "'";
  if (cmd[0]=='j' && cmd[1]=='s' && cmd[2]=='o' && cmd[3]=='n') {
    cmd.erase( 0, 5 );
    NLOG(DEBUG) << "Add json file: '" << cmd << "' to db";
    return "Added " + std::to_string( index_db( db_name, cmd.c_str() ) ) +
           " entries";
  }
  return {};
}

// *****************************************************************************
std::string
piac::db_list( const std::string& db_name, std::string&& cmd ) {
  NLOG(DEBUG) << "db list: '" << cmd << "'";
  if (cmd[0]=='h' && cmd[1]=='a' && cmd[2]=='s' && cmd[3]=='h') {
    cmd.erase( 0, 5 );
    return db_list_hash( db_name );
  }
  return {};
}
