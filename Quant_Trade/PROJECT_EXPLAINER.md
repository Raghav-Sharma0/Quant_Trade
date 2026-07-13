# High-Frequency Trading (HFT) Data Pipeline & Machine Learning Explainer

This document serves as a guide for our project partners to understand the architecture, operation, and data flow of this High-Frequency Trading (HFT) simulator and machine learning data pipeline.

---

## 1. Project Architecture Overview

The system is a high-performance hybrid pipeline divided across the **Windows Host** (for Go, Python, and ML) and **WSL2** (for the ultra-low-latency C++ matching engine).

```mermaid
graph TD
    subgraph WSL2 (Linux Subsystem)
        A[C++ Exchange Simulator] -- "WebSocket (Port 8080)" --> B[Netsh Port Forwarding]
    end

    subgraph Windows Host
        B -- "Ingests Binary Ticks" --> C[Go Ingestion Backend]
        C -- "1. Validates Ticks" --> C
        C -- "2. Writes Parquet" --> D[(Go Data Storage: data/ticks/)]
        C -- "3. Broadcasts Ticks (Port 8081)" --> E[Python Data Recorder]
        E -- "Validates & Flushes" --> F[(Python Data Storage: ml/data/raw/)]
        
        G[C++ Strategy Engine] -- "gRPC (Port 50051)" --> H[Python ML Inference Server]
    end
```

---

## 2. What Happens When You Run `make`?

The `Makefile` at the root of the project orchestrates compilation, startup, shutdown, and cleaning of all components natively:

###  Build Commands
* **`make build-cpp`**: Compiles the ultra-low-latency C++ Matching Engine/Exchange Simulator inside WSL2.
* **`make build-go`**: Compiles the Go Ingestion Backend (`server.exe`) on the Windows host.
* **`make` (or `make all`)**: Runs both C++ and Go builds sequentially.

### 🏃 Execution Commands
* **`make run`**: Invokes `scripts/run_local.sh`, which performs a clean cleanup and launches 4 concurrent systems:
  1. **Python ML Inference Server**: Starts the gRPC server (`port 50051`) serving predictive models.
  2. **Go Ingestion Backend**: Starts the ingestion server (`port 8081` / `port 9090`) which connects to the simulator.
  3. **Python Data Recorder**: Subscribes to Go's WebSocket feed to log incoming raw ticks.
  4. **C++ Exchange Simulator**: Starts generating synthetic ticks and matching trades.
* **`make stop`**: Invokes `scripts/stop_local.sh` to safely stop all background tasks on both WSL2 and Windows.

---

## 3. What Does "Producing Data" Mean?

"Producing data" is the process of generating, validating, and persistently storing high-frequency market updates for model training and backtesting.

###  The Data Lifecycle
1. **Generation (C++ Engine)**: The C++ Exchange Simulator simulates **market makers** updating quotes and **noise traders** submitting market orders. This triggers trade matching events and generates Level-2 order book snapshots.
2. **Ingestion (Go Backend)**: The Go Backend ingests these binary payloads over a local WebSocket pipe. It decodes the raw bytes into structured ticks.
3. **Validation**: The Go backend and Python recorder evaluate every tick through strict sanity checks:
   - Positive bids/asks.
   - Bids do not cross asks (no crossed book).
   - Valid cumulative volume and sequence numbers.
   - Tracking sequence gaps to prevent forward look-ahead contamination.
4. **Serialization (Parquet)**: Valid ticks are buffered in memory and flushed periodically to high-performance **Parquet files** (in `data/ticks/` and `ml/data/raw/`).

### 📊 The Recorded Fields
Every tick written to the Parquet file contains:
| Column | Type | Description |
| :--- | :--- | :--- |
| `timestamp_ns` | `INT64` | Nanosecond unix timestamp from the exchange clock. |
| `symbol` | `STRING` | Asset ticker symbol (e.g., `AAPL`). |
| `bid` | `DOUBLE` | Best bid price (highest buy offer). |
| `ask` | `DOUBLE` | Best ask price (lowest sell offer). |
| `bid_sz` | `DOUBLE` | Total shares available at the best bid price. |
| `ask_sz` | `DOUBLE` | Total shares available at the best ask price. |
| `last_price` | `DOUBLE` | Last traded price on the exchange. |
| `volume` | `DOUBLE` | Cumulative volume matched during the session. |
| `sequence` | `INT64` | Monotonically increasing sequence number from the exchange. |
| `seq_gap` | `BOOLEAN` | Marker indicating if one or more ticks were dropped prior to this snapshot. |

---

## 4. Why Do We Need This Data? (ML Perspective)

The final target of this data pipeline is **Machine Learning (Phase 2 & 3)**.

* **Order Book Imbalance (OBI)**: By storing the sizes (`bid_sz`, `ask_sz`) and prices (`bid`, `ask`), we compute OBI and **Microprice** (size-weighted mid-price), which are strong leading indicators of short-term price movements.
* **Label Masking (`seq_gap`)**: If there is a network gap, sequence numbers jump. We flag `seq_gap = True` so the ML training script masks (excludes) these windows, preventing look-ahead contamination.
* **Model Training**: The Parquet files generated here are used to train our XGBoost model, enabling the strategy bot to run real-time inference (via gRPC port `50051`) to decide when to buy, sell, or adjust trading spreads.
