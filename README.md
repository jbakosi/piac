# piac 

_Buy and sell anything privately and securely_

This project is work-in-progress. Questions? Contact us on
[matrix](https://matrix.to/#/@jbakosi:matrix.org).

## Planned Features

* Buy and sell products with [monero](https://getmonero.org)
* Decentralized distributed peer-to-peer network
* No censorship
* Pseudonymous identities
* Multi-signature escrowed transactions
* End-to-end-encrypted messaging
* Tor integration
* No KYC
* No buyer registration for browsing
* No fees except monero transaction fees and shipping costs
* Reputation system
* Product rating system
* Wishlists
* Built-in monero node or connect to remote nodes


## Building piac

```sh
# Install system-wide prerequisites on Debian/Ubuntu Linux
sudo apt-get install git cmake g++ pkg-config libssl-dev libminiupnpc-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev libboost-regex-dev libboost-serialization-dev libboost-system-dev libboost-thread-dev libzmq3-dev libhidapi-dev libprotobuf-dev libusb-dev libxapian-dev libeasyloggingpp-dev
# Clone piac
git clone https://codeberg.org/piac/piac.git && cd piac
# Build external libraries
mkdir -p external/build && cd external/build && cmake .. && make && cd -
# Build piac
mkdir build && cd build && cmake .. && make && cd -
# Test piac (optional)
cd build && ctest
```

## Running piac
```sh
# Run daemon
cd piac/build
./piac-daemon
# Run command-line client (in another terminal)
cd piac/build
./piac-cli
```

## Contributing

Contact us on [matrix](https://matrix.to/#/@jbakosi:matrix.org).

## License

GPL v3.
