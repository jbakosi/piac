// *****************************************************************************
/*!
  \file      src/cli_matrix_thread.hpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac matrix client functionality
*/
// *****************************************************************************
#pragma once

#include <string>

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

extern uint16_t g_matrix_sync_timeout;
extern bool g_matrix_connected;
extern bool g_matrix_shutdown;
extern zmqpp::context g_ctx_msg;

//! Send a message to a user
void matrix_message( std::string usr,
                     std::string ad,
                     std::string msg );

//! Entry point to thread to communicate with a matrix server
void matrix_thread( const std::string& server,
                    const std::string& username,
                    const std::string& password,
                    const std::string& db_key );

} // piac::
