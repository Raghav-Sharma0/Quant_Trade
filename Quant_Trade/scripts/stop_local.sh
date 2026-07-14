#!/bin/bash

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

echo "=== Stopping all processes ==="

# Read PIDs if files exist
if [ -f logs/recorder.pid ]; then
    PID=$(cat logs/recorder.pid)
    kill $PID 2>/dev/null || powershell.exe -Command "Stop-Process -Id $PID -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
    rm -f logs/recorder.pid
fi

if [ -f logs/backend.pid ]; then
    PID=$(cat logs/backend.pid)
    kill $PID 2>/dev/null || powershell.exe -Command "Stop-Process -Id $PID -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
    rm -f logs/backend.pid
fi

if [ -f logs/inference.pid ]; then
    PID=$(cat logs/inference.pid)
    kill $PID 2>/dev/null || powershell.exe -Command "Stop-Process -Id $PID -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
    rm -f logs/inference.pid
fi

if [ -f logs/simulator.pid ]; then
    PID=$(cat logs/simulator.pid)
    kill $PID 2>/dev/null || powershell.exe -Command "Stop-Process -Id $PID -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
    rm -f logs/simulator.pid
fi

# Clean up any remaining processes by name just in case
powershell.exe -Command "Stop-Process -Name server -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
powershell.exe -Command "Stop-Process -Name py -Force -ErrorAction SilentlyContinue" 2>/dev/null || true
powershell.exe -Command "Stop-Process -Name python -Force -ErrorAction SilentlyContinue" 2>/dev/null || true

if [ "$IS_LINUX" = true ]; then
    killall exchange_sim 2>/dev/null || true
    killall server 2>/dev/null || true
else
    wsl killall exchange_sim 2>/dev/null || true
fi

echo "All processes stopped."
