# Server

The backend foundation currently provides a small C++17 executable and a CTest smoke target. CRDT, WebSocket, persistence, and room-management files will be added in milestones M1–M3.

## Local build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the foundation executable with `build/collab_server --help` or `build/collab_server --version`.
