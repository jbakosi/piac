// *****************************************************************************
/*!
  \file      src/zmq_util.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac ZeroMQ utilities
*/
// *****************************************************************************

#include <string>
#include <unistd.h>

#include "zmq_util.hpp"
#include "logging_util.hpp"

#define REQUEST_TIMEOUT     3000   //  msecs, (> 1000!)
#define REQUEST_RETRIES     5      //  before we abandon trying to send

zmqpp::socket
piac::daemon_socket( zmqpp::context& ctx,
                     const std::string& host,
                     const std::string& rpc_server_public_key,
                     const zmqpp::curve::keypair& client_keys )
// *****************************************************************************
//  Create ZMQ socket
//! \param[in,out] ctx ZeroMQ socket context
//! \param[in] host Hostname or IP + port to send cmd to
//! \param[in] rpc_server_public_key CurveZMQ server public key to use
//! \param[in] client_keys CurveMQ client keypair to use
//! \return ZMQ socket created
//! \see http://curvezmq.org
// *****************************************************************************
{
  MINFO( "Connecting to " << host );
  auto sock = zmqpp::socket( ctx, zmqpp::socket_type::req );

  if (not rpc_server_public_key.empty()) {
    try {
      sock.set( zmqpp::socket_option::curve_server_key, rpc_server_public_key );
      sock.set( zmqpp::socket_option::curve_public_key, client_keys.public_key );
      sock.set( zmqpp::socket_option::curve_secret_key, client_keys.secret_key );
    } catch ( zmqpp::exception& ) {}
  }

  sock.connect( "tcp://" + host );
  //  configure socket to not wait at close time
  sock.set( zmqpp::socket_option::linger, 0 );
  return sock;
}

std::string
piac::pirate_send( const std::string& cmd,
                   zmqpp::context& ctx,
                   const std::string& host,
                   const std::string& rpc_server_public_key,
                   const zmqpp::curve::keypair& client_keys )
// *****************************************************************************
//  Send message via a ZMQ socket with retries and time-out
//! \param[in] cmd Command to send
//! \param[in,out] ctx ZeroMQ socket context
//! \param[in] host Hostname or IP + port to send cmd to
//! \param[in] rpc_server_public_key CurveZMQ server public key to use
//! \param[in] client_keys CurveMQ client keypair to use
//! \return Response from remote host
//! \see https://zguide.zeromq.org/docs/chapter4/#Client-Side-Reliability-Lazy-Pirate-Pattern
//! \see https://github.com/zeromq/libzmq/issues/3495
// *****************************************************************************
{
  std::string reply;
  int retries = REQUEST_RETRIES;
  auto server = daemon_socket( ctx, host, rpc_server_public_key, client_keys );

  // send cmd from a Lazy Pirate client
  while (retries) {
    std::string request = cmd;
    server.send( request );
    if (retries != REQUEST_RETRIES) sleep(1);

    zmqpp::poller poller;
    poller.add( server );
    bool expect_reply = true;
    while (expect_reply) {
      if (poller.poll(REQUEST_TIMEOUT) && poller.has_input(server)) {
        server.receive( reply );
        if (not reply.empty()) {
          MDEBUG( "Recv reply: " + reply );
          expect_reply = false;
          retries = 0;
        } else {
          MERROR( "Recv empty msg from daemon" );
        }
      } else if (--retries == 0) {
        MERROR( "Abandoning server at " + host );
        reply = "No response from server";
        expect_reply = false;
      } else {
        MWARNING( "No response from " + host + ", retrying: " << retries );
        poller.remove( server );
        server = daemon_socket( ctx, host, rpc_server_public_key, client_keys );
        poller.add( server );
        server.send( request );
      }
    }
  }

  MDEBUG( "Destroyed socket to " + host );
  return reply;
}

void
piac::try_bind( zmqpp::socket& sock, int& port, int range, bool use_strict_ports )
// *****************************************************************************
//  Try to bind ZMQ socket, attempting unused ports
//! \param[in,out] sock Socket to use
//! \param[in,out] port Port to try first
//! \param[in] range Number of ports to attempt in increasing order: port+range
//! \param[in] use_strict_ports True to try only the default port
// *****************************************************************************
{
  if (use_strict_ports) {
    sock.bind( "tcp://*:" + std::to_string(port) );
    return;
  }

  do {
    try {
      sock.bind( "tcp://*:" + std::to_string(port) );
      return;
    }
    catch ( zmqpp::exception& ) {}
    ++port;
  } while (port < port+range );

  MERROR( "Could not bind to socket within range: ["
          << port << ',' << port+range << ')' );
}
