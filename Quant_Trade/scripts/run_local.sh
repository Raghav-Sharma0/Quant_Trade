#!/bin/bash

# Ensure logs directory exists
mkdir -p logs

# Detect if running on Linux (either Native Linux or WSL)
if [ "$(uname -s)" = "Linux" ]; then
    IS_LINUX=true
else
    IS_LINUX=false
fi
# Detect if running inside WSL
if grep -qE "(Microsoft|microsoft)" /proc/version 2>/dev/null; then
    IS_WSL=true
else
    IS_WSL=false
fi
echo "Running on Linux: $IS_LINUX (WSL: $IS_WSL)"

echo "=== Cleaning up any existing processes ==="
# Kill Windows processes (only works on Windows or WSL)
powershell.exe -Command "Stop-Process -Name server -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
powershell.exe -Command "Stop-Process -Name py -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
powershell.exe -Command "Stop-Process -Name python -Force -ErrorAction SilentlyContinue" 2>/dev/null || true

# Kill Linux processes
if [ "$IS_LINUX" = true ]; then
    killall exchange_sim 2>/dev/null || true
    killall server 2>/dev/null || true
else
    wsl killall exchange_sim 2>/dev/null || true
fi
sleep 1

# Detect Python Command
if command -v py >/dev/null 2>&1; then
    PYTHON_CMD="py"
elif command -v py.exe >/dev/null 2>&1; then
    PYTHON_CMD="py.exe"
elif command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
elif command -v python.exe >/dev/null 2>&1; then
    PYTHON_CMD="python.exe"
elif command -v python3 >/dev/null 2>&1; then
    PYTHON_CMD="python3"
else
    echo "Error: Python executable not found"
    exit 1
fi
echo "Using Python command: $PYTHON_CMD"

# Detect Go Server Command
if [ -f "./backend-go/server" ]; then
    SERVER_CMD="./backend-go/server"
elif [ -f "./backend-go/server.exe" ]; then
    SERVER_CMD="./backend-go/server.exe"
else
    SERVER_CMD="server"
fi
echo "Using Go Server command: $SERVER_CMD"

echo "=== Starting Python ML Inference Server ==="
PYTHONPATH=. $PYTHON_CMD -u -m ml.inference.server > logs/inference.log 2>&1 &
INF_PID=$!
echo $INF_PID > logs/inference.pid
echo "Inference server started with PID $INF_PID (logs: logs/inference.log)"

sleep 2

echo "=== Starting Go Ingestion Backend ==="
$SERVER_CMD --config configs/dev.yaml > logs/backend.log 2>&1 &
BACKEND_PID=$!
echo $BACKEND_PID > logs/backend.pid
echo "Go backend started with PID $BACKEND_PID (logs: logs/backend.log)"

sleep 2

echo "=== Starting Python ML Data Recorder ==="
PYTHONPATH=. $PYTHON_CMD -u -m ml.data_gathering.recorder > logs/recorder.log 2>&1 &
RECORDER_PID=$!
echo $RECORDER_PID > logs/recorder.pid
echo "Data recorder started with PID $RECORDER_PID (logs: logs/recorder.log)"

sleep 2

echo "=== Starting C++ Exchange Simulator ==="
if [ "$IS_LINUX" = true ]; then
    # Run natively when on Linux (Native or WSL)
    cd exchange-sim && ./build/exchange_sim --synth --duration 3600 --ws-port 8080 --symbol 0 --noise-interval-us 10000 > ../logs/simulator.log 2>&1 &
    SIM_PID=$!
    cd ..
    echo "C++ Simulator started natively on Linux with PID $SIM_PID (logs: logs/simulator.log)"
else
    # Run via wsl command when on Windows Host
    wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/exchange-sim && ./build/exchange_sim --synth --duration 3600 --ws-port 8080 --symbol 0 --noise-interval-us 10000" > logs/simulator.log 2>&1 &
    SIM_PID=$!
    echo "C++ Simulator started in WSL (via Windows) with PID $SIM_PID (logs: logs/simulator.log)"
fi

echo ""
echo "All components started successfully!"
echo "To view logs, run: tail -f logs/*.log"
echo "To stop everything, run: make stop  or  ./scripts/stop_local.sh"
