# piac client/server architecture

The piac app consists of a daemon (or server) and a client. The simplest way of
running piac is to run a daemon and a client on the same computer. However,
running a daemon is not strictly necessary: if you know the address and port of
an existing running instance of the daemon, you can connect your client to it.
In the following the words _daemon_, _server_, or _peer_ are used
interchangeably.

## communication

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
default is attempted. To find out what command line arguments are accepted, run:
```sh
$ piac-daemon --help
...
$ piac-cli --help
...
```
and issue the 'help' command within the CLI:
```sh
$ piac-cli
piac-cli> help
...
```

## multi-threading

The daemon is threaded so it can use multiple CPU cores. At this time, there
are two different threads

1. RPC, used to communicate with peers. (Source code associated with this
   thread is labeled by `rpc`, for _remote procedure call_ or _remote process
   communication_.)
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
   |   RPC   <--------------------------------->   DB     |
   |  THREAD   |                              |  THREAD   |
   |           |                              |           |
   |    \      |                              |    \      |
   |    /      |                              |    /      |
   |    \      |                              |    \      |
   |    /      |                              |    /      |
   |    \      |                              |    \      |
```
