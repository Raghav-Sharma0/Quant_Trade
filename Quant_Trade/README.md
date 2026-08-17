<div align="center">

# ⚡ QuantTrade HFT Platform

**A production-grade, low-latency High-Frequency Trading simulation platform built with C++, Go, Python, and Next.js.**

[![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus)](https://isocpp.org/)
[![Go](https://img.shields.io/badge/Go-1.23-00ADD8?logo=go)](https://golang.org/)
[![Python](https://img.shields.io/badge/Python-3.10-yellow?logo=python)](https://python.org/)
[![Next.js](https://img.shields.io/badge/Next.js-16-black?logo=next.js)](https://nextjs.org/)
[![Docker](https://img.shields.io/badge/Docker-Compose-2496ED?logo=docker)](https://docker.com/)

</div>

---

## What Is This?

QuantTrade is a full-stack High-Frequency Trading (HFT) platform that simulates a real financial exchange end-to-end — from order matching and market data broadcasting, through real-time data ingestion and machine learning, to a live trading dashboard.

It is built as an engineering showcase of **low-latency systems design**, **polyglot architecture**, and **production-quality ML pipelines** applied to quantitative finance.

---

## 🎯 Key Features

- **Exchange Simulator** — Full limit order book with price-time priority matching, synthetic market makers, and noise traders generating realistic market dynamics
- **Sub-microsecond Risk Engine** — Pre-trade risk checks in ~100 nanoseconds including kill switches, rate limiting, position limits, and loss caps
- **High-Throughput Data Ingestion** — Lock-free ring buffer pipeline ingesting 100,000+ ticks/second with circuit breakers and automatic reconnection
- **XGBoost ML Model** — Walk-forward trained classifier that predicts short-term price direction from 16 microstructural features
- **Hot-Reload Inference** — ML model updates in production without server restart via background mtime polling
- **Real-Time Dashboard** — Live order book, price charts, ML predictions, and risk metrics with animated visualizations

---

## 🏗️ System Architecture

```
┌─────────────────────────────────┐
│   C++ Exchange Simulator        │
│   (Matching Engine + LOB)       │
│         WebSocket :8080         │
└──────────────┬──────────────────┘
               │  Binary Market Data
               ▼
┌─────────────────────────────────┐
│   Go Ingestion Backend          │
│   Lock-Free Pipeline            │
│   WebSocket :8081 | gRPC :9090  │
└────────┬──────────┬─────────────┘
         │          │
         ▼          ▼
┌──────────────┐ ┌──────────────────────┐
│ Python       │ │ Next.js Dashboard    │
│ ML Pipeline  │ │ Live Trading UI      │
│ gRPC :50051  │ │ :3000                │
└──────────────┘ └──────────────────────┘
```

**The platform operates as a continuous real-time loop:**
1. The **C++ Exchange Simulator** runs a limit order book with synthetic participants, broadcasting every price update over WebSocket
2. The **Go Backend** ingests binary market data through a lock-free pipeline, validates ticks, stores them as Parquet files, and re-broadcasts over JSON WebSocket
3. The **Python ML Pipeline** collects data, engineers 16 microstructural features, trains an XGBoost model using walk-forward validation, and serves predictions via gRPC
4. The **Frontend Dashboard** visualizes all of this live — order book depth, price action, ML signals, and risk state

---

## 🛠️ Technology Stack

| Layer | Technology | Purpose |
|---|---|---|
| Exchange Simulation | C++20, CMake, WebSocket | Matching engine, synthetic market participants |
| Data Ingestion | Go 1.23, gorilla/websocket, parquet-go | Lock-free binary ingestion, Parquet persistence |
| Inter-Process | gRPC / Protocol Buffers | Typed, fast service-to-service communication |
| ML Training | Python, XGBoost, scikit-learn, pandas | Feature engineering, walk-forward model training |
| ML Inference | Python gRPC server, joblib | Hot-reloadable prediction serving |
| Frontend | Next.js 16, TypeScript, Tailwind, Recharts | Real-time animated trading dashboard |
| Deployment | Docker Compose, Kubernetes (optional) | Container orchestration |

---

## 📊 Machine Learning

The platform trains an **XGBoost binary classifier** to predict short-term price direction (up/down over a configurable future window).

**16 microstructural features** are computed per tick:
- Bid-ask spread and microprice
- Order Book Imbalance (OBI)
- Log returns at 5, 10, 50-tick horizons
- Rolling volatility
- Exponentially Weighted Moving Averages (EWMA) of spread, imbalance, and returns at α = 0.01, 0.05, 0.10

**Walk-forward validation** via `TimeSeriesSplit` ensures the model is never trained on data from the future, avoiding look-ahead bias — a common mistake in financial ML.

The inference server **hot-reloads** new models automatically when the artifact directory is updated by a retraining job, with no downtime.

---

## 🚀 Quick Start

### Option 1: Docker (Recommended)

```bash
# Clone the repository
git clone https://github.com/your-org/quant-trade
cd quant-trade/Quant_Trade

# Start all services
docker compose up --build -d

# View logs
docker compose logs -f
```

Open **http://localhost:3000** in your browser.

> **Note**: The first build compiles the C++ exchange simulator, which takes 5–10 minutes and requires ~8 GB of free disk space.

### Option 2: Local (WSL2 + Windows)

```bash
# Build everything
make all

# Start all services
make run

# Stop all services
make stop
```

### Option 3: Frontend Only

```bash
cd frontend
npm install
npm run dev
# → http://localhost:3000
```

---

## 📁 Project Layout

```
Quant_Trade/
├── core-cpp/        # C++ header-only library (risk engine, order book, SPSC queue)
├── exchange-sim/    # C++ Exchange Simulator binary
├── backend-go/      # Go ingestion backend + WS/gRPC servers
├── ml/              # Python ML pipeline (data, features, training, inference)
├── frontend/        # Next.js real-time dashboard
├── proto/           # Protocol Buffer schemas
├── configs/         # YAML configuration (dev + prod)
├── artifacts/       # Trained ML model artifacts
├── scripts/         # Build and deployment scripts
└── docker-compose.yml
```

---

## 📡 Service Endpoints

| Service | Endpoint | Description |
|---|---|---|
| Frontend | http://localhost:3000 | Live trading dashboard |
| WS Market Data | ws://localhost:8081/ws/market-data | Real-time tick stream |
| WS Trades | ws://localhost:8081/ws/trades | Trade execution stream |
| WS ML Signals | ws://localhost:8081/ws/ml-predictions | ML prediction stream |
| gRPC Backend | localhost:9090 | Market data gRPC service |
| gRPC ML | localhost:50051 | Prediction gRPC service |
| Exchange Simulator | ws://localhost:8080 | Raw binary tick feed |

---

## 🔬 Engineering Highlights

**Lock-Free Ingestion Pipeline**: The Go backend uses a cache-line-padded SPSC ring buffer (131,072 slots) between the network I/O goroutine and the dispatcher. This avoids mutex contention on the hot path, keeping tick processing latency in the single-digit microsecond range.

**Circuit Breaker**: The WebSocket client wraps reconnect logic in a 3-state circuit breaker (Closed → Open → Half-Open). When the exchange simulator goes down, the breaker trips after 5 failures and blocks reconnect attempts for 10 seconds, preventing CPU-burning reconnect storms.

**Pre-Trade Risk in ~100ns**: The C++ risk engine evaluates 6 checks in cheapest-first order (kill switch → order size → rate limit → notional cap → position limit → loss cap). The sliding-window rate limiter uses a circular timestamp buffer for O(1) amortized complexity.

**Sequence Gap Masking**: Every tick carries a monotonically increasing sequence number. Gaps are detected and flagged (`seq_gap = true`) in both the Go backend and Python recorder. The ML training pipeline masks gap ticks to prevent training on artificially corrupted price jumps.

---

## 📚 Documentation

- **[DEVELOPER_README.md](DEVELOPER_README.md)** — Detailed technical reference for contributors: component internals, wire protocols, configuration, build instructions, and troubleshooting

---

## 📄 License

This project is for educational and portfolio purposes.
