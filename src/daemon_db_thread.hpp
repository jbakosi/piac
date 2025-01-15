// *****************************************************************************
/*!
  \file      src/daemon_db_thread.hpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac daemon interaction with database
*/
// *****************************************************************************

#pragma once

#include <string>
#include <unordered_set>

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wundef"
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
  #pragma clang diagnostic ignored "-Wdocumentation"
  #pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include <zmqpp/zmqpp.hpp>

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <zmqpp/curve.hpp>

namespace piac {

extern std::mutex g_hashes_mtx;
extern std::condition_variable g_hashes_cv;
extern bool g_hashes_access;

//! Update advertisement database hashes
void
db_update_hashes( const std::string& db_name,
                  std::unordered_set< std::string >& my_hashes );

//! Perform a database operation for a client
void
db_client_op( zmqpp::socket& client,
              zmqpp::socket& db_p2p,
              const std::string& db_name,
              const std::unordered_map< std::string, zmqpp::socket >& my_peers,
              std::unordered_set< std::string >& my_hashes,
              zmqpp::message& msg );

//! Perform an operation for a peer
void
db_peer_op( const std::string& db_name,
            zmqpp::message& msg,
            zmqpp::socket& db_p2p,
            std::unordered_set< std::string >& my_hashes );

//! Entry point to thread to perform database operations
[[noreturn]] void
db_thread( zmqpp::context& ctx_db,
           const std::string& db_name,
           int rpc_port,
           bool use_strict_ports,
           const std::unordered_map< std::string, zmqpp::socket >& my_peers,
           std::unordered_set< std::string >& my_hashes,
           int rpc_secure,
           const zmqpp::curve::keypair& rpc_server_keys,
           const std::vector< std::string >& rpc_authorized_clients );

} // ::piac
