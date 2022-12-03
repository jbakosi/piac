// *****************************************************************************
/*!
  \file      src/db_thread.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac daemon interaction with database
*/
// *****************************************************************************

#include <mutex>
#include <condition_variable>

#include "db.hpp"
#include "logging_util.hpp"
#include "crypto_util.hpp"
#include "zmq_util.hpp"
#include "db_thread.hpp"

void
piac::db_update_hashes( const std::string& db_name,
                        std::unordered_set< std::string >& my_hashes )
// *****************************************************************************
//  Update advertisement database hashes
//! \param[in] db_name The name of the database to query for the hashes
//! \param[in] my_hashes Set of advertisement database hashes to update
// *****************************************************************************
{
  auto hashes = piac::db_list_hash( db_name, /* inhex = */ false );
  std::lock_guard lock( g_hashes_mtx );
  g_hashes_access = false;
  my_hashes.clear();
  for (auto&& h : hashes) my_hashes.insert( std::move(h) );
  g_hashes_access = true;
  g_hashes_cv.notify_one();
  MDEBUG( "Number of db hashes: " << my_hashes.size() );
}

void
piac::db_client_op(
  zmqpp::socket& client,
  zmqpp::socket& db_p2p,
  const std::string& db_name,
  const std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_set< std::string >& my_hashes,
  zmqpp::message& msg )
// *****************************************************************************
//  Perform a database operation for a client
//! \param[in,out] client ZMQ socket of the client
//! \param[in,out] db_p2p ZMQ socket of the daemon's p2p thread
//! \param[in] db_name The name of the database to operate on
//! \param[in] my_peers List of this daemon's peers (address and socket)
//! \param[in,out] my_hashes Set of this daemon's advertisement database hashes
//! \param[in,out] msg Incoming message to answer
// *****************************************************************************
{
  std::string cmd;
  msg >> cmd;

  // extract hash of user auth from cmd if any, remove from cmd (and log)
  std::string user;
  auto u = cmd.rfind( "AUTH:" );
  if (u != std::string::npos) {
    user = cmd.substr( u + 5 );
    cmd.erase( u - 1 );
  }

  MDEBUG( "Recv msg " << cmd );

  if (cmd == "connect") {

    zmqpp::message reply;
    reply << "accept";
    MDEBUG( "Sending reply " << cmd );
    client.send( reply );

  } else if (cmd[0]=='d' && cmd[1]=='b') {

    cmd.erase( 0, 3 );
    auto q = std::move( cmd );
    std::string reply;
    if (q[0]=='q' && q[1]=='u' && q[2]=='e' && q[3]=='r' && q[4]=='y') {

      q.erase( 0, 6 );
      reply = piac::db_query( db_name, std::move(q) );

    } else if (q[0]=='a' && q[1]=='d' && q[2]=='d') {

      q.erase( 0, 4 );
      assert( not user.empty() );
      reply = piac::db_add( user, db_name, std::move(q), my_hashes );
      MDEBUG( "Number of documents: " <<piac::get_doccount( db_name ) );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message note;
      note << "NEW";
      db_p2p.send( note );
      MDEBUG( "Sent note on new documents" );

    } else if (q[0]=='r' && q[1]=='m') {

      q.erase( 0, 3 );
      assert( not user.empty() );
      reply = piac::db_rm( user, db_name, std::move(q), my_hashes );
      MDEBUG( "Number of documents: " << piac::get_doccount( db_name ) );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message note;
      note << "NEW";
      db_p2p.send( note );
      MDEBUG( "Sent note on removed documents" );

    } else if (q[0]=='l' && q[1]=='i' && q[2]=='s' && q[3]=='t') {

      q.erase( 0, 5 );
      reply = piac::db_list( db_name, std::move(q) );

    } else {

      reply = "unknown command";

    }
    client.send( reply );

  } else if (cmd == "peers") {

    zmqpp::message reply;
    if (not my_peers.empty()) {
      std::stringstream peers_list;
      for (const auto& [addr,sock] : my_peers) peers_list << addr << ' ';
      reply << peers_list.str();
    } else {
      reply << "No peers";
    }
    client.send( reply );

  } else {

    MERROR( "unknown command" );
    client.send( "unknown command" );

  }
}

void
piac::db_peer_op( const std::string& db_name,
                  zmqpp::message& msg,
                  zmqpp::socket& db_p2p,
                  std::unordered_set< std::string >& my_hashes )
// *****************************************************************************
//  Perform an operation for a peer
//! \param[in] db_name The name of the database to operate on
//! \param[in,out] msg Incoming message to answer
//! \param[in,out] db_p2p ZMQ socket of the daemon's p2p thread
//! \param[in,out] my_hashes Set of this daemon's advertisement database hashes
// *****************************************************************************
{
  std::string cmd;
  msg >> cmd;
  MDEBUG( "Recv msg: " << cmd );

  if (cmd == "GET") {

    std::string addr, size;
    msg >> addr >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > hashes;
    while (num-- != 0) {
      std::string h;
      msg >> h;
      hashes.emplace_back( std::move(h) );
    }

    auto docs = piac::db_get_docs( db_name, hashes );
    MDEBUG( "Looked up " << docs.size() << " hashes" );

    zmqpp::message reply;
    reply << "PUT" << addr << std::to_string( docs.size() );
    for (const auto& d : docs) reply << d;
    db_p2p.send( reply );
    MDEBUG( "Sending " << docs.size() << " entries" );

  } else if (cmd == "INS") {

    std::string size;
    msg >> size;
    std::size_t num = stoul( size );
    assert( num > 0 );
    std::vector< std::string > docs;
    while (num-- != 0) {
      std::string doc;
      msg >> doc;
      auto hash = sha256( doc );
      if (my_hashes.find(hash) == end(my_hashes)) {
        docs.emplace_back( std::move(doc) );
      }
    }
    if (not docs.empty()) {
      piac::db_put_docs( db_name, docs );
      MDEBUG(  "Inserted " << docs.size() << " entries to db" );
      auto ndoc = piac::get_doccount( db_name );
      MDEBUG( "Number of documents: " << ndoc );
      db_update_hashes( db_name, my_hashes );
      zmqpp::message reply;
      reply << "NEW";
      db_p2p.send( reply );
      MDEBUG( "Sent note on new documents" );
    }

  } else {

    MERROR( "unknown cmd" );

  }
}

[[noreturn]] void
piac::db_thread(
  zmqpp::context& ctx_db,
  const std::string& db_name,
  int rpc_port,
  bool use_strict_ports,
  const std::unordered_map< std::string, zmqpp::socket >& my_peers,
  std::unordered_set< std::string >& my_hashes,
  int rpc_secure,
  const zmqpp::curve::keypair& rpc_server_keys,
  const std::vector< std::string >& rpc_authorized_clients )
// *****************************************************************************
//  Entry point to thread to perform database operations
//! \param[in,out] ctx_db ZMQ context used for communication with the db thread
//! \param[in] db_name The name of the database to operate on
//! \param[in] rpc_port Port to use for client communication
//! \param[in] use_strict_ports True to try only the default port
//! \param[in] my_peers List of this daemon's peers (address and socket)
//! \param[in,out] my_hashes Set of this daemon's advertisement database hashes
//! \param[in] rpc_secure Non-zero to use secure client communication
//! \param[in] rpc_server_keys CurveMQ keypair to use for secure client comm.
//! \param[in] rpc_authorized_clients Only communicate with these clients if
//!    secure communication is used to talk to clients
//! \see http://curvezmq.org
//! \see http://www.evilpaul.org/wp/2017/05/02/authentication-encryption-zeromq
// *****************************************************************************
{
  MLOG_SET_THREAD_NAME( "db" );
  MINFO( "db thread initialized" );
  MINFO( "Using database: " << db_name );

  // initially optionally populate database
  auto ndoc = piac::get_doccount( db_name );
  MINFO( "Initial number of documents: " << ndoc );

  // initially query hashes of db entries
  db_update_hashes( db_name, my_hashes );

  zmqpp::context ctx_rpc;

  // configure secure socket that will listen to clients and bind to RPC port
  zmqpp::auth authenticator( ctx_rpc );
  authenticator.set_verbose( true );
  authenticator.configure_domain( "*" );
  authenticator.allow( "127.0.0.1" );
  if (rpc_secure) {
    if (rpc_authorized_clients.empty()) {    // stonehouse
      authenticator.configure_curve( "CURVE_ALLOW_ANY" );
    } else {                                  // ironhouse
      for (const auto& cpk : rpc_authorized_clients) {
        authenticator.configure_curve( cpk );
      }
    }
  }
  // create socket that will listen to clients via RPC
  zmqpp::socket client( ctx_rpc, zmqpp::socket_type::reply );
  if (rpc_secure) {
    client.set( zmqpp::socket_option::identity, "IDENT" );
    int as_server = 1;
    client.set( zmqpp::socket_option::curve_server, as_server );
    client.set( zmqpp::socket_option::curve_secret_key,
                rpc_server_keys.secret_key );
  }
  try_bind( client, rpc_port, 10, use_strict_ports );

  MINFO( "Bound to RPC port " << rpc_port );
  epee::set_console_color( epee::console_color_yellow, /* bright = */ false );
  std::cout << "Bound to RPC port " << rpc_port << '\n';
  epee::set_console_color( epee::console_color_default, /* bright = */ false );

  // create socket that will listen to requests for db lookups from peers
  zmqpp::socket db_p2p( ctx_db, zmqpp::socket_type::pair );
  db_p2p.bind( "inproc://db_p2p" );
  MDEBUG( "Bound to inproc:://db_p2p" );

  // listen to messages
  zmqpp::poller poller;
  poller.add( db_p2p );
  while (1) {

    zmqpp::message msg;
    if (client.receive( msg, /* dont_block = */ true )) {
      db_client_op( client, db_p2p, db_name, my_peers, my_hashes, msg );
    }

    if (poller.poll(100)) {
      if (poller.has_input( db_p2p )) {
        zmqpp::message m;
        db_p2p.receive( m );
        db_peer_op( db_name, m, db_p2p, my_hashes );
      }
    }
  }
}
