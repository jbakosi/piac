// *****************************************************************************
/*!
  \file      src/cli_message_thread.hpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac message thread receiving messages from matrix client
*/
// *****************************************************************************
#pragma once

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

//! Entry point to thread to receive matrix messages
void message_thread();

} // piac::
