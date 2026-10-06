FROM gcc:13-bookworm AS builder

RUN apt-get update \
    && apt-get install --no-install-recommends -y cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCOLLAB_EDITOR_BUILD_TESTS=OFF \
    && cmake --build build --parallel \
    && install -Dm755 build/collab_server /out/collab_server

FROM debian:bookworm-slim
COPY --from=builder /out/collab_server /usr/local/bin/collab_server
EXPOSE 9001
ENTRYPOINT ["/usr/local/bin/collab_server"]
