// *****************************************************************************
/*!
  \file      src/p2p_thread.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac daemon peer-to-peer communication
*/
// *****************************************************************************

#include <mutex>
#include <condition_variable>

#include "logging_util.hpp"
#include "crypto_util.hpp"
#include "zmq_util.hpp"
#include "p2p_thread.hpp"

namespace piac {

extern std::mutex g_hashes_mtx;
extern std::condition_variable g_hashes_cv;
extern bool g_hashes_access;

} // piac::

zmqpp::socket
piac::p2p_connect_peer( zmqpp::context& ctx, const std::string& addr )
// *****************************************************************************
//  Create ZeroMQ socket and onnect to peer piac daemon
//! \param[in,out] ctx ZeroMQ socket contex
//! \param[in] addr Address (hostname or IP + port) of peer to connect to
//! \return ZeroMQ socket created
// *****************************************************************************
{
  // create socket to connect to peer
  zmqpp::socket dealer( ctx, zmqpp::socket_type::dealer );
  dealer.connect( "tcp://" + addr );
  MDEBUG( "Connecting to peer at " + addr );
  return dealer;
}

void
piac::p2p_bcast_peers(
  int p2p_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  bool& to_bcast_peers )
// *****************************************************************************
//  Broadcast to peers
//! \param[in] p2p_port Peer-to-peer port to use
//! \param[in] my_peers List of peers (address and socket) to broadcast to
//! \param[in,out] to_bcast_peers True to broadcast, false to not
// *****************************************************************************
{
  if (not to_bcast_peers) return;

  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "PEER";
    msg << std::to_string( my_peers.size() + 1 );
    msg << "localhost:" + std::to_string(p2p_port);
    for (const auto& [taddr,sock] : my_peers) msg << taddr;
    sock.send( msg );
  }

  to_bcast_peers = false;
}

void
piac::p2p_bcast_hashes(
  int p2p_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  const std::unordered_set< std::string >& my_hashes,
  bool& to_bcast_hashes )
// *****************************************************************************
//  Broadcast advertisement database hashes to peers
//! \param[in] p2p_port Peer-to-peer port to use
//! \param[in,out] my_peers List of  peers (address and socket) to broadcast to
//! \param[in] my_hashes Set of advertisement database hashes to broadcast
//! \param[in,out] to_bcast_hashes True to broadcast, false to not
// *****************************************************************************
{
  if (not to_bcast_hashes) return;

  {
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
  }

  for (auto& [addr,sock] : my_peers) {
    zmqpp::message msg;
    msg << "HASH";
    msg << "localhost:" + std::to_string(p2p_port);
    msg << std::to_string( my_hashes.size() );
    for (const auto& h : my_hashes) msg << h;
    sock.send( msg );
    MDEBUG( "Broadcasting " << my_hashes.size() << " hashes to " << addr );
  }

  to_bcast_hashes = false;
}

void
piac::p2p_send_db_requests(
  int p2p_port,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_map< std::string,
    std::unordered_set< std::string > >& db_requests,
  bool& to_send_db_requests )
// *****************************************************************************
//  Send requests for advertisement database entries to peers
//! \param[in] p2p_port Peer-to-peer port to use
//! \param[in,out] my_peers List of  peers (address and socket) to broadcast to
//! \param[in,out] db_requests Multiple ad requests from multiple peers
//! \param[in,out] to_send_db_requests True to send requests, false to not
// *****************************************************************************
{
  if (not to_send_db_requests) return;

  for (auto&& [addr,hashes] : db_requests) {
    zmqpp::message msg;
    msg << "REQ";
    msg << "localhost:" + std::to_string(p2p_port);
    msg << std::to_string( hashes.size() );
    for (const auto& h : hashes) msg << h;
    hashes.clear();
    my_peers.at( addr ).send( msg );
  }

  db_requests.clear();
  to_send_db_requests = false;
}

void
piac::p2p_answer_p2p(
  zmqpp::context& ctx_p2p,
  zmqpp::socket& db_p2p,
  zmqpp::message& msg,
  std::unordered_map< std::string, zmqpp::socket >& my_peers,
  const std::unordered_set< std::string >& my_hashes,
  std::unordered_map< std::string,
    std::unordered_set< std::string > >& db_requests,
  int p2p_port,
  bool& to_bcast_peers,
  bool& to_bcast_hashes,
  bool& to_send_db_requests )
// *****************************************************************************
//  Answer peer's request
//! \param[in,out] ctx_p2p ZMQ context used for peer-to-peer communication
//! \param[in,out] db_p2p ZMQ context used for communication with the db thread
//! \param[in,out] msg Incoming message to answer
//! \param[in,out] my_peers List of this daemon's peers (address and socket)
//! \param[in] my_hashes This daemon's set of advertisement database hashes
//! \param[in,out] db_requests Store multiple ad requests from multiple peers
//! \param[in] p2p_port Peer-to-peer port to use
//! \param[in,out] to_bcast_peers True to broadcast to peers next, false to not
//! \param[in,out] to_bcast_hashes True to broadcast hashes next, false to not
//! \param[in,out] to_send_db_requests True to send db requests next, false: not
// *****************************************************************************
{
  std::string id, cmd;
  msg >> id >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "PEER") {

    std::string size;
    msg >> size;
    std::size_t num = stoul( size );
    while (num-- != 0) {
      std::string addr;
      msg >> addr;
      if (addr != "localhost:" + std::to_string(p2p_port) &&
          my_peers.find(addr) == end(my_peers))
      {
        my_peers.emplace( addr, p2p_connect_peer( ctx_p2p, addr ) );
        to_bcast_peers = true;
        to_bcast_hashes = true;
      }
    }
    MDEBUG( "Number of peers: " << my_peers.size() );

  } else if (cmd == "HASH") {

    {
      std::unique_lock lock( g_hashes_mtx );
      g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    }
    std::string from, size;
    msg >> from >> size;
    std::size_t num = stoul( size );
    while (num-- != 0) {
      std::string hash;
      msg >> hash;
      if (my_hashes.find(hash) == end(my_hashes)) {
        db_requests[ from ].insert( hash );
        to_send_db_requests = true;
      }
    }
    auto r = db_requests.find( from );
    MDEBUG( "Recv " << (r != end(db_requests) ? r->second.size() : 0)
            << " hashes from " << from );

  } else if (cmd == "REQ") {

    std::string addr, size;
    msg >> addr >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    zmqpp::message req;
    req << "GET" << addr << size;
    while (num-- != 0) {
      std::string hash;
      msg >> hash;
      req << hash;
    }
    db_p2p.send( req );
    MDEBUG( "Will prepare " << size << " db entries for " << addr );

  } else if (cmd == "DOC") {

    {
      std::unique_lock lock( g_hashes_mtx );
      g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
    }
    std::string size;
    msg >> size;
    MDEBUG( "Recv " << size << " db entries" );
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > docs_to_insert;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      auto hash = sha256( doc );
      if (my_hashes.find(hash) == end(my_hashes)) {
        docs_to_insert.emplace_back( std::move(doc) );
      }
    }
    zmqpp::message req;
    req << "INS" << std::to_string( docs_to_insert.size() );
    for (const auto& doc : docs_to_insert) req << doc;
    db_p2p.send( req );
    MDEBUG( "Attempting to insert " << docs_to_insert.size() << " db entries" );

  } else {

    MERROR( "unknown cmd" );

  }
}

void
piac::p2p_answer_db( zmqpp::message& msg,
                     std::unordered_map< std::string, zmqpp::socket >& my_peers,
                     bool& to_bcast_hashes )
// *****************************************************************************
//  Answer request from db thread
//! \param[in,out] msg Incoming message to answer
//! \param[in,out] my_peers List of this daemon's peers (address and socket)
//! \param[in,out] to_bcast_hashes True to broadcast hashes next, false to not
// *****************************************************************************
{
  std::string cmd;
  msg >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "PUT") {

    std::string addr, size;
    msg >> addr >> size;
    MDEBUG( "Prepared " << size << " db entries for " << addr );
    std::size_t num = stoul( size );
    assert( num > 0 );
    zmqpp::message rep;
    rep << "DOC" << size;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      rep << doc;
    }
    my_peers.at( addr ).send( rep );
    MDEBUG( "Sent back " << size << " db entries to " << addr );

  } else if (cmd == "NEW") {

    to_bcast_hashes = true;

  } else {

    MERROR( "unknown cmd" );

  }
}

[[noreturn]] void
piac::p2p_thread( zmqpp::context& ctx_p2p,
                  zmqpp::context& ctx_db,
                  std::unordered_map< std::string, zmqpp::socket >& my_peers,
                  const std::unordered_set< std::string >& my_hashes,
                  int default_p2p_port,
                  int p2p_port,
                  bool use_strict_ports )
// *****************************************************************************
//  Entry point to thread to communicate with peers
//! \param[in,out] ctx_p2p ZMQ context used for peer-to-peer communication
//! \param[in,out] ctx_db ZMQ context used for communication with the db thread
//! \param[in,out] my_peers List of this daemon's peers (address and socket)
//! \param[in] my_hashes This daemon's set of advertisement database hashes
//! \param[in] default_p2p_port Port to use by default for peer communication
//! \param[in] p2p_port Port that is used for peer communication
//! \param[in] use_strict_ports True to try only the default port
// *****************************************************************************
{
  MLOG_SET_THREAD_NAME( "p2p" );
  MINFO( "p2p thread initialized" );

  // create socket that will listen to peers and bind to p2p port
  zmqpp::socket router( ctx_p2p, zmqpp::socket_type::router );
  try_bind( router, p2p_port, 10, use_strict_ports );
  MINFO( "Bound to P2P port " << p2p_port );

  // remove our address from peer list
  my_peers.erase( "localhost:" + std::to_string(p2p_port) );
  // add default peers
  for (int p = default_p2p_port; p < p2p_port; ++p)
    my_peers.emplace( "localhost:" + std::to_string( p ),
                      zmqpp::socket( ctx_p2p, zmqpp::socket_type::dealer ) );
  // initially connect to peers
  for (auto& [addr,sock] : my_peers) sock = p2p_connect_peer( ctx_p2p, addr );
  MDEBUG( "Initial number of peers: " << my_peers.size() );

  { // log initial number of hashes (populated by db thread)
    std::unique_lock lock( g_hashes_mtx );
    g_hashes_cv.wait( lock, []{ return g_hashes_access; } );
  }
  MDEBUG( "Initial number of db hashes: " << my_hashes.size() );

  // create socket to send requests for db lookups from peers
  zmqpp::socket db_p2p( ctx_db, zmqpp::socket_type::pair );
  db_p2p.connect( "inproc://db_p2p" );
  MDEBUG( "Connected to inproc:://db_p2p" );

  std::unordered_map< std::string, std::unordered_set< std::string > >
    db_requests;

  // listen to peers
  zmqpp::poller poller;
  poller.add( router );
  poller.add( db_p2p );
  bool to_bcast_peers = true;
  bool to_bcast_hashes = true;
  bool to_send_db_requests = false;

  while (1) {
    p2p_bcast_peers( p2p_port, my_peers, to_bcast_peers );
    p2p_bcast_hashes( p2p_port, my_peers, my_hashes, to_bcast_hashes );
    p2p_send_db_requests( p2p_port, my_peers, db_requests,
                          to_send_db_requests );

    if (poller.poll()) {
      if (poller.has_input( router )) {
        zmqpp::message msg;
        router.receive( msg );
        p2p_answer_p2p( ctx_p2p, db_p2p, msg, my_peers, my_hashes, db_requests,
                        p2p_port, to_bcast_peers, to_bcast_hashes,
                        to_send_db_requests );
      }
      if (poller.has_input( db_p2p )) {
        zmqpp::message msg;
        db_p2p.receive( msg );
        p2p_answer_db( msg, my_peers, to_bcast_hashes );
      }
    }
  }
}
