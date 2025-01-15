// *****************************************************************************
/*!
  \file      src/db.hpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac database functionality
*/
// *****************************************************************************

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

//! Get number of documents in Xapian database
Xapian::doccount get_doccount( const std::string db_name );

//! Add document to Xapian database
std::string
add_document( const std::string& author,
              Xapian::TermGenerator& indexer,
              Xapian::WritableDatabase& db,
              Document& ndoc );

//! Index Xapian database
std::string
index_db( const std::string& author,
          const std::string& db_name,
          const std::string& input_filename,
          const std::unordered_set< std::string >& my_hashes = {} );

//! Query Xapian database
[[nodiscard]] std::string
db_query( const std::string& db_name, std::string&& cmd );

//! Get documents from Xapian database
[[nodiscard]] std::vector< std::string >
db_get_docs( const std::string& db_name,
             const std::vector< std::string >& hashes );

//! Put documents to Xapian database
std::size_t
db_put_docs( const std::string& db_name,
             const std::vector< std::string >& docs );

//! Remove documents from Xapian database
std::string
db_rm_docs( const std::string& author,
            const std::string& db_name,
            const std::unordered_set< std::string >& hashes_to_delete,
            const std::unordered_set< std::string >& my_hashes = {} );

//! List hashes from Xapian database
[[nodiscard]] std::vector< std::string >
db_list_hash( const std::string& db_name, bool inhex );

//! List documents from Xapian database
[[nodiscard]] std::vector< std::string >
db_list_doc( const std::string& db_name );

//! List number of unique users in Xapian database
[[nodiscard]] std::size_t
db_list_numuser( const std::string& db_name );

//! Add documents to Xapian database
std::string
db_add( const std::string& author,
        const std::string& db_name,
        std::string&& cmd,
        const std::unordered_set< std::string >& my_hashes = {} );

//! Remove documents from Xapian database
std::string
db_rm( const std::string& author,
       const std::string& db_name,
       std::string&& cmd,
       const std::unordered_set< std::string >& my_hashes );

//! List Xapian database
std::string db_list( const std::string& db_name, std::string&& cmd );

} // piac::
