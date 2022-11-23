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
   * can issue basic queries to the db, in a web search fashion,
   * can add or remove ads after user authententication.

# progress/to-do

Here is a todo list in no particular order:

- [x] add todo to readme
- [x] add some more details to readme
- [x] encrypt socket between cli and daemon, stonehouse
- [x] encrypt socket between cli and daemon, ironhouse
- [x] build/test with clang
- [x] add `-Wall` to build system
- [x] set up generation of code coverage (gcov, cppcheck)
- [ ] add valgrind tests
- [ ] implement more sophisticated p2p commmunication based on white/gray lists
- [ ] encrypt db at rest
- [ ] add images to ads in db
- [ ] purge ad after completion
- [ ] allow connecting within I2P via i2pd (both daemon-cli and p2p)
- [ ] add p2p traffic encryption and authentication
- [ ] add messaging between users via matrix (libkazv)
- [ ] allow running and connecting to own monero node
- [ ] allow connecting to remote monero node
- [ ] allow user to see balance
- [ ] allow user to buy something
- [ ] allow user to track order status
- [ ] allow user to create whishlist
- [ ] explore escrow
- [ ] explore/add reputation (automatic after successful buy/sell and/or by other users)
- [ ] explore product rating
- [ ] add detailed contributing.md
- [ ] describe trust model
- [ ] write privacy policy
- [ ] describe how to handle disputes
- [ ] GUI: bright/dark theme, limits on entries, add ad all in one page
- [ ] price in XMR (optionally show in user-configured currency)
- [ ] explore anonymous shipping address options
