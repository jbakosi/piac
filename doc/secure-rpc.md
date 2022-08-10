# Encrypted RPC in piac

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

## Runnging client/server in _grasslands_ (no-security) mode

This is the simplest, neither the server nor the client require any extra
command line arguments:
```sh
$ ./piac-daemon
$ ./piac-cli
```

You either run the daemon in the background using `--detach`, or run it in its
own terminal.

## Runnging client/server in _stonehouse_ (encrypted) mode

This sets up an encrypted channel between server and client and the server only
allows clients that have the server's public key. Only encrypted connections
are allowed. There are two ways to configure stonehouse:

### 1. Stonehouse using self-generated keys
The simplest way to setup stonehouse is to let the server generate its own
public key, saved into a file, which then the client can read:

```sh
$ ./piac-daemon --rpc-secure
$ ./piac-cli --rpc-secure --rpc-server-public-key-file stonehouse.pub
```

### 2. Stonehouse using externally-generated keys
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

## Running client/server in _ironhouse_ (encrypted & authenticated) mode

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
The file `authorized_clients` can contain multiple clients' public keys, a new
on on each line.

__Note:__ Always use an encrypted communication channel to distribute keys,
e.g., via rsync or scp.
