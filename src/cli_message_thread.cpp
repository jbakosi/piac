// *****************************************************************************
/*!
  \file      src/cli_matrix_thread.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac message thread receiving messages from matrix client
*/
// *****************************************************************************

#include "logging_util.hpp"

#include "cli_message_thread.hpp"
#include "cli_matrix_thread.hpp"

static void
msg_handle_mtx( zmqpp::message& msg, bool& to_shutdown )
// *****************************************************************************
//  Handle message from matrix client thread
//! \param[in,out] msg Incoming message to handle
//! \param[in,out] to_shutdown Set to true here if to shut down
// *****************************************************************************
{
  std::string s;
  msg >> s;

  if (s == "SHUTDOWN") {
    to_shutdown = true;
  } else if (s == "parsed_messages") {
    MINFO( "recv parsed_messages" );
  }
}

void
piac::message_thread()
// *****************************************************************************
//! Entry point to thread to receive matrix messages
// *****************************************************************************
{
  MLOG_SET_THREAD_NAME( "msg" );
  MINFO( "msg thread initialized" );

  // create socket that will receive messages from the matrix client thread
  zmqpp::socket msg_mtx( g_ctx_msg, zmqpp::socket_type::pair );
  msg_mtx.bind( "inproc://msg_mtx" );
  MDEBUG( "Bound to inproc:://msg_mtx" );

  // listen to messages
  zmqpp::poller poller;
  poller.add( msg_mtx );
  bool to_shutdown = false;
  while (not to_shutdown) {
    if (poller.poll(100)) {
      if (poller.has_input( msg_mtx )) {
        zmqpp::message msg;
        msg_mtx.receive( msg );
        msg_handle_mtx( msg, to_shutdown );
      }
    }
  }

  MINFO( "msg thread quit" );
}
