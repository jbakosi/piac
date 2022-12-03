// *****************************************************************************
/*!
  \file      src/monero_util.cpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac monero wallet interaction functionality
*/
// *****************************************************************************

#include "string_util.hpp"
#include "crypto_util.hpp"
#include "monero_util.hpp"

void
piac::WalletListener::on_sync_progress( uint64_t height,
                                        uint64_t start_height,
                                        uint64_t end_height,
                                        double percent_done,
                                        const std::string& message )
{
  m_height = height;
  m_start_height = start_height;
  m_end_height = end_height;
  m_percent_done = percent_done;
  m_message = message;
}

void
piac::send_cmd( std::string cmd,
                zmqpp::context& ctx,
                const std::string& host,
                const std::string& rpc_server_public_key,
                const zmqpp::curve::keypair& client_keys,
                const std::unique_ptr< monero_wallet_full >& wallet )
// *****************************************************************************
//  Send a command to piac daemon
//! \param[in] cmd Command to send to piac daemon
//! \param[in,out] ctx ZeroMQ socket context to use
//! \param[in] host Hostname or IP + port of piac daemon to send cmd to
//! \param[in] rpc_server_public_key CurveZMQ server public key to use
//! \param[in] client_keys CurveMQ client keypair to use
//! \param[in] wallet Monero wallet to use as author / user id
// *****************************************************************************
{
  trim( cmd );
  MDEBUG( cmd );

  // append author if cmd contains "db add/rm"
  auto npos = std::string::npos;
  if ( cmd.find("db") != npos &&
      (cmd.find("add") != npos || cmd.find("rm") != npos) )
  {
    if (not wallet) {
      std::cout << "Need active user id (wallet) to add to db. "
                   "See 'new' or 'user'.\n";
      return;
    }
    cmd += " AUTH:" + piac::sha256( wallet->get_primary_address() );
  }

  // send message to daemon with command
  auto reply = pirate_send( cmd, ctx, host, rpc_server_public_key, client_keys );
  std::cout << reply << '\n';
}

void
piac::start_syncing( const std::string& msg,
                     monero_wallet_full* wallet,
                     WalletListener& listener )
// *****************************************************************************
//  Start wallet sync the background
//! \param[in] msg Info message to print before starting to sync
//! \param[in,out] wallet Pointer to monero wallet object from monero-cpp
//! \param[in,out] listener Wallet listener object up to initialize used to
//!   interact with the background sync
// *****************************************************************************
{
  assert( wallet );
  std::cout << msg;
  if (wallet->is_connected_to_daemon()) {
    std::cout << ", starting background sync, check progress with 'balance'\n";
    wallet->add_listener( listener );
    wallet->start_syncing( /* sync period in ms = */ 10000 );
  } else {
    std::cout << ", no connection to monero daemon, see 'monerod'\n";
  }
}

[[nodiscard]] std::unique_ptr< monero_wallet_full >
piac::create_wallet( const std::string& monerod_host, WalletListener& listener )
// *****************************************************************************
//  Create a new monero wallet
//! \param[in] monerod_host Hostname + port of a monero daemon to connect to
//! \param[in,out] listener Wallet listener object up to initialize used to
//!   interact with the background sync
//! \return Smart pointer to monero wallet object
// *****************************************************************************
{
  MDEBUG( "new" );
  monero::monero_wallet_config wallet_config;
  wallet_config.m_network_type = monero_network_type::STAGENET;
  wallet_config.m_server_uri = monerod_host;
  auto wallet = monero_wallet_full::create_wallet( wallet_config );
  start_syncing( "Created new user/wallet", wallet, listener );
  std::cout << "Mnemonic seed: " << wallet->get_mnemonic() << '\n' <<
    "This is your monero wallet mnemonic seed that identifies you within\n"
    "piac. You can use it to create your ads or pay for a product. This seed\n"
    "can be restored within your favorite monero wallet software and you can\n"
    "use this wallet just like any other monero wallet. Save this seed and\n"
    "keep it secret.\n";
  return std::unique_ptr< monero_wallet_full >( wallet );
}

void
piac::show_wallet_keys( const std::unique_ptr< monero_wallet_full >& wallet )
// *****************************************************************************
//  Query and print currently active wallet keys
//! \param[in] wallet Smart pointer to monero wallet object
// *****************************************************************************
{
  MDEBUG( "keys" );
  if (not wallet) {
    std::cout << "Need active user id (wallet). See 'new' or 'user'.\n";
    return;
  }
  std::cout << "Mnemonic seed: " << wallet->get_mnemonic() << '\n';
  std::cout << "Primary address: " << wallet->get_primary_address() << '\n';
  std::cout << "Secret view key: " << wallet->get_private_view_key() << '\n';
  std::cout << "Public view key: " << wallet->get_public_view_key() << '\n';
  std::cout << "Secret spend key: " << wallet->get_private_spend_key() << '\n';
  std::cout << "Public spend key: " << wallet->get_public_spend_key() << '\n';
}

void
piac::show_wallet_balance( const std::unique_ptr< monero_wallet_full >& wallet,
                           const WalletListener& listener )
// *****************************************************************************
//  Query and print currently active wallet balance
//! \param[in] wallet Smart pointer to monero wallet object
//! \param[in] listener Wallet listener object interacting with background sync
// *****************************************************************************
{
  MDEBUG( "balance" );
  if (not wallet) {
    std::cout << "Need active user id (wallet). See 'new' or 'user'.\n";
    return;
  }
  std::cout << "Balance = " << std::fixed << std::setprecision( 6 )
            << static_cast< double >( wallet->get_balance() ) / 1.0e+12
            << " XMR\n";
  std::cout << "Wallet sync start height: " << listener.start_height() << '\n';
  std::cout << "Wallet sync height: " << listener.height() << '\n';
  std::cout << "Wallet sync end height: " << listener.end_height() << '\n';
  std::cout << "Wallet sync at: " << std::fixed << std::setprecision( 2 )
            << listener.percent_done() * 100.0 << "%\n";
}

[[nodiscard]]
std::unique_ptr< monero_wallet_full >
piac::switch_user( const std::string& mnemonic,
                   const std::string& monerod_host,
                   WalletListener& listener )
// *****************************************************************************
//  Change currently currently active user id / monero wallet
//! \param[in] mnemonic Monero mnemonic to use
//! \param[in] monerod_host Hostname + port of a monero daemon to connect to
//! \param[in,out] listener Wallet listener object up to initialize used to
//!   interact with the background sync
// *****************************************************************************
{
  MDEBUG( "user" );
  assert( not mnemonic.empty() );
  monero::monero_wallet_config wallet_config;
  wallet_config.m_mnemonic = mnemonic;
  wallet_config.m_network_type = monero_network_type::STAGENET;
  wallet_config.m_server_uri = monerod_host;
  auto wallet = monero_wallet_full::create_wallet( wallet_config );
  start_syncing( "Switched to new user/wallet", wallet, listener );
  return std::unique_ptr< monero_wallet_full >( wallet );
}

void
piac::show_user( const std::unique_ptr< monero_wallet_full >& wallet )
// *****************************************************************************
//  Show user id /monero wallet mnemonic
//! \param[in] wallet Smart pointer to monero wallet object
// *****************************************************************************
{
  MDEBUG( "user" );
  if (not wallet) {
    std::cout << "No active user id (wallet). See 'new' or 'user'.\n";
    return;
  }
  std::cout << "Mnemonic seed: " << wallet->get_mnemonic() << '\n';
}
