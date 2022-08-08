# Table of contents

  - [Overall status](#status)
    - [RPC: server/client communication](#rpc)
    - [P2P: peer-to-peer communication](#p2p)
  - [Client/server architecture](#architecture)
    - [Communication](#communication)
    - [Multi-threading](#multi-threading)
    - [Encrypted RPC](#encrypted-rpc)

# Overall status

Basic p2p communication among multiple daemons works and routinely tested. Some
details:

Networking and threading code is fully asynchronous.

The daemon is threaded, i.e., can use multiple cores: one thread interrogates
the database, another connects with peers.

## RPC: server/client communication

A daemon listens on a socket and accepts connections from potentially multiple
clients.

RPC connections between clients and server can be optionally securely
authenticated and encrypted using elliptic-curve cryptography. See below for
how to set that up.

Clients can connect to the server and query to (1) issue basic searches with
the full features of xapian, e.g., multiple languages, synonyms, ranking, etc.,
(2) post an ad (or multiple ads in a single command) recorded by the daemon to
the database (testing this routinely with 500 randomly-generated ads for now),
(3) list hashes of ads (used to identify an ad and ensuring uniqueness among
peers), and (4) query peers a daemon is connected to at the moment.

## P2P: peer-to-peer communication

Each daemon also listens on multiple sockets that connect to other peers, in a
1-to-N fashion, which facilitates peer communication.

The p2p connections are _not_ currently authenticated or encrypted.

Using this mechanism, peers can find each other as they come online, either via
a hard-coded default P2P port or an address:port given on the command line of a
known peer.

Peers broadcast their list of peers, which ensures that those peers also find
each other that did not know each other at startup, but only know each other
via other peers. In other words, eventually all peers find each other.  Only
tested this with 3-4 peers so far, but the algorithm is general.

Peers also broadcast their list of hashes, each uniquely identifying an ad in
their database. When other peers receive this list, they check what they have
and request missing ads they do not yet have. As new peers come online, this
procedure eventually ends up with the union of all ads every peer having the
same ads in their database.

Users authenticate themselves in the client. Authentication is done via
generating a new, or using an existing, monero wallet's mnemonic seed. There
are no usernames and passwords, only this seed. This seed should be kept secret
by each user, as that's not only their single id but also their wallet. If a
user forgets a seed, there is no way to recover it. After user authentication,
users can post ads, currently in the form of JSON. A hash of the user's
wallet's primary key is attached to the ad in the database. Ads can only be
added using a user id (monero wallet primary key). Only the author of the ad
can delete ads from the database. Changes to the database are automatically
synced among peers. The database currently only stores text; images are next.

All of the above is regression tested.

# Client/server architecture

The piac app consists of a daemon (or server) and a client. The simplest way of
running piac is to run a daemon and a client on the same computer. However,
running a daemon is not strictly necessary: if you know the address and port of
an existing running instance of the daemon, you can connect your client to it.
In the following the words _daemon_, _server_, or _peer_ are used
interchangeably.

## Communication

Peers communicate with each other and form a decentralized peer-to-peer
network. There is no central database: each peer maintains a full copy of the
database which clients can add to or remove ads from after authentication.

Communication is facilitated by sockets, implemented using
[libzmq](https://zeromq.org) via its higher-level abstraction
[zmqpp](https://github.com/zeromq/zmqpp).

There are two main channels of communication:

1. REQ/REP socket between a client and a server.
2. ROUTER/DEALER socket between peers.

```
    __________
   |  CLIENT  |
   |          |
   |          |
   |          |
   |___/|\____|
        |
       REQ
     (connect)
        |
        |
        |
        |
        |
        |
       REP
      (bind)
        |
    ___ | _____                                 ___________
   |   \|/     |  DEALER               ROUTER  |  DAEMON   |
   |           | (connect)              (bind) |           |
   |         ------------------------------------>         |
   |           |                               |           |
   |           |                               |           |
   | DAEMON  <------------------------------------         |
   |__________ | ROUTER                DEALER  |___________|
                 (bund)               (connect)
```

Multiple daemons and clients can run on a single computer and the communication
ports can be specified on the command line or not, in which case a sensible
default is attempted.

## Multi-threading

The daemon is threaded so it can use multiple CPU cores. At this time, there
are two different threads

1. P2P, used to communicate with peers. (Source code associated with this
   thread is labeled by `p2p`)
2. DB, used for dealing with the ad database. (Source code associated with this
   thread is labeled by `db`.)

Synchronization between the threads are facilitated by
`std::condition_variable`s and communication between the threads are done via
zmq's [inproc](http://api.zeromq.org/master:zmq-inproc) transport using the
[exclusive pair](http://api.zeromq.org/master:zmq-socket) socket pattern.

```
   |    /      |                              |    /      |
   |    \      |                              |    \      |
   |    \      |                              |    \      |
   |    /      |                              |    /      |
   |    \      |  PAIR                 PAIR   |    \      |
   |           | (connect)            (bind)  |           |
   |   P2P   <--------------------------------->   DB     |
   |  THREAD   |                              |  THREAD   |
   |           |                              |           |
   |    \      |                              |    \      |
   |    /      |                              |    /      |
   |    \      |                              |    \      |
   |    /      |                              |    /      |
   |    \      |                              |    \      |
```

## Encrypted RPC

The (RPC) communication channel between client and server can be optionally
encrypted. Encryption is implemented via [CurveZMQ](http://curvezmq.org), which
is based on [CurveCP](https://curvecp.org/), using the _Curve25519_ elliptic
curve. From the channel-security viewpoint, there are three ways to connect a
client to a server:

1. _grasslands_, which corresponds to no encryption, i.e., clear text,
2. _stonehouse_, which encrypts the channel but allows any client to connect,
   and
3. _ironhouse_, which encrypts the channel and only allows a set of
   authorized clients to connect.

These names corresponds to those coined by [Pieter
Hintjens](http://hintjens.com/blog:49) of ZMQ. The grasslands protocol is
really only useful if the client and server are running on the same physical
machine, since messages are sent via clear text. Stonehouse or ironhouse should
be used if the client and server communicate accross the network.

### Runnging client/server in _grasslands_ (no-security) mode

This is the simplest, neither the server nor the client require any extra
command line arguments:
```sh
$ ./piac-daemon
$ ./piac-cli
```

You either run the daemon in the background using `--detach`, or run it in its
own terminal.

### Runnging client/server in _stonehouse_ (encrypted) mode

This sets up an encrypted channel between server and client and the server only
allows clients that have the server's public key. Only encrypted connections
are allowed. There are two ways to configure stonehouse:

#### 1. Stonehouse using self-generated keys
The simplest way to setup stonehouse is to let the server generate its own
public key, saved into a file, which then the client can read:

```sh
$ ./piac-daemon --rpc-secure
$ ./piac-cli --rpc-secure --rpc-server-public-key-file stonehouse.pub
```

#### 2. Stonehouse using externally-generated keys
If you want to specify your own public key, you can generate it with the
`curve_keygen` executable:
```
$ ./curve_keygen 
=== CURVE PUBLIC KEY ===
Rs:L&e&Y:}q}=UBluws5TB/H[f?(k=Okb6XUE112
=== CURVE SECRET KEY ===
gvi&6Bq&xMpm8WjmW!nmi}vELzrS^0:%Zg4gt1z*
```
which then you can feed to the server and the client:
```sh
$ echo 'Rs:L&e&Y:}q}=UBluws5TB/H[f?(k=Okb6XUE112' > server.pub
$ echo 'gvi&6Bq&xMpm8WjmW!nmi}vELzrS^0:%Zg4gt1z*' > server
$ ./piac-daemon --rpc-secure --rpc-server-public-key-file server.pub --rpc-server-secret-key-file server
$ ./piac-cli --rpc-secure --rpc-server-public-key-file server.pub
```

### Running client/server in _ironhouse_ (encrypted & authenticated) mode

The is the best from the security viewpoint, the server only allows encrypted
and authenticated connections from a set predefined clients.

First, generate keypairs for both the server and the client:
```
$ ./curve_keygen 
=== CURVE PUBLIC KEY ===
5![.0HCW=JSjA>YVGbcerGwRiQBX4t?u$PE}v=p9
=== CURVE SECRET KEY ===
lI}y&OTD9)LK15gA&#[Xt=&N=gca}lLJG8j}%*L9
$ ./curve_keygen
=== CURVE PUBLIC KEY ===
cb{B{CJwNf/5K%?[Vbb!FVTYAs:PfX}K?yMh9Fu{
=== CURVE SECRET KEY ===
:}@kZX)=5fnSa9tRcZ77NE!Hb)P014}o/<11DiSK
```
Then setup the server and the client:
```sh
$ echo '5![.0HCW=JSjA>YVGbcerGwRiQBX4t?u$PE}v=p9' > server.pub
$ echo 'lI}y&OTD9)LK15gA&#[Xt=&N=gca}lLJG8j}%*L9' > server
$ echo 'cb{B{CJwNf/5K%?[Vbb!FVTYAs:PfX}K?yMh9Fu{' > client.pub
$ echo ':}@kZX)=5fnSa9tRcZ77NE!Hb)P014}o/<11DiSK' > client
$ echo 'cb{B{CJwNf/5K%?[Vbb!FVTYAs:PfX}K?yMh9Fu{' > authorized_clients
$ ./piac-daemon --rpc-secure --rpc-server-public-key-file server.pub --rpc-server-secret-key-file server --rpc-authorized-clients-file authorized_clients
$ ./piac-cli --rpc-secure --rpc-server-public-key-file server.pub --rpc-client-public-key-file client.pub --rpc-client-secret-key-file client
```
Note that the file `authorized_clients` can contain multiple clients' public
keys, a new on on each line.

__Note:__ Always use an encrypted communication channel to distribute keys,
e.g., via rsync or scp.
