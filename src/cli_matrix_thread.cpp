// *****************************************************************************
/*!
  \file      src/cli_matrix_thread.cpp
  \copyright 2022-2025 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac matrix client functionality
*/
// *****************************************************************************

#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpadded"
  #pragma clang diagnostic ignored "-Wextra-semi"
  #pragma clang diagnostic ignored "-Wweak-vtables"
  #pragma clang diagnostic ignored "-Wold-style-cast"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#endif

#include "mtxclient/http/client.hpp"
#include "mtxclient/crypto/client.hpp"
#include "mtx/responses/sync.hpp"
#include "mtx/responses/common.hpp"
#include "mtx/responses/create_room.hpp"
#include "mtx/events.hpp"
#include "mtx/events/encrypted.hpp"

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

#include "cli_matrix_thread.hpp"
#include "logging_util.hpp"
#include "string_util.hpp"

constexpr auto OLM_ALGO = "m.olm.v1.curve25519-aes-sha2";
static std::shared_ptr< mtx::http::Client> g_mtxclient = nullptr;
static std::shared_ptr< mtx::crypto::OlmClient> g_olmclient = nullptr;
static std::string g_matrix_user;
static std::string g_mtx_account_db_filename;
static std::string g_mtx_session_db_filename;
static std::string g_mtx_db_storage_key;
static std::vector< zmqpp::socket > g_msg_mtx;

uint16_t piac::g_matrix_sync_timeout = 10'000;     // milliseconds
bool piac::g_matrix_connected = false;
bool piac::g_matrix_shutdown = false;
zmqpp::context piac::g_ctx_msg;

struct OutboundSessionData {
  std::string session_id;
  std::string session_key;
  uint64_t message_index = 0;
};

static inline void
to_json( nlohmann::json& obj, const OutboundSessionData& msg ) 
// *****************************************************************************
//! Serialize OutboundSessionData to json format
//! \param[in,out] obj nlohmann::json object to write to
//! \param[in] msg OutboundSessionData message to read from
// *****************************************************************************
{
  obj[ "session_id" ] = msg.session_id;
  obj[ "session_key" ] = msg.session_key;
  obj[ "message_index" ] = msg.message_index;
}

static inline void
from_json( const nlohmann::json& obj, OutboundSessionData& msg )
// *****************************************************************************
//! Deserialize OutboundSessionData from json format
//! \param[in] obj nlohmann::json object to read from
//! \param[in,out] msg OutboundSessionData message to write to
// *****************************************************************************
{
  msg.session_id = obj.at( "session_id" ).get< std::string >();
  msg.session_key = obj.at( "session_key" ).get< std::string >();
  msg.message_index = obj.at( "message_index" ).get< uint64_t >();
}

struct OutboundSessionDataRef {
  OlmOutboundGroupSession* session;
  OutboundSessionData data;
};

struct DevKeys {
  std::string ed25519;
  std::string curve25519;
};

static inline void
to_json( nlohmann::json& obj, const DevKeys& msg ) 
// *****************************************************************************
//! Serialize DevKeys to json format
//! \param[in,out] obj nlohmann::json object to write to
//! \param[in] msg DevKeys message to read from
// *****************************************************************************
{
  obj[ "ed25519" ] = msg.ed25519;
  obj[ "curve25519" ] = msg.curve25519;
}

static inline void
from_json( const nlohmann::json& obj, DevKeys& msg )
// *****************************************************************************
//! Deserialize DevKeys from json format
//! \param[in] obj nlohmann::json object to read from
//! \param[in,out] msg DevKeys message to write to
// *****************************************************************************
{
  msg.ed25519 = obj.at( "ed25519" ).get< std::string >();
  msg.curve25519 = obj.at( "curve25519" ).get< std::string >();
}

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpadded"
#endif

struct OlmCipherContent {
  std::string body;
  uint8_t type;
};

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

inline void from_json(const nlohmann::json& obj, OlmCipherContent& msg )
// *****************************************************************************
//! Deserialize OlmCipherContent from json format
//! \param[in] obj nlohmann::json object to read from
//! \param[in,out] msg OlmCipherContent to write to
// *****************************************************************************
{
  msg.body = obj.at("body").get< std::string >();
  msg.type = obj.at("type").get< uint8_t >();
}

struct OlmMessage {
  std::string sender_key;
  std::string sender;
  using RecipientKey = std::string;
  std::map< RecipientKey, OlmCipherContent > ciphertext;
};

inline void from_json( const nlohmann::json& obj, OlmMessage& msg )
// *****************************************************************************
//! Deserialize OlmMessage from json format
//! \param[in] obj nlohmann::json object to read from
//! \param[in,out] msg OlmMessage to write to
// *****************************************************************************
{
  if (obj.at("type") != "m.room.encrypted") {
    throw std::invalid_argument( "invalid type for olm message" );
  }

  if (obj.at("content").at("algorithm") != OLM_ALGO)
    throw std::invalid_argument("invalid algorithm for olm message");

  msg.sender = obj.at("sender").get< std::string >();
  msg.sender_key = obj.at("content").at("sender_key").get< std::string >();
  msg.ciphertext = obj.at("content").at("ciphertext").
   get< std::map< std::string, OlmCipherContent > >();
}

struct Storage {

  using OlmSessionPtr = mtx::crypto::OlmSessionPtr;
  using InboundGroupSessionPtr = mtx::crypto::InboundGroupSessionPtr;
  using OutboundGroupSessionPtr = mtx::crypto::OutboundGroupSessionPtr;

  //! This device id
  std::string device_id;
  //! Access token
  std::string access_token;
  //! Storage for the user_id -> list of devices mapping
  std::map< std::string, std::vector< std::string > > devices;
  //! Storage for the identity key for a device.
  std::map< std::string, DevKeys > device_keys;
  //! Flag that indicate if a specific room has encryption enabled.
  std::map< std::string, bool > encrypted_rooms;
  //! Keep track of members per room.
  std::map< std::string, std::map< std::string, bool > > members;

  void add_member( const std::string& room_id, const std::string& user_id ) {
    members[ room_id ][ user_id ] = true;
  }

  std::string in_room( const std::string& usr1, const std::string& usr2 ) {
    for (const auto& [room_id, users] : members)
      if (users.find(usr1) != end(users) && users.find(usr2) != end(users))
        return room_id;
    return {};
  }

  //! Mapping from curve25519 to session
  std::map< std::string, OlmSessionPtr > olm_inbound_sessions;
  std::map< std::string, OlmSessionPtr > olm_outbound_sessions;

  std::map< std::string, InboundGroupSessionPtr > inbound_group_sessions;
  std::map< std::string, OutboundSessionData > outbound_group_session_data;
  std::map< std::string, OutboundGroupSessionPtr > outbound_group_sessions;

  bool outbound_group_exists( const std::string& room_id ) {
    return
      outbound_group_sessions.find( room_id ) !=
        end( outbound_group_sessions ) &&
      outbound_group_session_data.find( room_id ) !=
        end( outbound_group_session_data );
  }

  void set_outbound_group_session( const std::string& room_id,
                                   OutboundGroupSessionPtr session,
                                   OutboundSessionData data )
  {
    outbound_group_session_data[ room_id ] = data;
    outbound_group_sessions[ room_id ] = std::move( session );
  }

  OutboundSessionDataRef
  get_outbound_group_session( const std::string& room_id ) {
    return { outbound_group_sessions[ room_id ].get(),
             outbound_group_session_data[ room_id ] };
  }

  bool inbound_group_exists( const std::string& room_id,
                             const std::string& session_id,
                             const std::string& sender_key )
  {
    const auto key = room_id + session_id + sender_key;
    return inbound_group_sessions.find( key ) != end(inbound_group_sessions);
  }

  void set_inbound_group_session( const std::string& room_id,
                                  const std::string& session_id,
                                  const std::string& sender_key,
                                  InboundGroupSessionPtr session )
  {
    const auto key = room_id + session_id + sender_key;
    inbound_group_sessions[ key ] = std::move( session );
  }

  OlmInboundGroupSession*
    get_inbound_group_session( const std::string& room_id,
                               const std::string& session_id,
                              const std::string& sender_key )
  {
    const auto key = room_id + session_id + sender_key;
    return inbound_group_sessions[ key ].get();
  }

  void load() {
    using namespace std;
    using namespace mtx::crypto;
    MDEBUG( "matrix session restoring storage" );

    ifstream db( g_mtx_session_db_filename );
    if (not db.is_open()) {
      MINFO( "couldn't open matrix session db " << g_mtx_session_db_filename );
      return;
    }
    string db_data( (istreambuf_iterator< char >( db )),
                     istreambuf_iterator< char >() );

    if (db_data.empty()) return;

    nlohmann::json obj = nlohmann::json::parse( db_data );

    try {
      device_id = obj.at( "device_id" ).get< string >();
      access_token = obj.at( "access_token" ).get< string >();
    } catch ( const nlohmann::json::exception& e ) {
      MWARNING( "json parsing error: " << e.what() );
    }

    devices = obj.at( "devices" ).get< map< string, vector< string > > >();
    device_keys = obj.at( "device_keys" ).get< map< string, DevKeys > >();
    encrypted_rooms = obj.at( "encrypted_rooms" ).get< map< string, bool> >();
    members = obj.at( "members" ).get< map< string, map< string, bool > > >();

    if (obj.count( "olm_inbound_sessions" ) != 0) {
      auto sessions = obj.at( "olm_inbound_sessions" ).get<map<string, string>>();
      for (const auto &s : sessions)
        olm_inbound_sessions[ s.first ] =
          unpickle< SessionObject >( s.second, g_mtx_db_storage_key );
    }

    if (obj.count( "olm_outbound_sessions" ) != 0) {
      auto sessions = obj.at("olm_outbound_sessions").get<map<string,string>>();
      for (const auto &s : sessions)
        olm_outbound_sessions[ s.first ] =
          unpickle< SessionObject >( s.second, g_mtx_db_storage_key );
    }

    if (obj.count( "inbound_group_sessions" ) != 0) {
      auto sessions = obj.at("inbound_group_sessions").get<map<string, string>>();
      for (const auto &s : sessions)
        inbound_group_sessions[ s.first ] =
          unpickle< InboundSessionObject >( s.second, g_mtx_db_storage_key );
    }

    if (obj.count( "outbound_group_sessions" ) != 0) {
      auto sessions = obj.at("outbound_group_sessions").get<map<string, string>>();
      for (const auto &s : sessions)
        outbound_group_sessions[ s.first ] =
          unpickle< OutboundSessionObject >( s.second, g_mtx_db_storage_key );
    }

    if (obj.count( "outbound_group_session_data" ) != 0) {
      auto sessions = obj.at( "outbound_group_session_data" ).
        get<map<string,OutboundSessionData>>();
      for (const auto &s : sessions)
        outbound_group_session_data[s.first] = s.second;
    }
  }

  void save() {
    using namespace mtx::crypto;
    MDEBUG( "matrix session saving storage" );

    std::ofstream db( g_mtx_session_db_filename );
    if (not db.is_open()) {
      MERROR( "Couldn't open matrix session db " << g_mtx_session_db_filename );
      return;
    }

    nlohmann::json data;
    data[ "devices" ] = devices;
    data[ "device_keys" ] = device_keys;
    data[ "encrypted_rooms" ] = encrypted_rooms;
    data[ "members" ] = members;

    // Save inbound sessions
    for (const auto &s : olm_inbound_sessions)
      data[ "olm_inbound_sessions" ][ s.first ] =
        pickle< SessionObject >( s.second.get(), g_mtx_db_storage_key );

    for (const auto &s : olm_outbound_sessions)
      data[ "olm_outbound_sessions" ][ s.first ] =
        pickle< SessionObject >( s.second.get(), g_mtx_db_storage_key );

    for (const auto &s : inbound_group_sessions)
      data[ "inbound_group_sessions" ][ s.first ] =
        pickle< InboundSessionObject >( s.second.get(), g_mtx_db_storage_key );

    for (const auto &s : outbound_group_sessions)
      data[ "outbound_group_sessions" ][ s.first ] =
        pickle< OutboundSessionObject >( s.second.get(), g_mtx_db_storage_key );

    for (const auto &s : outbound_group_session_data)
      data[ "outbound_group_session_data" ][ s.first ] = s.second;

    // Save to file
    db << data.dump( 2 );
    db.close();
  }
};

static Storage g_storage;

static void
print_errors( mtx::http::RequestErr err )
// *****************************************************************************
//! Log matrix request errors
//! \param[in] err Matrix error object
// *****************************************************************************
{
  if (err->status_code)
    MERROR( "status code: " << static_cast<uint16_t>(err->status_code));
  if (not err->matrix_error.error.empty() )
    MERROR( "error msg: " << err->matrix_error.error );
  if (err->error_code)
    MERROR( "error code: " << err->error_code );
  if (not err->parse_error.empty())
    MERROR( "parse error: " << err->parse_error );
}

static void
decrypt_olm_message( const OlmMessage& olm_msg )
// *****************************************************************************
//! Decrypt Olm message
//! \param[in] olm_msg Message to decrypt
// *****************************************************************************
{
  MINFO( "OLM message" );
  MINFO( "sender: " << olm_msg.sender );
  MINFO( "sender_key: " << olm_msg.sender_key );

  const auto my_id_key = g_olmclient->identity_keys().curve25519;
  for (const auto& cipher : olm_msg.ciphertext) {
    if (cipher.first == my_id_key) {
      const auto msg_body = cipher.second.body;
      const auto msg_type = cipher.second.type;

      MINFO( "the message is meant for us" );
      MINFO( "body: " << msg_body );
      MINFO( "type: " << msg_type );

      if (msg_type == 0) {
        MINFO( "opening session with " << olm_msg.sender );
        auto inbound_session = g_olmclient->create_inbound_session( msg_body );

        auto ok = mtx::crypto::matches_inbound_session_from(
          inbound_session.get(), olm_msg.sender_key, msg_body );

        if (not ok) {
          MERROR( "session could not be established" );
        } else {
          auto output = g_olmclient->decrypt_message( inbound_session.get(),
                                                      msg_type,
                                                      msg_body );

          auto plaintext = nlohmann::json::parse(
            std::string( begin(output), end(output) ) );
          MINFO( "decrypted message: " << plaintext.dump(2) );

          g_storage.olm_inbound_sessions.emplace( olm_msg.sender_key,
                                                  std::move( inbound_session ) );

          std::string room_id =
            plaintext.at( "content" ).at( "room_id" ).get< std::string >();
          std::string session_id =
            plaintext.at( "content").at( "session_id" ).get< std::string >();
          std::string session_key =
            plaintext.at( "content" ).at( "session_key" ).get< std::string >();

          if (g_storage.inbound_group_exists( room_id, session_id,
                                              olm_msg.sender_key ))
          {
            MWARNING( "megolm session already exists" );
          } else {
            auto megolm_session =
              g_olmclient->init_inbound_group_session( session_key );
            g_storage.set_inbound_group_session( room_id,
                                                 session_id,
                                                 olm_msg.sender_key,
                                                 std::move( megolm_session ) );
            MINFO( "megolm_session saved" );
          }
        }
      }
    }
  }
}

static void
handle_to_device_msgs( const mtx::responses::ToDevice& msgs )
// *****************************************************************************
//! Handle to-device messages
//! \param[in] msgs Messages to handle
// *****************************************************************************
{
  if (not msgs.events.empty()) {
    MINFO( "inspecting to_device messages " << msgs.events.size() );
  }

  for (const auto& msg : msgs.events) {
    MINFO( std::visit(
      [](const auto& e){ return nlohmann::json(e); }, msg ).dump( 2 ) );

    try {
      OlmMessage olm_msg = std::visit(
        [](const auto &e){ return nlohmann::json(e).get< OlmMessage >(); }, msg
      );
      decrypt_olm_message( std::move( olm_msg ) );
    } catch ( const nlohmann::json::exception& e ) {
      MWARNING( "parsing error for olm message: " << e.what() );
    } catch ( const std::invalid_argument& e ) {
      MWARNING( "validation error for olm message: " << e.what() );
    }
  }
}

static void
keys_uploaded_cb( const mtx::responses::UploadKeys&, mtx::http::RequestErr err )
// *****************************************************************************
// *****************************************************************************
{
 if (err) {
   print_errors( err );
   return;
 }
 g_olmclient->mark_keys_as_published();
 MINFO( "keys uploaded" );
}

template< class T >
static bool is_room_encryption( const T& event )
// *****************************************************************************
// *****************************************************************************
{
  using namespace mtx::events;
  using namespace mtx::events::state;
  return std::holds_alternative< StateEvent< Encryption > >( event );
}

template< class T >
static std::string
get_json( const T& event )
// *****************************************************************************
// *****************************************************************************
{
  return std::visit( [](auto e){ return nlohmann::json(e).dump(2); }, event );
}

static void
mark_encrypted_room( const RoomId& id )
// *****************************************************************************
// *****************************************************************************
{
  MINFO( "encryption is enabled for room: " << id.get() );
  g_storage.encrypted_rooms[ id.get() ] = true;
}

template<class T>
static bool
is_member_event( const T& event )
// *****************************************************************************
// *****************************************************************************
{
  using namespace mtx::events;
  using namespace mtx::events::state;
  return std::holds_alternative< StateEvent< Member > >( event );
}

static bool
is_encrypted( const mtx::events::collections::TimelineEvents& event )
// *****************************************************************************
// *****************************************************************************
{
  using mtx::events::EncryptedEvent;
  using mtx::events::msg::Encrypted;
  return std::holds_alternative< EncryptedEvent< Encrypted > >( event );
}

template<class Container, class Item>
static bool
exists( const Container& container, const Item& item )
// *****************************************************************************
// *****************************************************************************
{
  return container.find(item) != end(container);
}

static void
save_device_keys( const mtx::responses::QueryKeys& res )
// *****************************************************************************
// *****************************************************************************
{
  for (const auto &entry : res.device_keys) {
    const auto user_id = entry.first;

    std::vector< std::string > device_list;
    for (const auto &device : entry.second) {
      const auto key_struct = device.second;

      const std::string device_id = key_struct.device_id;
      const std::string index = "curve25519:" + device_id;

      if (key_struct.keys.find(index) == end(key_struct.keys)) continue;

      const auto key = key_struct.keys.at( index );

      if (!exists(g_storage.device_keys, device_id)) {
        MINFO( "saved user " << user_id << "'s device id => key: " <<
               device_id << " => " << key );
        g_storage.device_keys[ device_id ] =
          { key_struct.keys.at("ed25519:" + device_id),
            key_struct.keys.at("curve25519:" + device_id) };
      }

      device_list.push_back( device_id );
    }

    g_storage.devices[ user_id ] = device_list;
  }
}

static void
get_device_keys( const UserId& user )
// *****************************************************************************
// *****************************************************************************
{
  // Retrieve all devices keys
  mtx::requests::QueryKeys query;
  query.device_keys[ user.get() ] = {};

  g_mtxclient->query_keys( query,
    [](const mtx::responses::QueryKeys &res, mtx::http::RequestErr err) {
      if (err) {
        print_errors( err );
        return;
      }

      for (const auto& key : res.device_keys) {
        const auto user_id = key.first;
        const auto devices = key.second;

        for (const auto &device : devices) {
          const auto id = device.first;
          const auto data = device.second;

          try {
            auto ok = verify_identity_signature(
              nlohmann::json( data ).get< mtx::crypto::DeviceKeys >(),
              DeviceId( id ), UserId( user_id ) );

            if (not ok) {
              MWARNING( "signature could not be verified" );
              MWARNING( nlohmann::json( data ).dump( 2 ) );
            }
          } catch (const mtx::crypto::olm_exception &e) {
            MWARNING( "exception: " << e.what() );
          }
        }
      }

      save_device_keys( std::move(res) );
  } );
}

static void
send_group_message( OlmOutboundGroupSession* session,
                    const std::string& session_id,
                    const std::string& room_id,
                    const std::string& msg )
// *****************************************************************************
// *****************************************************************************
{
  // Create event payload
  nlohmann::json doc{ {"type", "m.room.message"},
                      {"content", {{"type", "m.text"}, {"body", msg}}},
                      {"room_id", room_id} };

  auto payload = g_olmclient->encrypt_group_message( session, doc.dump() );

  using namespace mtx::events;
  using namespace mtx::identifiers;
  using namespace mtx::http;

  msg::Encrypted data;
  data.ciphertext = std::string( begin(payload), end(payload) );
  data.sender_key = g_olmclient->identity_keys().curve25519;
  data.session_id = session_id;
  data.device_id = g_mtxclient->device_id();
  data.algorithm = OLM_ALGO;

  g_mtxclient->send_room_message< msg::Encrypted >(
    room_id, data, [](const mtx::responses::EventId& res, RequestErr err) {
      if (err) {
        print_errors( err );
        return;
      }
      MINFO("message sent with event_id: " << res.event_id.to_string());
    } );
}

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpadded"
#endif

static void
create_outbound_megolm_session( const std::string& room_id,
                                const std::string& msg )
// *****************************************************************************
// *****************************************************************************
{
  // Create an outbound session
  auto outbound_session = g_olmclient->init_outbound_group_session();

  const auto session_id  = mtx::crypto::session_id( outbound_session.get() );
  const auto session_key = mtx::crypto::session_key( outbound_session.get() );

  mtx::events::DeviceEvent< mtx::events::msg::RoomKey > megolm_payload;
  megolm_payload.content.algorithm = OLM_ALGO;
  megolm_payload.content.room_id = room_id;
  megolm_payload.content.session_id = session_id;
  megolm_payload.content.session_key = session_key;
  megolm_payload.type = mtx::events::EventType::RoomKey;

  if (g_storage.members.find(room_id) == end(g_storage.members)) {
    MERROR( "no members found for room " << room_id );
    return;
  }

  const auto members = g_storage.members[ room_id ];

  for (const auto &member : members) {
    const auto devices = g_storage.devices[ member.first ];

    // TODO: Figure out for which devices we don't have olm sessions.
    for (const auto &dev : devices) {
      // TODO: check if we have downloaded the keys
      const auto& device_keys = g_storage.device_keys[ dev ];

      auto to_device_cb = [](mtx::http::RequestErr err) {
        if (err) {
          print_errors( err );
        }
      };

      if (g_storage.olm_outbound_sessions.find(device_keys.curve25519) !=
          end(g_storage.olm_outbound_sessions))
      {
        MINFO("found existing olm outbound session with device" << dev );
        auto olm_session =
          g_storage.olm_outbound_sessions[ device_keys.curve25519 ].get();

        auto device_msg =
          g_olmclient->create_olm_encrypted_content( olm_session,
                                                     megolm_payload,
                                                     UserId( member.first ),
                                                     device_keys.ed25519,
                                                     device_keys.curve25519 );

        nlohmann::json body{{"messages", {{member, {{dev, device_msg}}}}}};
        g_mtxclient->send_to_device("m.room.encrypted", body, to_device_cb);
        // TODO: send message to device
      } else {
        MINFO( "claiming one time keys for device " << dev );
        using mtx::responses::ClaimKeys;
        using mtx::http::RequestErr;
        auto cb = [member = member.first, dev, megolm_payload, to_device_cb](
                    const ClaimKeys &res, RequestErr err) {
          if (err) {
            print_errors(err);
            return;
          }

          MINFO( "claimed keys for member - dev: " << member << " - " << dev );
          MINFO( "room_key " << nlohmann::json(megolm_payload).dump(4) );

          MWARNING( "signed one time keys" );
          auto retrieved_devices = res.one_time_keys.at( member );
          for (const auto &rd : retrieved_devices) {
            MINFO( "devices: " << rd.first << " : \n " << rd.second.dump(2) );

            // TODO: Verify signatures
            auto otk = rd.second.begin()->at("key");
            auto id_key = g_storage.device_keys[dev].curve25519;

            auto session = g_olmclient->create_outbound_session( id_key,
              otk.get< std::string >() );

            auto device_msg = g_olmclient->create_olm_encrypted_content(
              session.get(),
              megolm_payload,
              UserId(member),
              g_storage.device_keys[dev].ed25519,
              g_storage.device_keys[dev].curve25519 );

            // TODO: saving should happen when the message is sent
            g_storage.olm_outbound_sessions[ id_key ] = std::move(session);

            nlohmann::json body{{"messages", {{member, {{dev, device_msg}}}}}};

            g_mtxclient->send_to_device("m.room.encrypted", body, to_device_cb);
          }
        };

        mtx::requests::ClaimKeys claim_keys;
        claim_keys.one_time_keys[ member.first ][ dev ] =
          mtx::crypto::SIGNED_CURVE25519;

        // TODO: we should bulk request device keys here
        g_mtxclient->claim_keys(claim_keys, cb);
      }
    }
  }

  // TODO: This should be done after all sendToDevice messages have been sent.
  send_group_message( outbound_session.get(), session_id, room_id, msg );

  // TODO: save message index also.
  g_storage.set_outbound_group_session(
    room_id, std::move(outbound_session), {session_id, session_key} );
}

#if defined(__clang__)
  #pragma clang diagnostic pop
#endif

static std::string
invite_room( const std::string& usr,
             const std::string& ad,
             const std::string& msg )
// *****************************************************************************
//  Invite user to a room
// *****************************************************************************
{
  MINFO( g_matrix_user << " invites " << usr << " to room" );

  mtx::requests::CreateRoom req;
  req.name = "Conversation between " + g_matrix_user + " and " + usr;
  req.topic = ad;
  req.invite = { usr };
  std::string room_id;
  g_mtxclient->create_room( req,
    [&](const mtx::responses::CreateRoom& res, mtx::http::RequestErr err) {
        print_errors( err );
        room_id = res.room_id.to_string();
        MINFO( g_matrix_user << " created room id " << room_id );
        g_mtxclient->invite_user( room_id, usr,
          [](const mtx::responses::Empty&, mtx::http::RequestErr) {} );
        create_outbound_megolm_session( room_id, msg );
    });
  return room_id;
}

void
piac::matrix_message( std::string usr, std::string ad, std::string msg )
// *****************************************************************************
//  Send a message to a user
// *****************************************************************************
{
  MINFO( g_matrix_user << " messages " << usr );

  // create new room between users if it does not yet exist
  auto room_id = g_storage.in_room( g_matrix_user, usr );
  if (room_id.empty()) {

    MINFO( "room between " << g_matrix_user << " and " << usr <<
           " does not yet exist" );
    room_id = invite_room( usr, ad, msg );

  } else {

    MINFO( "room between " << g_matrix_user << " and " << usr <<
           " already exists" );
    auto session_obj = g_storage.get_outbound_group_session( room_id );
    send_group_message( session_obj.session,
                        session_obj.data.session_id,
                        room_id,
                        msg );
  }
}

static void
parse_messages( const mtx::responses::Sync& res )
// *****************************************************************************
//! Parse received messages after sync
//! \param[in] res Sync response to parse
// *****************************************************************************
{
  MDEBUG( "parse_messages" );

  for (const auto& room : res.rooms.invite) {
    auto room_id = room.first;
    MINFO( "joining room " + room_id );
    g_mtxclient->join_room( room_id,
      [room_id](const mtx::responses::RoomId&, mtx::http::RequestErr e) {
        if (e) {
          print_errors( e );
          MERROR( "failed to join room " << room_id );
          return;
        }
      });
  }

  // Check if we have any new m.room_key messages
  // (i.e starting a new megolm session)
  handle_to_device_msgs( res.to_device );

  // Check if the uploaded one time keys are enough
  for (const auto &device : res.device_one_time_keys_count) {
    if (device.second < 50) {
      MDEBUG( "number of one time keys: " << device.second );
      g_olmclient->generate_one_time_keys( 50 - device.second );
      g_mtxclient->upload_keys( g_olmclient->create_upload_keys_request(),
                                &keys_uploaded_cb );
    }
  }

  for (const auto &room : res.rooms.join) {
    const auto room_id = room.first;

    for (const auto &e : room.second.state.events) {
      if (is_room_encryption( e )) {
        mark_encrypted_room( RoomId(room_id) );
        MDEBUG( "room state events " << get_json(e) );
      } else if (is_member_event( e )) {
        using namespace mtx::events;
        auto m = std::get< StateEvent< state::Member > >( e );
        get_device_keys( UserId( m.state_key ) );
        g_storage.add_member( room_id, m.state_key );
      }
    }

    for (const auto &e : room.second.timeline.events) {
      if (is_room_encryption(e)) {

        mark_encrypted_room( RoomId( room_id ) );
        MDEBUG( "room timeline events " << get_json(e) );

      } else if (is_member_event( e )) {

        using namespace mtx::events;
        auto m = std::get< StateEvent< state::Member > >( e );
        get_device_keys( UserId( m.state_key ) );
        g_storage.add_member( room_id, m.state_key );

      } else if (is_encrypted( e )) {

        MDEBUG( "received an encrypted event: " << room_id );
        MDEBUG( get_json(e) );

        using mtx::events::EncryptedEvent;
        using mtx::events::msg::Encrypted;
        auto msg = std::get< EncryptedEvent< Encrypted > >( e );

        if (g_storage.inbound_group_exists(
              room_id, msg.content.session_id, msg.content.sender_key))
        {
          auto r = g_olmclient->decrypt_group_message(
            g_storage.get_inbound_group_session(
              room_id, msg.content.session_id, msg.content.sender_key ),
            msg.content.ciphertext );

          auto msg_str = std::string( begin(r.data), end(r.data) );
          const auto body = nlohmann::json::parse(msg_str).
            at("content").at("body").get< std::string >();

          MDEBUG( "decrypted data: " << body );
          MDEBUG( "decrypted message index: " << r.message_index );

        } else {
          MWARNING( "no megolm session found to decrypt the event" );
        }

      }
    }
  }

  // tell the message thread that we have just parsed messages
  zmqpp::message msg;
  msg << "parsed_messages";
  g_msg_mtx.back().send( msg );
}

static void
sync_handler( const mtx::responses::Sync& res, mtx::http::RequestErr err )
// *****************************************************************************
// Callback to executed after a /sync request completes.
//! \param[in] res Sync response
//! \param[in] err Errors if any
// *****************************************************************************
{
  MDEBUG( "sync_handler" );

  mtx::http::SyncOpts opts;

  if (err) {
    MERROR( "error during sync" );
    print_errors( err );
    opts.since = g_mtxclient->next_batch_token();
    g_mtxclient->sync( opts, &sync_handler );
    return;
  }

  if (piac::g_matrix_shutdown) return;

  parse_messages( res );
  opts.timeout = piac::g_matrix_sync_timeout;
  opts.since = res.next_batch;
  g_mtxclient->set_next_batch_token( res.next_batch );
  g_mtxclient->sync( opts, &sync_handler );
}

static void
initial_sync_handler( const mtx::responses::Sync& res,
                      mtx::http::RequestErr err )
// *****************************************************************************
// Callback execute after the first (initial) /sync request completes
//! \param[in] res Sync response
//! \param[in] err Errors if any
// *****************************************************************************
{
  MDEBUG( "initial_sync_handler" );

  mtx::http::SyncOpts opts;

  if (err) {
    MERROR( "error during initial sync" );
    print_errors( err );

    if (err->status_code != 200) {
      MERROR( "retrying initial sync ..." );
      opts.timeout = 0;
      g_mtxclient->sync( opts, &initial_sync_handler );
    }

    return;
  }

  if (piac::g_matrix_shutdown) return;

  parse_messages( res );

  for (const auto &room : res.rooms.join) {
    const auto room_id = room.first;
    for (const auto &e : room.second.state.events) {
      if (is_member_event(e)) {
        using namespace mtx::events;
        auto m = std::get< StateEvent< state::Member > >( e );
        get_device_keys( UserId( m.state_key ) );
        g_storage.add_member( room_id, m.state_key );
      }
    }
  }

  opts.since = res.next_batch;
  opts.timeout = piac::g_matrix_sync_timeout;
  g_mtxclient->set_next_batch_token( res.next_batch );
  g_mtxclient->sync( opts, &sync_handler );
}

static void
login_cb( const mtx::responses::Login&, mtx::http::RequestErr error )
// *****************************************************************************
//! Callback executed after login attempt
//! \param[in] error Error if any
// *****************************************************************************
{
  MLOG_SET_THREAD_NAME( "mtx" );
  MDEBUG( "login_cb" );

  if (error) {
    MERROR( "login error" );
    print_errors( error );
    if (not error->matrix_error.error.empty() ) {
      std::cerr << error->matrix_error.error << '\n';
    }
    return;
  }

  if (not g_storage.device_id.empty() && not g_storage.access_token.empty()) {
    g_mtxclient->set_device_id( g_storage.device_id );
    g_mtxclient->set_access_token( g_storage.access_token );
  } else {
    g_storage.device_id = g_mtxclient->device_id();
    g_storage.access_token = g_mtxclient->access_token();
  }

  MDEBUG( "user id: " + g_mtxclient->user_id().to_string() );
  MDEBUG( "device id: " + g_mtxclient->device_id() );
  MDEBUG( "access token: " + g_mtxclient->access_token() );
  MDEBUG( "ed25519: " + g_olmclient->identity_keys().ed25519 );
  MDEBUG( "curve25519: " + g_olmclient->identity_keys().curve25519 );

  // Upload one-time keys
  g_olmclient->set_user_id( g_mtxclient->user_id().to_string() );
  g_olmclient->set_device_id( g_mtxclient->device_id() );
  g_olmclient->generate_one_time_keys( 50 );

  g_mtxclient->upload_keys( g_olmclient->create_upload_keys_request(),
    [](const mtx::responses::UploadKeys&, mtx::http::RequestErr err ) {
      if (err) {
        print_errors(err);
        return;
      }
      g_olmclient->mark_keys_as_published();
      MDEBUG( "keys uploaded" );
      MDEBUG( "starting initial sync" );
      mtx::http::SyncOpts opts;
      opts.timeout = 0;
      g_mtxclient->sync( opts, &initial_sync_handler );
    } );
}

void
piac::matrix_thread( const std::string& server,
                     const std::string& username,
                     const std::string& password,
                     const std::string& db_key )
// *****************************************************************************
//  Entry point to thread to communicate with a matrix server
//! \param[in] server Matrix hostname to connect to as \<host\>[:port]
//! \param[in] username Username to use
//! \param[in] password Password to use
//! \param[in] db_key Database key to use to encrypt session db on disk
// *****************************************************************************
{
  MLOG_SET_THREAD_NAME( "mtx" );
  MINFO( "mtx thread initialized" );
  MDEBUG( "matrix login request to server: " << server << ", username: " <<
          username << ", connected: " << std::boolalpha << g_matrix_connected );

  if (g_matrix_connected) {
    MDEBUG( "already connected" );
    return;
  }

  int port = 443;
  auto [host,prt] = split( server, ":" );
  if (prt.empty()) {
    g_mtxclient = std::make_shared< mtx::http::Client >( host );
  } else {
    try {
      port = std::stoi( prt );
    } catch (std::exception&) {
      std::cerr << "invalid <host>:<port> format\n";
      return;
    }
    g_mtxclient = std::make_shared< mtx::http::Client >( host, port );
  }
  MDEBUG( "will connect as @" << username << ':' << host << ':' << port );
  g_matrix_user = '@' + username + ':' + host;

  g_mtxclient->verify_certificates( false );
  g_olmclient = std::make_shared< mtx::crypto::OlmClient >();

  std::string db_base_filename = "piac-matrix-@" + username + ':' + server;
  g_mtx_account_db_filename = db_base_filename + ".account.json";
  g_mtx_session_db_filename = db_base_filename + ".session.json";
  MINFO( "account db filename: " << g_mtx_account_db_filename );
  MINFO( "session db filename: " << g_mtx_session_db_filename );
  g_mtx_db_storage_key = db_key;

  std::ifstream db( g_mtx_account_db_filename );
  std::string db_data( (std::istreambuf_iterator< char >( db )),
                       std::istreambuf_iterator< char >() );
  if (db_data.empty()) {
    g_olmclient->create_new_account();
  } else {
    g_olmclient->load(
      nlohmann::json::parse(db_data).at("account").get<std::string>(),
      g_mtx_db_storage_key );
    g_storage.load();
  }

  // create socket to forward matrix messages to
  g_msg_mtx.emplace_back( piac::g_ctx_msg, zmqpp::socket_type::pair );
  g_msg_mtx.back().connect( "inproc://msg_mtx" );
  MDEBUG( "Connected to inproc:://msg_mtx" );

  g_matrix_connected = true;
  g_mtxclient->login( username, password, login_cb );   // blocking, syncing...
  g_mtxclient->close();
  g_matrix_connected = false;
  g_matrix_shutdown = false;

  // tell the message thread that we are shutting down
  zmqpp::message msg;
  msg << "SHUTDOWN";
  g_msg_mtx.back().send( msg );
  g_msg_mtx.clear();

  MDEBUG( "saving matrix session to " +
          g_mtx_account_db_filename + " and " + g_mtx_session_db_filename );
  g_storage.save();
  std::ofstream odb( g_mtx_account_db_filename );
  if (not odb.is_open()) {
    MERROR( "Couldn't open matrix account db " << g_mtx_account_db_filename );
    return;
  }
  nlohmann::json data;
  data[ "account" ] = g_olmclient->save( g_mtx_db_storage_key );
  odb << data.dump( 2 );
  odb.close();

  MINFO( "mtx thread quit" );
}
