.PHONY: all build-cpp build-go run stop tail clean

all: build-cpp build-go

build-cpp:
	@echo "=== Building C++ Exchange Simulator ==="
	@cd exchange-sim && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

build-go:
	@echo "=== Building Go Ingestion Backend ==="
	@cd backend-go && go build -o server cmd/server/main.go

run: all
	@mkdir -p logs
	@echo "=== Starting Python ML Inference Server ==="
	@if [ -d venv ]; then \
		. venv/bin/activate && PYTHONPATH=.:ml/inference python3 ml/inference/server.py > logs/inference.log 2>&1 & echo $$! > logs/inference.pid; \
	else \
		PYTHONPATH=.:ml/inference python3 ml/inference/server.py > logs/inference.log 2>&1 & echo $$! > logs/inference.pid; \
	fi
	@sleep 2
	@echo "=== Starting C++ Exchange Simulator ==="
	@./exchange-sim/build/exchange_sim --synth --duration 100000 --ws-port 8080 --symbol 0 --noise-interval-us 10000 > logs/simulator.log 2>&1 & echo $$! > logs/simulator.pid
	@sleep 1
	@echo "=== Starting Go Ingestion Backend ==="
	@cd backend-go && ./server --config ../configs/dev.yaml > ../logs/backend.log 2>&1 & echo $$! > ../logs/backend.pid
	@sleep 1
	@echo "=== Starting Python ML Data Recorder ==="
	@if [ -d venv ]; then \
		. venv/bin/activate && PYTHONPATH=. python3 -m ml.data_gathering.recorder > logs/recorder.log 2>&1 & echo $$! > logs/recorder.pid; \
	else \
		PYTHONPATH=. python3 -m ml.data_gathering.recorder > logs/recorder.log 2>&1 & echo $$! > logs/recorder.pid; \
	fi
	@echo ""
	@echo "All systems running! Logs are written to logs/ directory."
	@echo "To view live logs, run: make tail"
	@echo "To stop everything, run: make stop"

stop:
	@echo "=== Stopping all processes ==="
	@if [ -f logs/recorder.pid ]; then kill $$(cat logs/recorder.pid) 2>/dev/null || true; rm -f logs/recorder.pid; fi
	@if [ -f logs/backend.pid ]; then kill $$(cat logs/backend.pid) 2>/dev/null || true; rm -f logs/backend.pid; fi
	@if [ -f logs/simulator.pid ]; then kill $$(cat logs/simulator.pid) 2>/dev/null || true; rm -f logs/simulator.pid; fi
	@if [ -f logs/inference.pid ]; then kill $$(cat logs/inference.pid) 2>/dev/null || true; rm -f logs/inference.pid; fi
	@echo "All processes stopped."

tail:
	@tail -f logs/*.log

clean:
	@echo "=== Cleaning build files ==="
	@rm -rf exchange-sim/build
	@rm -f backend-go/server
	@rm -rf logs
