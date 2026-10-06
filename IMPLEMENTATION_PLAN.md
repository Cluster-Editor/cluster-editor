# CollabEditor Implementation Plan

This document turns the architecture described in [`README.md`](README.md) into an incremental, file-by-file delivery plan.

> **Current state:** M0 is implemented. The repository now has a CMake build definition, a minimal C++17 foundation executable, a smoke test, Make targets, Docker/Compose scaffolding, an Nginx configuration, and subsystem documentation. M1–M6 remain planned.

## 1. Delivery principles

1. Keep the client and server wire formats identical from the first protocol implementation.
2. Implement and test the CRDT as a deterministic library before connecting networking or UI code.
3. Make every operation idempotent so reconnects and duplicate frames are safe.
4. Add persistence only after in-memory convergence is proven.
5. Keep deployment configuration reproducible through Docker Compose and environment-based configuration.
6. Add tests with each subsystem instead of postponing all validation to the end.

## 2. Milestones

| Milestone | Outcome | Exit criteria |
| --- | --- | --- |
| M0 — Repository foundation | Build, formatting, and local developer commands | Empty project builds and test commands are wired |
| M1 — Shared protocol | Stable NodeID and 34-byte operation frame | C++ and TypeScript round-trip the same bytes |
| M2 — Core CRDT | Deterministic insert/delete/convergence engine | Randomly shuffled operations converge in all replicas |
| M3 — Durable backend | WAL-backed multi-document server | Restart restores documents without losing acknowledged operations |
| M4 — Client engine | Optimistic browser-side editing and sync | Local edits render immediately and reconcile after reconnect |
| M5 — Editor experience | Usable collaborative editor | Two browser sessions edit, select, and show presence correctly |
| M6 — Production delivery | Containerized, proxied, tested deployment | `make dev`, `make test`, and production compose work from a clean checkout |

**M0 status:** Complete. The foundation executable supports `--help`, `--version`, and validated `--port` input. The direct MinGW build and smoke test pass; CMake and Docker should be run once those tools are installed.

## 3. File-wise implementation plan

### M0 — Repository foundation

| File | Responsibility | Depends on |
| --- | --- | --- |
| `CMakeLists.txt` | Configure C++17 build, compiler warnings, dependencies, server target, and tests | None |
| `Makefile` | Provide `make dev`, `make build`, `make test`, `make format`, and `make clean` commands | `CMakeLists.txt`, Docker Compose |
| `docker-compose.yml` | Start backend, client, and Nginx services locally | Dockerfiles, Nginx config |
| `.gitignore` | Exclude build output, dependencies, logs, WAL data, and local environment files | None |
| `server/README.md` | Backend build and API notes | Backend files |
| `client/README.md` | Frontend setup and development notes | Client files |
| `deploy/README.md` | Deployment prerequisites and release process | Deployment files |
| `nginx/README.md` | Local and production proxy behavior | Nginx config |

### M1 — Shared identity and wire protocol

#### C++ backend

| File | Responsibility | Depends on |
| --- | --- | --- |
| `server/include/node_id.h` | Define `NodeID { lamport, client_id, sequence }`, equality, hashing, and total ordering | None |
| `server/include/protocol.h` | Define packed operation types, field widths, constants, and frame-size assertions | `node_id.h` |
| `server/src/protocol.cpp` | Encode/decode frames with bounds and version validation | `protocol.h` |
| `server/include/config.h` | Define port, WAL path, limits, and environment-backed defaults | None |

#### TypeScript client

| File | Responsibility | Depends on |
| --- | --- | --- |
| `client/package.json` | Declare React, TypeScript, Vite, test, and lint dependencies | None |
| `client/tsconfig.json` | Enable strict TypeScript compilation and path aliases | `package.json` |
| `client/vite.config.ts` | Configure the development server and production bundle | `package.json` |
| `client/src/crdt/NodeID.ts` | Mirror C++ NodeID representation, comparison, and serialization | None |
| `client/src/network/WireProtocol.ts` | Encode/decode the exact server frame layout using `ArrayBuffer`/`DataView` | `NodeID.ts` |

**M1 validation:** Add protocol fixtures shared by C++ and TypeScript. Verify field offsets, endianness, frame length, invalid-frame rejection, and encode/decode round trips.

### M2 — Core CRDT and convergence

#### C++ backend

| File | Responsibility | Depends on |
| --- | --- | --- |
| `server/include/rga_node.h` | Store character value, NodeID, parent ID, tombstone state, and links | `node_id.h` |
| `server/include/rga_list.h` | Maintain deterministic RGA ordering, insertion, deletion, traversal, and visible-text rendering | `rga_node.h` |
| `server/include/crdt_engine.h` | Own Lamport clock, applied-operation set, causal pending buffer, and RGA document | `rga_list.h`, `protocol.h` |
| `server/src/crdt_engine.cpp` | Implement operation validation, causal release, idempotency, and clock advancement | `crdt_engine.h` |
| `server/include/spsc_queue.h` | Provide a bounded single-producer/single-consumer queue for event handoff | None |

#### Client CRDT

| File | Responsibility | Depends on |
| --- | --- | --- |
| `client/src/crdt/RGANode.ts` | Represent a client-side character node and tombstone | `NodeID.ts` |
| `client/src/crdt/RGADocument.ts` | Implement deterministic list insertion, deletion, traversal, and text rendering | `RGANode.ts` |
| `client/src/crdt/CRDTEngine.ts` | Generate local operations, integrate remote operations, buffer causal gaps, and deduplicate | `RGADocument.ts`, `WireProtocol.ts` |

#### Tests

| File | Responsibility | Depends on |
| --- | --- | --- |
| `server/tests/test_node_id.cpp` | Verify ordering, equality, hashing, and boundary values | `node_id.h` |
| `server/tests/test_rga.cpp` | Verify inserts, deletes, tombstones, duplicate operations, and rendering | `rga_list.h` |
| `server/tests/test_engine.cpp` | Verify Lamport clocks, missing-parent buffering, and idempotency | `crdt_engine.h` |
| `server/tests/test_convergence.cpp` | Apply randomized operations in different orders to multiple replicas | `crdt_engine.h` |
| `client/src/crdt/*.test.ts` | Mirror core CRDT and protocol tests in the browser runtime | Client CRDT files |

**M2 exit criteria:** Any replica receiving the same valid operation set, in any order and with duplicates, produces the same visible document and metadata state.

### M3 — Durable multi-room backend

| File | Responsibility | Depends on |
| --- | --- | --- |
| `server/include/wal.h` | Define append-only WAL records, checksums, flushing, and recovery interfaces | `protocol.h` |
| `server/src/wal.cpp` | Append operations, recover valid records, truncate incomplete tails, and report corruption | `wal.h` |
| `server/include/document_manager.h` | Create, find, and remove isolated document rooms | `crdt_engine.h`, `wal.h` |
| `server/src/document_manager.cpp` | Coordinate room lifecycle, replay WAL state, and persist accepted operations | `document_manager.h` |
| `server/include/session_manager.h` | Track authenticated/identified WebSocket sessions and room membership | `protocol.h` |
| `server/src/session_manager.cpp` | Join/leave rooms, disconnect cleanup, and peer broadcast selection | `session_manager.h` |
| `server/include/server.h` | Declare WebSocket routes, lifecycle hooks, and request handling | Managers, `config.h` |
| `server/src/server.cpp` | Implement uWebSockets setup, binary frame handling, backpressure, and broadcasts | `server.h` |
| `server/src/main.cpp` | Parse configuration, initialize managers, start the reactor, and handle shutdown | `server.h`, `config.h` |
| `server/tests/test_wal.cpp` | Verify append, replay, restart recovery, checksum failure, and partial-tail handling | `wal.h` |
| `server/tests/test_document_manager.cpp` | Verify room isolation and lifecycle behavior | `document_manager.h` |
| `server/tests/test_server.cpp` | Exercise connection, join, broadcast, malformed frames, and disconnect flows | `server.h` |

**M3 exit criteria:** A client can connect to `/ws/{documentId}`, submit a valid operation, receive room broadcasts, and recover the document after a clean or interrupted restart.

### M4 — Client networking and synchronization

| File | Responsibility | Depends on |
| --- | --- | --- |
| `client/src/network/ConnectionManager.ts` | Connect, reconnect with backoff, detect closed connections, and expose connection state | `WireProtocol.ts` |
| `client/src/network/SyncManager.ts` | Send local operations, request document sync, queue offline edits, and reconcile acknowledgements | `ConnectionManager.ts`, `CRDTEngine.ts` |
| `client/src/hooks/useCollaboration.ts` | Bind engine/network state to React lifecycle and subscriptions | `SyncManager.ts` |
| `client/src/hooks/useDocument.ts` | Load a document ID, manage document state, and expose editor actions | `useCollaboration.ts` |
| `client/src/utils/ids.ts` | Generate stable client/session identifiers | None |
| `client/src/utils/errors.ts` | Normalize protocol, connection, and user-facing errors | None |
| `client/src/styles/tokens.css` | Define shared colors, spacing, typography, and presence colors | None |
| `client/src/styles/app.css` | Define responsive application layout and editor states | `tokens.css` |

**M4 validation:** Test reconnects, duplicate frames, out-of-order frames, offline edits, sync snapshots, and server rejection without losing local text.

### M5 — Editor and application UI

| File | Responsibility | Depends on |
| --- | --- | --- |
| `client/src/editor/EditorState.ts` | Maintain selection, cursor, local edit intent, and rendered text mapping | `CRDTEngine.ts` |
| `client/src/editor/Cursor.tsx` | Render the local cursor and selection caret | `EditorState.ts` |
| `client/src/editor/CursorOverlay.tsx` | Render remote user cursors with names and colors | Presence state |
| `client/src/editor/SelectionHighlight.tsx` | Render remote selections without changing document content | Presence state |
| `client/src/editor/Editor.tsx` | Render the editor, translate keystrokes to CRDT operations, and apply remote updates | Editor state, collaboration hook |
| `client/src/editor/Toolbar.tsx` | Provide document actions, connection state, and formatting controls | Editor state |
| `client/src/components/Header.tsx` | App title, document identity, and connection indicator | Collaboration hook |
| `client/src/components/DocumentList.tsx` | Select or create documents | Document hook |
| `client/src/components/ShareDialog.tsx` | Show/copy a shareable document URL | Router/document state |
| `client/src/components/UserPresence.tsx` | Show connected collaborators and colors | Session/presence state |
| `client/src/components/LoadingSpinner.tsx` | Reusable loading state | Styles |
| `client/src/App.tsx` | Compose routes, document loading, editor, and shell components | All UI components |
| `client/src/main.tsx` | Mount the React application and global styles | `App.tsx` |
| `client/index.html` | Browser entry document and metadata | None |

**M5 exit criteria:** Two browser windows can edit one document simultaneously, display remote changes, preserve selections as text changes, and show connection/presence state.

### M6 — Infrastructure, security, and release

| File | Responsibility | Depends on |
| --- | --- | --- |
| `server/Dockerfile` | Build a minimal backend image with CMake dependencies | Backend build |
| `client/Dockerfile` | Build the frontend and serve static assets | Client build |
| `nginx/nginx.conf` | Serve frontend, proxy `/ws/`, configure upgrade headers, timeouts, and TLS hooks | Client/server services |
| `deploy/docker-compose.prod.yml` | Define production services, health checks, volumes, and restart policies | Dockerfiles, Nginx |
| `deploy/fly.toml` | Define Fly.io app, ports, regions, health checks, and persistent storage | Production image |
| `deploy/.env.example` | Document non-secret runtime configuration | Backend/client config |
| `.github/workflows/ci.yml` | Run formatting, C++ build/tests, TypeScript checks, and Docker validation | All build files |
| `LICENSE` | Keep MIT licensing terms current | None |

Before production deployment, add authentication/authorization, document access control, input limits, rate limiting, WAL permissions, TLS, structured logging, and metrics. Do not commit real secrets or production `.env` files.

## 4. Recommended implementation order

1. Add M0 build files and a minimal compile/test target.
2. Implement `NodeID` and protocol serialization on both platforms.
3. Implement the C++ RGA and CRDT engine with convergence tests.
4. Port the same behavior to the TypeScript client engine.
5. Add WAL recovery and document/session managers.
6. Add the WebSocket server and integration tests.
7. Add client connection and synchronization managers.
8. Build the editor UI and presence features.
9. Add Docker, Nginx, deployment, CI, and security hardening.
10. Update `README.md` after each milestone so commands and file lists remain accurate.

## 5. Definition of done

- `make build` completes from a clean checkout.
- `make test` passes backend, protocol, client, and convergence tests.
- C++ and TypeScript encode identical wire frames.
- Concurrent inserts/deletes converge regardless of delivery order.
- Duplicate and delayed operations are safe.
- WAL replay restores acknowledged document state.
- Two clients can edit the same document through Nginx/WebSockets.
- Docker Compose starts a working local stack.
- CI runs on every pull request.
- No secrets, build artifacts, or dependency directories are committed.
