# Distributed KV Store

A small distributed key-value store written in C. One leader, dynamic
followers, a friendly interactive client. Designed to be runnable on two
laptops over a LAN with no IPs to memorize.

## Build

```sh
make
```

Produces a single binary: `./kv`.

## Quick start

```sh
./kv                                 # interactive menu — pick leader / follower / client
```

Or skip the menu with subcommands:

```sh
./kv leader   <port>                 # start a leader on <port>
./kv follower <leader_ip> <port>     # connect a follower to a leader
./kv client   <leader_ip> <port>     # interactive REPL against a leader
```

When you start a leader, it prints your machine's IP(s) and the exact commands
the other machine should run.

## REPL example

```
$ ./kv client 192.168.1.10 5980
Connected to leader at 192.168.1.10:5980
  (followers connected: 2)

Type: put <key> <value>  |  get <key>  |  del <key>  |  quit

kv> put name Ali
OK (replicated to 2 followers)
kv> get name
Ali
kv> del name
OK (replicated to 2 followers)
kv> quit
Goodbye.
```

## Optional token authentication

Set `KV_TOKEN` on every node that should be allowed to participate:

```sh
KV_TOKEN=mysecret ./kv leader 5980
KV_TOKEN=mysecret ./kv follower 192.168.1.10 5980
KV_TOKEN=mysecret ./kv client   192.168.1.10 5980
```

If the leader has `KV_TOKEN` set, any connection that doesn't send the matching
token first is closed immediately. With no `KV_TOKEN`, the leader accepts any
connection (the original behavior).

## What's supported

- Concurrent reads, exclusive writes (`pthread_rwlock_t`).
- Disk persistence (`database.txt`) — every mutation is durable before ACK.
- One leader + N followers; followers register dynamically.
- Live PUT / DEL replication from leader to all followers.
- Full state sync to a follower on first connect *or* on reconnect.
- Reconnect-with-backoff if the leader disappears; leader is unaffected by
  followers vanishing (dead fds are pruned on the next replication write).
- Optional token auth.

## Wire protocol (for reference)

Plaintext, line-terminated (`\n`):

| Direction | Message |
|---|---|
| Client → server | `PUT <key> <value>` |
| Client → server | `GET <key>` |
| Client → server | `DEL <key>` |
| Client → server | `AUTH <token>` |
| Follower → leader | `REGISTER` |
| Server → client | `COMPLETE` / `NOT_FOUND` / `KEY NOT FOUND` / `INVALID COMMAND!` |
| Server → client | `FOLLOWERS <count>` (appended after PUT/GET/DEL) |
| Server → client | `AUTH_REQUIRED` / `AUTH_FAILED` / `OK` |
| Server → client | `WELCOME to Distributed-KV v1.0 (followers connected: <N>)` |

The REPL (`./kv client ...`) hides all of this; it's documented here only if
you want to script against it directly.

## Run with Docker

Build once on each machine (or build once, push to a registry, pull on the other):

```sh
docker build -t kv .
```

**Leader (laptop A):**
```sh
docker run --rm -p 5980:5980 -v $(pwd)/data:/data kv leader 5980
```
- `-p 5980:5980` publishes the container's port so laptop B can reach it.
- `-v $(pwd)/data:/data` keeps `database.txt` on the host so it survives `docker stop`. Drop the `-v` if you don't care about persistence.

**Follower (laptop B):**
```sh
docker run --rm -v $(pwd)/data:/data kv follower <A_IP> 5980
```

**Client (anywhere):**
```sh
docker run --rm -it kv client <A_IP> 5980
```
The `-it` keeps the terminal attached so the REPL works.

**With token auth:**
```sh
docker run --rm -p 5980:5980 -e KV_TOKEN=mysecret -v $(pwd)/data:/data kv leader 5980
docker run --rm           -e KV_TOKEN=mysecret -v $(pwd)/data:/data kv follower <A_IP> 5980
docker run --rm -it       -e KV_TOKEN=mysecret kv client <A_IP> 5980
```

One container per machine. No `docker compose` and no internal Docker networking — the leader's port is published on the host, and followers reach it across the LAN exactly like the bare binary.

## ⚠️ Security notice

This KV store uses **plaintext TCP with no encryption**. Token authentication
(when `KV_TOKEN` is set) prevents casual unauthorized access, but the token
itself is visible to anyone sniffing the network. **Only run this on trusted
private networks. Do not expose it to the public internet.** Real production
deployments would need TLS, which is out of scope here.

## Layout

| File | Role |
|---|---|
| `main.c` | Entry point, subcommand dispatch, interactive menu, IP autodetect. |
| `kv_store.{c,h}` | Hash table + rwlock + full-state-sync helper. |
| `network.c` | TCP listener, per-client thread, `REGISTER` handoff, AUTH gate. |
| `replication.c` | Leader fan-out, follower main loop (connect / AUTH / REGISTER / stream / reconnect). |
| `followers.{c,h}` | Thread-safe registry of follower socket fds. |
| `persistence.c` | `database.txt` read/write. |
| `client.c` | Interactive REPL — translates lowercase commands to the wire protocol. |
| `Makefile` | Builds `kv`. |
