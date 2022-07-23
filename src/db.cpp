#include <string>

#include "util.hpp"
#include "db.hpp"

// ****************************************************************************
Xapian::doccount
piac::get_doccount( const std::string db_name ) {
  try {
    Xapian::Database db( db_name );
    return db.get_doccount();
  } catch ( const Xapian::Error &e ) {
    MWARNING( e.get_description() );
  }
  return {};
}

// ****************************************************************************
std::string
piac::add_document( const std::string& author,
                    Xapian::TermGenerator& indexer,
                    Xapian::WritableDatabase& db,
                    Document& ndoc )
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
  doc.add_value( XapianValues::PRICE, std::to_string( ndoc.price() ) );
  // Generate a hash of the doc fields and store it in the document
  auto entry = ndoc.serialize();
  ndoc.author( author );
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

// ****************************************************************************
std::size_t
piac::index_db( const std::string& author,
                const std::string& db_name,
                const std::string& input_filename,
                const std::unordered_set< std::string >& my_hashes )
{
  assert( not author.empty() );

  std::ifstream f( input_filename );
  if (not f.good()) {
    MERROR( "Cannot open database input file: " + input_filename );
    return 0;
  }

  MDEBUG( "Indexing " << input_filename );
  Xapian::WritableDatabase db( db_name, Xapian::DB_CREATE_OR_OPEN );
  Xapian::TermGenerator indexer;
  Xapian::Stem stemmer( "english" );
  indexer.set_stemmer( stemmer );
  indexer.set_stemming_strategy( indexer.STEM_SOME_FULL_POS );
  try {

    // Read json db from file
    Documents ndoc;
    ndoc.deserializeFromFile( input_filename );
    // Insert documents we do not yet have from json into xapian db
    std::size_t numins = 0;
    for (auto& d : ndoc.documents()) {
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
    return numins;

  } catch ( const Xapian::Error &e ) {
    MERROR( e.get_description() );
  }
  return 0;
}

// *****************************************************************************
std::string
piac::db_query( const std::string& db_name, std::string&& cmd ) {
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

// *****************************************************************************
std::vector< std::string >
piac::db_get_docs( const std::string& db_name,
                   const std::vector< std::string >& hashes )
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

// *****************************************************************************
std::size_t
piac::db_put_docs( const std::string& db_name,
                   const std::vector< std::string >& docs )
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

// *****************************************************************************
std::vector< std::string >
piac::db_list_hash( const std::string& db_name, bool inhex ) {
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

// *****************************************************************************
std::string
piac::db_add( const std::string& author,
              const std::string& db_name,
              std::string&& cmd,
              const std::unordered_set< std::string >& my_hashes )
{
  MDEBUG( "db add: '" << cmd << "'" );
  assert( not author.empty() );
  if (cmd[0]=='j' && cmd[1]=='s' && cmd[2]=='o' && cmd[3]=='n') {
    cmd.erase( 0, 5 );
    trim( cmd );
    MDEBUG( "Add json file: '" << cmd << "' to db" );
    auto numins = index_db( author, db_name, cmd, my_hashes );
    return "Added " + std::to_string( numins ) + " entries";
  }
  return {};
}

// *****************************************************************************
std::string
piac::db_list( const std::string& db_name, std::string&& cmd ) {
  MDEBUG( "db list: '" << cmd << "'" );
  if (cmd[0]=='h' && cmd[1]=='a' && cmd[2]=='s' && cmd[3]=='h') {
    cmd.erase( 0, 5 );
    auto hashes = db_list_hash( db_name, /* inhex = */ true );
    std::string result( "Number of documents: " +
                        std::to_string( hashes.size() ) + '\n' );
    for (auto&& h : hashes) result += std::move(h) + '\n';
    result.pop_back();
    return result;
  }
  return {};
}
