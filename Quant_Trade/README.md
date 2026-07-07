# Quant Trade Project

A high-frequency trading (HFT) simulation platform built with a polyglot microservice architecture, featuring a C++ trading core/exchange simulator, a Go-based market data backend, and a Python-based Machine Learning pipeline for price prediction.

## Project Structure and Status

The project consists of three primary components that communicate via gRPC and WebSockets.

### 1. Market Data Backend (`backend-go/`)
**Status:** ✅ Completed
A Go-based server that acts as the central hub for market data and client communication.
*   **Ingestion:** Connects to the exchange simulator and ingests raw ticks and trades.
*   **Streaming:** Uses WebSockets and gRPC (`StreamTicks`) to broadcast live market data to downstream consumers (like the frontend or ML services).
*   **Storage:** Stores ingested ticks persistently in compressed Parquet format (`parquet-go`).
*   **API:** Provides a gRPC interface (`GetHistoricalTicks`, `HealthCheck`) for querying historical data and server status.

### 2. Machine Learning Pipeline (`ml/`)
**Status:** ✅ Completed
A Python-based pipeline for data gathering, feature engineering, training, and real-time inference.
*   **Data Gathering (`ml/data_gathering/`):** Python scripts to connect to WebSocket feeds, validate ticks, handle sequence gaps, and write data to Parquet buffers.
*   **Feature Engineering (`ml/feature_engineering/`):** Computes microstructural features (spread, microprice, imbalance, log returns) and streaming EWMA stats without look-ahead bias.
*   **Training (`ml/training/train.py`):** Trains an XGBoost classifier using walk-forward cross-validation (TimeSeriesSplit) to predict short-term price movements. Saves models, pipelines, and metadata as artifacts.
*   **Inference Server (`ml/inference/server.py`):** A high-performance gRPC server that loads the trained model, maintains streaming feature state for incoming ticks, and returns predictive signals (`buy_prob` and `direction`).

### 3. Trading Core & Exchange Simulator (`core-cpp/` & `exchange-sim/`)
**Status:** Managed via C++
*   **Exchange Simulator:** Simulates order books, matching engines, and broadcasts market data updates.
*   **Core:** C++ strategies that use the gRPC predictions (cached asynchronously to avoid blocking the hot path) to make trading decisions.

### 4. Protobuf Definitions (`proto/`)
Defines the cross-language contracts (gRPC/Protobuf) for the system:
*   `prediction.proto`: Requesting and receiving ML predictions.
*   `market_data.proto`: Streaming and fetching ticks.
*   `order.proto`, `execution.proto`, `risk.proto`, `strategy.proto`: Core trading types.

---

## How to Run the Project

### Prerequisites
*   **Go** (1.22+) for the backend.
*   **Python** (3.11+) for the ML pipeline.
*   **gRPC tools** for Python (`grpcio`, `grpcio-tools`, `xgboost`, `pandas`, `pyarrow`, `scikit-learn`).

### 1. Start the Go Backend
The Go backend ingests market data and serves it via gRPC and HTTP/WebSockets.
```bash
cd /mnt/0A86F7A686F79085/Users/ragha/Code/Quant_Trade/Quant_Trade/backend-go
go run cmd/server/main.go --config configs/dev.yaml
```

### 2. Train the ML Model
Before running the ML inference server, you must train the model to generate the necessary artifacts (`model.pkl`, `pipe.pkl`). If no real Parquet data is found, it will auto-generate dummy data for testing.
```bash
cd /mnt/0A86F7A686F79085/Users/ragha/Code/Quant_Trade/Quant_Trade
PYTHONPATH=. python ml/training/train.py
```
*(Artifacts will be saved to `Quant_Trade/artifacts/`)*

### 3. Start the ML Inference Server
Starts the Python gRPC server (port 50051) that listens for feature requests from the C++ core and returns trading predictions.
```bash
cd /mnt/0A86F7A686F79085/Users/ragha/Code/Quant_Trade/Quant_Trade
PYTHONPATH=. python ml/inference/server.py
```

### Note on Protobufs
If you change any `.proto` files in the `proto/` directory, you must recompile them for Python (the Go protos are already generated in `backend-go/generated/proto/`):
```bash
cd /mnt/0A86F7A686F79085/Users/ragha/Code/Quant_Trade/Quant_Trade
python -m grpc_tools.protoc -Iproto --python_out=ml/inference --grpc_python_out=ml/inference proto/prediction.proto
```
