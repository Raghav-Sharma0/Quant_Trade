# Single-stage Dockerfile for C++ Exchange Simulator to guarantee library matching
FROM alpine:3.20

# Install C++ toolchain and library development files
RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    protobuf-dev \
    grpc-dev \
    c-ares-dev \
    re2-dev \
    openssl-dev \
    git

WORKDIR /app

# Copy the core C++ headers and simulator source files
COPY core-cpp/ /app/core-cpp/
COPY exchange-sim/ /app/exchange-sim/

# Compile the exchange simulator
WORKDIR /app/exchange-sim
RUN cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

# Expose the websocket publishing port
EXPOSE 8080

ENTRYPOINT ["/app/exchange-sim/build/exchange_sim"]
