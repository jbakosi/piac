// *****************************************************************************
/*!
  \file      src/daemon_p2p_thread.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac daemon peer-to-peer communication
*/
// *****************************************************************************

#pragma once

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

namespace piac {

//! Create ZeroMQ socket and onnect to peer piac daemon
zmqpp::socket
p2p_connect_peer( zmqpp::context& ctx, const std::string& addr );


//! Broadcast to peers
void
p2p_bcast_peers( int p2p_port,
                 std::unordered_map< std::string, zmqpp::socket >& my_peers,
                 bool& to_bcast_peers );

//! Broadcast advertisement database hashes to peers
void
p2p_bcast_hashes( int p2p_port,
                  std::unordered_map< std::string, zmqpp::socket >& my_peers,
                  const std::unordered_set< std::string >& my_hashes,
                  bool& to_bcast_hashes );

//! Send requests for advertisement database entries to peers
void
p2p_send_db_requests(
  int p2p_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_map< std::string,
    std::unordered_set< std::string > >& db_requests,
  bool& to_send_db_requests );

//! Answer peer's request
void
p2p_answer_p2p( zmqpp::context& ctx_p2p,
                zmqpp::socket& db_p2p,
                zmqpp::message& msg,
                std::unordered_map< std::string, zmqpp::socket >& my_peers,
                const std::unordered_set< std::string >& my_hashes,
                std::unordered_map< std::string,
                  std::unordered_set< std::string > >& db_requests,
                int p2p_port,
                bool& to_bcast_peers,
                bool& to_bcast_hashes,
                bool& to_send_db_requests );

//! Answer request from db thread
void
p2p_answer_db( zmqpp::message& msg,
               std::unordered_map< std::string, zmqpp::socket >& my_peers,
               bool& to_bcast_hashes );

//! Entry point to thread to communicate with peers
[[noreturn]] void
p2p_thread( zmqpp::context& ctx_p2p,
            zmqpp::context& ctx_db,
            std::unordered_map< std::string, zmqpp::socket >& my_peers,
            const std::unordered_set< std::string >& my_hashes,
            int default_p2p_port,
            int p2p_port,
            bool use_strict_ports );

} // ::piac
