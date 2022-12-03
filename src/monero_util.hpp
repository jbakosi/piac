// *****************************************************************************
/*!
  \file      src/monero_util.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac monero wallet interaction functionality
*/
// *****************************************************************************

#pragma once

#include "macro.hpp"

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-copy-dtor"
  #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
  #pragma clang diagnostic ignored "-Wsuggest-override"
  #pragma clang diagnostic ignored "-Wunused-template"
  #pragma clang diagnostic ignored "-Wsign-conversion"
  #pragma clang diagnostic ignored "-Wcast-qual"
  #pragma clang diagnostic ignored "-Wheader-hygiene"
  #pragma clang diagnostic ignored "-Wshadow"
  #pragma clang diagnostic ignored "-Wshadow-field"
  #pragma clang diagnostic ignored "-Wextra-semi"
  #pragma clang diagnostic ignored "-Wextra-semi-stmt"
  #pragma clang diagnostic ignored "-Wdocumentation"
  #pragma clang diagnostic ignored "-Wunused-parameter"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
  #pragma clang diagnostic ignored "-Wdouble-promotion"
  #pragma clang diagnostic ignored "-Wsuggest-destructor-override"
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
  #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
  #pragma clang diagnostic ignored "-Wimplicit-int-conversion"
  #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
  #pragma clang diagnostic ignored "-Wold-style-cast"
  #pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
  #pragma clang diagnostic ignored "-Wswitch-enum"
  #pragma clang diagnostic ignored "-Wshorten-64-to-32"
  #pragma clang diagnostic ignored "-Wcovered-switch-default"
  #pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
  #pragma clang diagnostic ignored "-Wunused-variable"
  #pragma clang diagnostic ignored "-Wused-but-marked-unused"
  #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
  #pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
  #pragma clang diagnostic ignored "-Wredundant-parens"
  #pragma clang diagnostic ignored "-Wunused-exception-parameter"
  #pragma clang diagnostic ignored "-Wreorder-ctor"
  #pragma clang diagnostic ignored "-Wweak-vtables"
  #pragma clang diagnostic ignored "-Wshift-count-overflow"
  #pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
  #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(STRICT_GNUC)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #pragma GCC diagnostic ignored "-Wreorder"
#endif

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include "wallet/monero_wallet_full.h"

#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(STRICT_GNUC)
  #pragma GCC diagnostic pop
#endif

#include "zmq_util.hpp"

namespace piac {

//! Monero wallet synchronization listener class
class WalletListener : public monero::monero_wallet_listener {
  public:

    WalletListener() : m_height( 0 ), m_start_height( 0 ), m_end_height( 0 ),
      m_percent_done( 0.0 ), m_message() {}

    void on_sync_progress( uint64_t height, uint64_t start_height,
                           uint64_t end_height, double percent_done,
                           const std::string& message ) override;

    virtual ~WalletListener() = default;
    uint64_t height() const { return m_height; }
    uint64_t start_height() const { return m_start_height; }
    uint64_t end_height() const { return m_end_height; }
    double percent_done() const { return m_percent_done; }
    std::string message() const { return m_message; }

  private:
    uint64_t m_height;
    uint64_t m_start_height;
    uint64_t m_end_height;
    double m_percent_done;
    std::string m_message;
};

//! Send a command to piac daemon
void
send_cmd( std::string cmd,
          zmqpp::context& ctx,
          const std::string& host,
          const std::string& rpc_server_public_key,
          const zmqpp::curve::keypair& client_keys,
          const std::unique_ptr< monero_wallet_full >& wallet );

//! Start wallet sync in the background
void
start_syncing( const std::string& msg,
               monero_wallet_full* wallet,
               WalletListener& listener );

//! Create a new monero wallet
std::unique_ptr< monero_wallet_full >
create_wallet( const std::string& monerod_host, WalletListener& listener );

//! Query and print currently active wallet keys
void
show_wallet_keys( const std::unique_ptr< monero_wallet_full >& wallet );

//! Query and print currently active wallet balance
void
show_wallet_balance( const std::unique_ptr< monero_wallet_full >& wallet,
                     const WalletListener& listener );

//! Change currently active user id / monero wallet
std::unique_ptr< monero_wallet_full >
switch_user( const std::string& mnemonic,
             const std::string& monerod_host,
             WalletListener& listener );

//! Show user id / monero wallet mnemonic
void
show_user( const std::unique_ptr< monero_wallet_full >& wallet );

} // piac::
