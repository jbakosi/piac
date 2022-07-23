# piac 

[![status-badge](https://ci.codeberg.org/api/badges/piac/piac/status.svg)](https://ci.codeberg.org/piac/piac)

_Buy and sell anything privately and securely_

This project is work-in-progress. Questions? Contact us on
[matrix](https://matrix.to/#/@jbakosi:matrix.org).

## Planned features

* Buy and sell products with [monero](https://getmonero.org)
* Decentralized distributed peer-to-peer marketplace
* Pseudonymous identities
* No tracking
* No analytics
* No KYC
* No censorship
* No registration required
* No fees (except monero transaction fees and shipping costs)
* Multi-signature escrowed transactions
* End-to-end-encrypted messaging
* Tor integration
* Reputation system
* Product rating system
* Wishlists
* Built-in monero node or connect to remote nodes

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
./piac-daemon
# Run command-line client (in another terminal)
cd piac/build
./piac-cli
```

## Contribute
Contact us on [matrix](https://matrix.to/#/@jbakosi:matrix.org).

## Pronounce
[[ˈpijɒt͡s]](https://en.wiktionary.org/wiki/piac)

## License
GPL v3.
