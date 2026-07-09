# Quant Trade System Architecture & Pipeline Workflow

This document explains the end-to-end flow, execution logic, and responsibilities of each component in the HFT (High-Frequency Trading) data pipeline and ML prediction loop.

---

## 1. System Topology & Data Flow

```mermaid
graph TD
    subgraph C++ Exchange Simulator [exchange-sim]
        ME[Matching Engine] -->|Quotes| MM[Market Maker Agent]
        ME -->|Orders| NT[Noise Trader Agent]
        ME -->|Executions/Quotes| WSP[WebSocket Publisher :8080]
    end

    subgraph Go Backend Server [backend-go]
        WSC[WebSocket Client] <-- Connects to :8080 --- WSP
        WSC -->|Raw JSON Queue| Workers[8x Concurrent Ingestion Workers]
        Workers -->|Parse & Validate| VAL[Tick Validator]
        Workers -->|Async Queue| PW[Parquet Storage Writer]
        Workers -->|Valid Ticks/Trades| HUB[Broadcast Hub]
        HUB -->|WS Gateway :8081| WSG[WebSocket Gateway]
        HUB -->|gRPC Server :9090| GRPC[gRPC Market Data Service]
    end

    subgraph Python ML Pipeline [ml/]
        REC[Data Recorder] <-- Subscribe to :8081 --- WSG
        REC -->|Store Training Data| PAR[Parquet Files]
        
        INF[Inference Server :50051] -->|Load XGBoost Model| ML[model.pkl]
    end

    subgraph C++ Strategy Engine [core-cpp]
        STRAT[SimpleSpreadStrategy] -->|gRPC Query| INF
        STRAT -->|Execution Orders| ME
    end
```

---

## 2. Component breakdown

### 1. C++ Exchange Simulator (`exchange-sim`)
The exchange simulator acts as the mock matching engine matching buyers and sellers at high speeds.
* **Matching Engine**: Maintains an order book structure (bids and asks) with price-time priority.
* **Market Maker Agent**: Simulates a liquidity provider by adding/updating limit bids and asks on every interval (e.g. `1ms`) around a reference price to establish market depth.
* **Noise Trader Agent**: Simulates retail flow by placing random market orders at randomized microsecond intervals (`--noise-interval-us`), matching against active quotes to generate executions.
* **WebSocket Publisher (`WsPublisher`)**: Exposes a low-latency socket server on port `8080`. Every order book snapshot update (`tick`) and matched execution (`trade`) is broadcast as a JSON frame.

---

### 2. Go Backend Ingestion Server (`backend-go`)
A high-throughput server that acts as a middleware pipeline to validate and log exchange data.
* **WebSocket Ingestor (`ws_client.go`)**: Opens a persistent connection to the C++ simulator and pulls raw JSON events into an internal message buffer.
* **8x Parallel Worker Pool (`ingestor.go`)**: Decodes messages concurrently across 8 threads. Ticks/trades are processed in single-pass JSON unmarshals to distribute CPU parsing load.
* **Tick Validator (`validator.go`)**: Checks for crossed books (bids $\ge$ asks), negative prices/sizes, sequence gaps, and anomalous price jumps.
* **Asynchronous Parquet Writer (`parquet_writer.go`)**: Gathers verified ticks in RAM. When the buffer size limit is reached, a background worker flushes them to disk as binary `.parquet` log files for machine learning offline training.
* **Broadcast Hub (`hub.go`)**: Routes incoming data to:
  * **gRPC Server (`:9090`)**: Exposes streaming tick feeds and historical Parquet lookups.
  * **HTTP/WS Gateway (`:8081`)**: Exposes `/ws/market-data` and `/ws/trades` for downstream consumers.

---

### 3. Python ML Pipeline (`ml/`)
Processes raw logged data and offers predictions for strategy execution.
* **Data Recorder (`recorder.py`)**: Subscribes to the Go WebSocket gateway on `:8081` to capture tick updates and save raw features.
* **Inference Server (`server.py`)**: Hosts a gRPC server on port `50051`. It loads a serialized XGBoost model (`model.pkl`).
* **Predictor (`predictor.py`)**: Receives high-frequency features (such as bids, asks, bid sizes, and ask sizes) via gRPC and predicts the next price movement direction (UP/DOWN) and probability.

---

### 4. C++ Strategy Engine (`core-cpp`)
Executes trading strategies.
* **SimpleSpreadStrategy**: Tracks current ticks from the matching engine, queries the Python inference server over gRPC (`:50051`) for price predictions, and submits optimized orders to the simulator.

---

## 3. How to Run & Control the Pipeline

The project uses a unified orchestrating `Makefile` in the root folder to handle the lifecycle of all services:

| Command | Action |
|---|---|
| `make run` | Compiles C++ and Go, starts all 4 components (ML Server, Exchange Sim, Go Backend, ML Recorder) in the background. |
| `make tail` | Monitor stdout/stderr logs from all running services live. |
| `make stop` | Gracefully terminates all background simulator, backend, and ML processes. |
| `make clean` | Removes C++ builds, Go binaries, and all logs. |
