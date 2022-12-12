# piac communication infrastructure

This documents outlines the communication infrastructure within piac daemons
(servers) and clients.

## Client/server architecture

The piac app consists of a daemon (or server) and a client. The simplest way of
running piac is to run a daemon and a client on the same computer. However,
running a daemon is not strictly necessary: if you know the address and port of
an existing running instance of the daemon, you can connect your client to it.
In the following the words _daemon_, _server_, or _peer_ are used
interchangeably.

## RPC: server/client communication

A daemon listens on a socket and accepts connections from potentially multiple
clients.

RPC connections between clients and server can be optionally securely
authenticated and encrypted using elliptic-curve cryptography.

Clients can connect to the server and query to (1) issue basic searches with
the full [feature-set](https://xapian.org/features) of xapian, e.g., multiple
languages, synonyms, ranking, etc., (2) post an ad (or multiple ads in a single
command) recorded by the daemon to the database (testing this routinely with
500 randomly-generated ads for now), (3) list hashes of ads (used to identify
an ad and ensuring uniqueness among peers), and (4) query peers a daemon is
connected to at the moment.

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

## Communication via ZMQ sockets

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
                 (bind)               (connect)
```

Multiple daemons and clients can run on a single computer and the communication
ports can be specified on the command line or not, in which case a sensible
default is attempted.

## Multi-threading and communication via ZMQ

### Daemon threads

The daemon is threaded and it can use multiple CPU cores if available. There
are two different threads, spawned after all initialization is complete:

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

### Command line interface client threads

The command line interface client (cli) is also threaded and will use multiple
CPU cores if available. There are two threads spawned when a user connects to a
matrix server:

1. MTX, interfacing with a matrix server to facilitate communication via the
   matrix protocol. (Source code associated with this thread is labeled by
   `mtx`)
2. MSG, used to receive and store messages that the matrix client thread
   receives via matrix. (Source code associated with this thread is labeled by
   `msg`.)

Communication between the mtx and msg threads is facilitated by
zmq's [inproc](http://api.zeromq.org/master:zmq-inproc) transport using the
[exclusive pair](http://api.zeromq.org/master:zmq-socket) socket pattern.

```
                       |    /    |                    |    /    |          |    /      |
                       |    \    |                    |    \    |          |    \      |
                       |    \    |                    |    \    |          |    \      |
                       |    /    |                    |    /    |          |    /      |
  ___________          |    \    |                    |    \    |          |    \      |
 |           |         |    /    |                    |    /    |          |    /      |
 | MTXCLIENT |         |    \    |  PAIR        PAIR  |    \    |          |    \      |
 |    LIB    |  sync   |         | (connect)   (bind) |         |          |           |
             ----------->   MTX  ---------------------->  MSG   ------------>  CLI     |
 |           |         |  THREAD |                    |  THREAD |          |  THREAD   |
 |____/|\____|         |         |                    |         |          |  (user)   |
       |               |    \    |                    |    \    |          |           |
       |               |    /    |                    |    /    |          |    /      |
  ____ | ____          |    \    |                    |    \    |          |    \      |
 |    \|/    |         |    /    |                    |    /    |          |    /      |
 |           |         |    \    |                    |    \    |          |    \      |
 |  MATRIX   |         |    /    |                    |    /    |          |    /      |
 |  SERVER   |
 |___________|
```

When the user disconnects from the matrix server, the threads are finished and
recreated for a new matrix login. The threads are spawned and handled by the
main CLI thread which interacts with the user.
