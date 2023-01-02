# piac overall status

Currently, the basics are being prototyped. At this point, the app consists of
a daemon and a command-line client. See other files in this directory for
details on how these interact with each other, how they form a distributed
peer-to-peer network, and some details on what works already. At a high level:

- Basic p2p communication among multiple daemons works and routinely tested.

- The p2p keeps all peers up-to-date on their db.

- Networking and threading code is asynchronous.

- The daemon is threaded: one thread interrogates the database, another
  connects with peers.

- The communication between server and client can be optionally encrypted and
authenticated.

- The command-line interface client
   * connects users to daemons and the ad database,
   * authenticates users via a monero wallet mnemonic seed,
   * can issue basic queries to the ad db in a web search fashion,
   * can add or remove ads after user authententication,
   * can query and monitor wallet balance connecting to a monero daemon.

# progress/to-do

Here is a todo list in no particular order:

- [x] add todo to readme
- [x] add some more details to readme
- [x] encrypt socket between cli and daemon, stonehouse
- [x] encrypt socket between cli and daemon, ironhouse
- [x] build/test with clang
- [x] add `-Wall` to build system
- [x] set up generation of code coverage (gcov, cppcheck)
- [x] refactor + add doxygen docs
- [ ] add images to ads in db
- [ ] purge ad after completion of purchase
- [ ] add ad authentication (WIP)
- [ ] allow connecting within I2P via i2pd (both daemon-cli and p2p)
- [ ] add p2p traffic encryption and authentication
- [ ] implement more sophisticated p2p commmunication based on white/gray lists
- [ ] add messaging between users via matrix (WIP, login, create/join room work, tested)
- [ ] allow user to save other users in addressbook (matrixuser + monero address)
- [ ] add user's bio (e.g., external contact info) and make it searchable
- [x] allow connecting to local monero node
- [ ] allow connecting to remote monero node (WIP)
- [ ] allow running local monero node
- [x] allow user to query balance (sync wallet in background)
- [ ] allow user to buy something
- [ ] allow user to track order status
- [ ] allow user to create whishlist
- [ ] refactor to facilitate easy translation to other languages of both cli and daemon
- [ ] explore escrow
- [ ] explore/add reputation (automatic after successful buy/sell and/or by other users)
- [ ] explore product rating
- [ ] explore encrypting db at rest
- [ ] respect `NO_COLOR` env var, see https://github.com/monero-project/monero/issues/8688
- [ ] write communication protocol specification document for daemon
- [ ] add JSON API to daemon (e.g., https://github.com/uskr/jsonrpc-lean)
- [ ] add valgrind tests
- [ ] add detailed contributing.md
- [ ] describe trust model
- [ ] write privacy policy
- [ ] describe how to handle disputes
- [ ] GUI: bright/dark theme, limits on entries, add ad all in one page
- [ ] display ad price in XMR (optionally in user-configured currency)
- [ ] explore anonymous shipping address options
- [ ] explore implementing a trust metric to mitigate Sybil-attacks
