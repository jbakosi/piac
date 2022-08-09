# piac

[![status-badge](https://ci.codeberg.org/api/badges/piac/piac/status.svg)](https://ci.codeberg.org/piac/piac)

_Buy and sell anything privately and securely_

The goal of this project is to develop a decentralized peer-to-peer marketplace where sellers and buyers can transact privately and securely preserving as much anonymity as practically possible.

This project is work-in-progress.

## Contribute
Join the discussion on [matrix](https://matrix.to/#/#neroshop:matrix.org).

## Planned features
* Buy and sell with [monero](https://getmonero.org)
* Decentralized distributed peer-to-peer marketplace
* Pseudonymous identities
* No tracking
* No analytics
* No KYC
* No censorship
* No registration
* No fees (except monero transaction fees and shipping costs)
* Multi-signature escrowed transactions
* End-to-end-encrypted connections and messaging
* I2P and Tor integration
* Reputation system
* Product rating system
* Wishlists
* Built-in monero node or connect to remote nodes

## Progress
Currently, the basics are prototyped. At this point, the app consists of a daemon and a command-line client. See [doc/README.md](https://codeberg.org/piac/piac/src/branch/master/doc) for details on how these interact with each other, how they form a distributed peer-to-peer network, and some details on what works already. Here is a todo list in no particular order:

- [x] add todo to readme
- [x] add some more details to readme
- [x] encrypt socket between cli and daemon, stonehouse
- [x] encrypt socket between cli and daemon, ironhouse
- [x] build/test with clang
- [ ] add `-Wall` to build system
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

## Build
```sh
# Install system-wide prerequisites on Debian/Ubuntu Linux
sudo apt-get install git cmake g++ pkg-config libssl-dev libminiupnpc-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev libboost-regex-dev libboost-serialization-dev libboost-system-dev libboost-thread-dev libzmq3-dev libhidapi-dev libprotobuf-dev libusb-dev libxapian-dev rapidjson-dev libreadline-dev libcrypto++-dev libssl-dev
# Clone piac
git clone https://codeberg.org/piac/piac.git && cd piac
# Build external libraries
mkdir -p external/build && cd external/build && cmake .. && make && cd -
# Build piac
mkdir build && cd build && cmake .. && make && cd -
# Test piac (optional)
cd build && ctest
```

## Run
```sh
# Run daemon
cd piac/build
./piac-daemon --help
# Run command-line client (in another terminal)
cd piac/build
./piac-cli --help
```

## Pronounce
[[ˈpijɒt͡s]](https://en.wiktionary.org/wiki/piac)

## License
GPL v3.
