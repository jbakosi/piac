#pragma once

#include <vector>
#include <unordered_set>

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-W#warnings"
#endif

#include "xapian.h"

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include "document.hpp"

namespace piac {

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
