# piac overall status

- Basic p2p communication among multiple daemons works and routinely tested.

- Networking and threading code is fully asynchronous.

- The daemon is threaded, i.e., can use multiple cores: one thread interrogates
the database, another connects with peers.

- The communication between server and client can be optionally encrypted and
authenticated.

- The command line interface connects users to daemons and the ad database.

- The command line interface authenticates users via a monero wallet mnemonic seed.

# progress/to-do

Currently, the basics are prototyped. At this point, the app consists of a
daemon and a command-line client. See other files in this directory for details
on how these interact with each other, how they form a distributed peer-to-peer
network, and some details on what works already. Here is a todo list in no
particular order:

- [x] add todo to readme
- [x] add some more details to readme
- [x] encrypt socket between cli and daemon, stonehouse
- [x] encrypt socket between cli and daemon, ironhouse
- [x] build/test with clang
- [ ] add `-Wall` to build system
- [ ] allow connecting to I2P via i2pd
- [ ] implement more sophisticated p2p commmunication basde eon white/gray lists
- [ ] add valgrind tests
- [ ] set up automatic generation of code coverage
- [ ] add detailed contributing.md
- [ ] encrypt db at rest
- [ ] add images to ads in db
- [ ] purge ad after completion
- [ ] describe trust model
- [ ] write privacy policy
- [ ] describe how to handle disputes
- [ ] GUI: bright/dark theme, limits on entries, add ad all in one page
- [ ] price in XMR (optionally show in user-configured currency)
- [ ] explore anonymous shipping address options
