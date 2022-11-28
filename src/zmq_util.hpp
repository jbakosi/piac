// *****************************************************************************
/*!
  \file      src/zmq_util.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac ZeroMQ utilities
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
  #pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <zmqpp/zmqpp.hpp>

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include <zmqpp/curve.hpp>

namespace piac {

//! Create ZMQ socket
zmqpp::socket
daemon_socket( zmqpp::context& ctx,
               const std::string& host,
               const std::string& rpc_server_public_key,
               const zmqpp::curve::keypair& client_keys );

//! Send message via a ZMQ socket with retries and time-out
std::string
pirate_send( const std::string& cmd,
             zmqpp::context& ctx,
             const std::string& host,
             const std::string& rpc_server_public_key,
             const zmqpp::curve::keypair& client_keys );

//! Try to bind ZMQ socket, attempting unused ports
void
try_bind( zmqpp::socket& sock, int& port, int range, bool use_strict_ports );

} // piac::
