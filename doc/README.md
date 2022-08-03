# Overall status

Basic p2p communication among multiple daemons works and routinely tested. Some
details:

Networking and threading code is fully asynchronous.

The daemon is threaded, i.e., can use multiple cores: one thread interrogates
the database, another connects with peers.

## RPC: basic server/client communication

A daemon listens on a socket and accepts connections from potentially multiple
clients.

RPC connections between clients and server can be optionally securely
authenticated and encrypted using elliptic-curve cryptography.

Clients can connect to the server and query to (1) issue basic searches with
the full features of xapian, e.g., multiple languages, synonyms, ranking, etc.,
(2) post an ad (or multiple ads in a single command) recorded by the daemon to
the database (testing this routinely with 500 randomly-generated ads for now),
(3) list hashes of ads (used to identify an ad and ensuring uniqueness among
peers), and (4) query peers a daemon is connected to at the moment.

## P2P: basic peer-to-peer communication

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

Users use the client to authenticate a user (as a seller or a buyer).
Authentication is done via generating a new, or using an existing, monero
wallet's mnemonic seed. This seed should be kept secret by each user, as that's
not only their single id but also their wallet.

After user authentication, users can post ads, currently in the form of json.

A hash of the user's wallet's primary key is attached to the ad in the
database.

Ads can only be added using a user id (monero wallet primary key).

Only the author of the ad can delete ads from the databsae.

Changes to the database are automatically synced among peers.

All of the above is regression tested.

The database currently only stores text; images are next.

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
default is attempted. To find out what command line arguments are accepted, run:
```sh
$ piac-daemon --help
jbakosi@sprout:~/code/piac/build:master$ ./piac-daemon --help
piac: piac-daemon v0.1.0-release

Usage: piac-daemon [OPTIONS]

OPTIONS
  --db <directory>
         Use database, default: piac.db

  --detach
         Run as a daemon in the background

  --help
         Show help message

  --log-file <filename.log>
         Specify log filename, default: piac-daemon.log

  --log-level <[0-4]>
         Specify log level: 0: minimum, 4: maximum

  --max-log-file-size <size-in-bytes> 
         Specify maximum log file size in bytes. Default: 104850000. Once the log file
         grows past that limit, the next log file is created with a UTC timestamp postfix
         -YYYY-MM-DD-HH-MM-SS. Set --max-log-file-size 0 to prevent piac-cli from managing
         the log files.

  --max-log-files <num> 
         Specify a limit on the number of log files. Default: 50. The oldest log files
         are removed. In production deployments, you would probably prefer to use
         established solutions like logrotate instead.

  --peer <hostname>[:port]
         Specify a peer to connect to

  --rpc-bind-port <port>
         Listen on RPC port given, default: 55090

  --rpc-secure
         Enable authentication and secure connection to clients.

  --rpc-public-key-file <filename>
         Save public key to file. Need to also set --rpc-secure.

  --p2p-bind-port <port>
         Listen on P2P port given, default: 65090

  --version
         Show version information

$ piac-cli --help
piac: piac-cli v0.1.0-release

Usage: piac-cli [OPTIONS]

OPTIONS
  --help
         Show help message

  --log-file <filename.log>
         Specify log filename, default: piac-cli.log

  --log-level <[0-4]>
         Specify log level: 0: minimum, 4: maximum

  --max-log-file-size <size-in-bytes> 
         Specify maximum log file size in bytes. Default: 104850000. Once the log file
         grows past that limit, the next log file is created with a UTC timestamp postfix
         -YYYY-MM-DD-HH-MM-SS. Set --max-log-file-size 0 to prevent piac-cli from managing
         the log files.

  --max-log-files <num> 
         Specify a limit on the number of log files. Default: 50. The oldest log files
         are removed. In production deployments, you would probably prefer to use
         established solutions like logrotate instead.

  --rpc-public-key <key>
         Specify server's public key. This is to facilitate authentication and encrypted
         communication to server.

  --rpc-public-key-file <filename>
         Specify filename containing server's public key. --rpc-public-key takes
         precedence if also specified.

  --version
         Show version information
```
and issue the 'help' command within the CLI:
```sh
$ piac-cli
piac-cli> help
COMMANDS
      server <host>[:<port>] [<public-key>]
                Specify server to send commands to. The <host> argument specifies
                a hostname or an IPv4 address in standard dot notation.
                The optional <port> argument is an integer specifying a port. The
                default is localhost:55090. The optional public-key is the server's
                public key to use for authenticated and secure connections.

      db <command>
                Send database command to daemon. Example db commands:
                > db query cat - search for the word 'cat'
                > db query race -condition - search for 'race' but not 'condition'
                > db add json <path-to-json-db-entry>
                > db rm <hash> - remove document
                > db list - list all documents
                > db list hash - list all document hashes
                > db list numdoc - list number of documents
                > db list numusr - list number of users in db

      exit, quit, q
                Exit

      help
                This help message

      keys
                Show monero wallet keys of current user

      new
                Create new user identity. This will generate a new monero wallet
                which will be used as a user id when creating an ad or paying for
                an item. This wallet can be used just like any other monero wallet.
                If you want to use your existing monero wallet, see 'user'.

      peers
                List server peers

      user [<mnemonic>]
                Show active monero wallet mnemonic seed (user id) if no mnemonic is
                given. Switch to mnemonic if given.

      version
                Display piac-cli version
piac>
```

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
