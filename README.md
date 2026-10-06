# ⚡ CollabEditor: Real-Time CRDT Collaborative Engine

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![WebSockets](https://img.shields.io/badge/Networking-uWebSockets-orange.svg)](https://github.com/uNetworking/uWebSockets)
[![Architecture](https://img.shields.io/badge/Sync_Engine-RGA_CRDT-brightgreen.svg)](#architecture)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

# collab-editor

A production-grade, real-time collaborative text editor powered by an **RGA (Replicated Growable Array) CRDT** — the same class of algorithm behind Google Docs and Figma's text engine. Multiple users can type simultaneously on the same document with zero conflicts and guaranteed convergence, no locks or server-side merge logic required.

---

## What is a CRDT and why does it matter here?

In a naive collaborative editor, if User A in Gorakhpur and User B in Tokyo both insert a character at position 5 at the same millisecond, the server receives two operations in different orders depending on network delay. The documents diverge.

An **RGA CRDT** solves this by giving every inserted character a globally unique, causally ordered identity:

```
NodeID = ⟨ Lamport Timestamp, Client ID, Local Sequence ⟩
```

Characters are never re-indexed when text changes. Insert and delete operations are commutative and idempotent — apply them in any order, on any replica, and every peer converges to the same document. This property is called **Strong Eventual Consistency (SEC)**.

---

## Architecture

```
 ┌──────────────────────────────────────────────┐
 │              BROWSER CLIENT                  │
 │  React 18 + TypeScript + Optimistic Engine   │
 └──────────────────────┬───────────────────────┘
                        │
                        │  34-byte Binary WebSocket Frames
                        ▼
 ┌──────────────────────────────────────────────┐
 │                NGINX PROXY                   │
 │     TLS Termination + WebSocket Upgrades     │
 └──────────────────────┬───────────────────────┘
                        │
                        ▼
 ┌──────────────────────────────────────────────┐
 │             C++17 BACKEND ENGINE             │
 │    uWebSockets Async Reactor (Thread-Safe)   │
 ├──────────────────────────────────────────────┤
 │  Multi-Room Document Manager                 │
 │  Causal Operations Buffer                    │
 │  Memory-Mapped Write-Ahead Log (WAL)         │
 └──────────────────────────────────────────────┘
```

### Key subsystems

**Dual CRDT engines.** The TypeScript client engine applies edits locally before they hit the network — zero-latency rendering. The C++ server engine acts as the authoritative state validator, clock manager, and pub/sub broadcaster.

**Binary wire protocol.** Every operation is encoded as a fixed 34-byte `#pragma pack(push, 1)` frame. Approximately 10× smaller than equivalent JSON, with no parsing overhead or garbage collection pressure.

**Causal buffering.** If an operation arrives referencing a character that hasn't been received yet ("insert X after Y, but Y is still in transit"), the engine parks X in a pending buffer and splices it in automatically the moment Y arrives. This makes the system correct on high-latency and lossy networks.

**Write-Ahead Log.** All edits are sequentially appended to a binary disk log. On server restart, the document state is reconstructed from the WAL with no data loss.

**Multi-room document manager.** Each document gets an isolated WebSocket room at `/ws/{documentId}`. Rooms are created and destroyed dynamically.

---

## Keystroke lifecycle

```
[User types 'A']
  → 1.  Local insert in client RGA list
  → 2.  Render instantly on screen  (0ms UI lag)
  → 3.  Encode to 34-byte ArrayBuffer
  → 4.  Send over WebSocket
  → 5.  C++ server receives binary frame
  → 6.  Server advances Lamport clock, applies to server RGA
  → 7.  Append operation to WAL
  → 8.  Broadcast frame to all room peers
  → 9.  Remote clients decode and integrate into their local RGA
```

---

## Performance targets

| Concern | Approach | Outcome |
|---|---|---|
| Conflict resolution | RGA deterministic ordering via Lamport + Client ID tie-breaking | Guaranteed convergence across all replicas |
| Payload size | Fixed 34-byte packed binary frames | ~10× smaller than JSON equivalents |
| Network reordering | Causal dependency buffering | Correct on lossy and out-of-order networks |
| Local edit cost | Memory index + doubly-linked list | O(1) cursor ops; O(N) full render |
| Server throughput | Non-blocking uWebSockets event loop | Tens of thousands of concurrent WS frames/sec |

---

## Project structure

```
collab-editor/
├── server/                  # C++17 backend
│   ├── include/
│   │   ├── node_id.h        # NodeID type and ordering
│   │   ├── rga_node.h       # Individual RGA list node
│   │   ├── rga_list.h       # Core RGA sequence structure
│   │   ├── crdt_engine.h    # Engine: clock, buffer, broadcast
│   │   ├── spsc_queue.h     # Lock-free single-producer queue
│   │   ├── wal.h            # Write-ahead log
│   │   ├── document_manager.h
│   │   ├── session_manager.h
│   │   ├── protocol.h       # 34-byte binary wire format
│   │   └── config.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── server.cpp
│   │   ├── document_manager.cpp
│   │   └── session_manager.cpp
│   ├── tests/
│   │   ├── test_node_id.cpp       # NodeID ordering invariants
│   │   ├── test_rga.cpp           # List manipulation
│   │   ├── test_engine.cpp        # Causal buffer behaviour
│   │   ├── test_convergence.cpp   # Multi-replica random shuffle
│   │   └── test_protocol.cpp      # Binary encode/decode round-trips
│   └── third_party/uWebSockets/
│
├── client/                  # React 18 + TypeScript frontend
│   └── src/
│       ├── crdt/            # Client-side RGA engine
│       │   ├── NodeID.ts
│       │   ├── RGANode.ts
│       │   ├── RGADocument.ts
│       │   └── CRDTEngine.ts
│       ├── network/         # WebSocket + binary protocol
│       │   ├── WireProtocol.ts
│       │   ├── ConnectionManager.ts
│       │   └── SyncManager.ts
│       ├── editor/          # Editor UI
│       │   ├── Editor.tsx
│       │   ├── EditorState.ts
│       │   ├── Cursor.tsx
│       │   ├── CursorOverlay.tsx
│       │   ├── SelectionHighlight.tsx
│       │   └── Toolbar.tsx
│       └── components/      # App shell
│           ├── DocumentList.tsx
│           ├── ShareDialog.tsx
│           ├── UserPresence.tsx
│           ├── Header.tsx
│           └── LoadingSpinner.tsx
│
├── nginx/                   # Reverse proxy config
├── deploy/                  # Fly.io + Docker Compose prod configs
├── docker-compose.yml
└── Makefile
```

---

## Running locally

**Prerequisites:** Docker, Docker Compose

```bash
git clone https://github.com/Ashiii27/collab-editor
cd collab-editor
make dev
```

This builds and starts the C++ server, React client, and Nginx proxy via Docker Compose. Open `http://localhost` in two browser windows and type in either — changes appear in both in real time.

**Run the C++ test suite:**

```bash
make test
```

Covers NodeID ordering, RGA list manipulation, causal out-of-order buffering, binary protocol round-trips, and multi-replica convergence under random operation shuffles.

---

## Deployment

The `deploy/` directory contains Fly.io manifests and a production Docker Compose file. To deploy to Fly.io:

```bash
fly launch --config deploy/fly.toml
fly deploy
```

The production compose file (`deploy/docker-compose.prod.yml`) wires up TLS termination through Nginx and mounts a persistent WAL volume on the server container.

---

## Tech stack

| Layer | Technology |
|---|---|
| Backend engine | C++17, uWebSockets, CMake |
| Persistence | Memory-mapped binary WAL |
| Frontend | React 18, TypeScript, Vite |
| Proxy | Nginx |
| Containerisation | Docker, Docker Compose |
| Deployment | Fly.io |

---

## References

- Roh et al., *Replicated abstract data types: Building blocks for collaborative applications* (2011) — original RGA paper
- Shapiro et al., *A comprehensive study of Convergent and Commutative Replicated Data Types* (2011) — SEC formal definition
- [uWebSockets](https://github.com/uNetworking/uWebSockets)