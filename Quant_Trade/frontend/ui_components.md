# HFT Platform Frontend UI Component Specification

This document defines the layout, features, and real-time visualization components for the HFT Polyglot Pipeline & ML Platform UI.

---

## 1. Landing Page Specifications

The landing page must establish credibility by highlighting the polyglot stack, pipeline architecture, and sub-millisecond benchmarks.

### 1.1 Hero Section
* **Title**: High-Frequency Polyglot Trading & ML Orchestration Platform
* **Subtitle**: Sub-millisecond market data ingestion, lock-free pre-trade risk checks, and real-time walk-forward XGBoost inference.
* **Call to Actions (CTAs)**:
  * "Launch Live Dashboard" (Redirects to Dashboard)
  * "View Architecture Spec" (Redirects to system design docs)
* **Hero Visual**: An interactive CSS/SVG-animated mock pipeline illustrating binary tick data flowing from C++ to Go to Python in real-time.

### 1.2 Performance & Benchmarking Panel
An interactive chart or grid highlighting real-time platform metrics:
* **Pre-Trade Risk Check Latency**: `< 100 ns` (Nanoseconds)
* **SPSC Lock-Free Queue Transfer**: `< 50 ns`
* **Go Ingestion Max Throughput**: `100,000+ ticks/sec`
* **gRPC Inference Latency**: `< 2.1 ms` (XGBoost prediction loop)
* **Sequence Gap Recovery**: `100%` (via automated gap masking)

### 1.3 Polyglot Tech Stack Grid
Displays cards explaining the role of each language in the system:
* **C++ (Matching Engine & Core)**: Zero-overhead memory alignment, LOB priority queue, pre-trade checks.
* **Go (Ingestion Hub)**: High-concurrency network routing, binary deserialization, gzip Parquet buffer management.
* **Python (ML & Inference)**: Walk-forward XGBoost model training, streaming microstructural features (OBI, EWMA), and high-throughput gRPC prediction serving.

---

## 2. Interactive HFT Dashboard Specifications

The dashboard acts as the live control center, subscribing to the Go Ingestion server via WebSockets (`ws://localhost:8081/ws/market-data`) to display market states.

### 2.1 Live Limit Order Book (LOB) Component
* **Visuals**: Vertical split-table or stacked bars showing Bid/Ask levels.
  * **Asks (Red)**: Top-down list of ask levels (Price, Size, Cumulative Volume).
  * **Spread Indicator**: Highlights the current Bid-Ask spread and Midprice.
  * **Bids (Green)**: Bottom-up list of bid levels (Price, Size, Cumulative Volume).
* **Depth Visualization**: A side horizontal bar chart representing relative depth at each price level to instantly show liquidity pockets.

### 2.2 Microstructural Features Panel
Visualizes streaming features calculated on incoming ticks:
* **Order Book Imbalance (OBI) Gauge**: 
  * Dial or slider ranging from `-1.0` (severe ask pressure) to `+1.0` (severe bid pressure).
  * Formula: $\text{OBI} = \frac{\text{Bid Size} - \text{Ask Size}}{\text{Bid Size} + \text{Ask Size}}$
* **Price Divergence Line Chart**:
  * Real-time line chart comparing the standard **Midprice** and the **Microprice** (size-weighted).
  * Highlights divergence which signals short-term order book pressure.

### 2.3 ML Inference & Strategy Panel
Displays predictions received from the C++ Strategy Bot (derived from python gRPC inference):
* **XGBoost Buy Probability**: Radial progress bar showing buy confidence (0% to 100%).
* **Signal Indicator**: Glow badge displaying current trading signal:
  * `BUY` (Green glow, prob > 65%)
  * `SELL` (Red glow, prob < 35%)
  * `HOLD` (Gray/Blue, neutral)
* **Walk-Forward Status**: Shows if the model is currently active, version hash, and sequence gap health status (`seq_gap = True` trigger warning badge).

### 2.4 Pre-Trade Risk & Position Tracker
Real-time monitoring of safety constraints managed by the C++ Strategy Risk Engine:
* **Risk Engine Status**: `ACTIVE` (Green) / `BLOCKED` (Red)
* **Pre-Trade Risk Check Latency**: Live sparkline chart showing latency in nanoseconds.
* **Position Dashboard**:
  * Net Position Size (Long/Short contracts).
  * Real-time Realized & Unrealized PnL.
  * Maximum Exposure limit bar (visualizes distance to risk cap).

### 2.5 Execution & Trade Log
* **Trades Stream Table**: Monotonically updating list of trade matches:
  * Columns: `Timestamp`, `Symbol`, `Price`, `Size`, `Execution Mode` (IOC/FOK/Limit), `Sequence ID`, `Network Health` (displays `OK` or `SEQ GAP` warning badge).
