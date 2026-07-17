# Detect Operating System and WSL environment
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
IS_WSL  := $(shell uname -a 2>/dev/null | grep -i microsoft)

.PHONY: all build-cpp build-go run stop tail clean bench bench-risk test-risk

all: build-cpp build-go

build-cpp:
ifeq ($(UNAME_S),Linux)
	@echo "=== Building C++ Exchange Simulator Natively on Linux/WSL ==="
	@cd exchange-sim && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
else
	@echo "=== Building C++ Exchange Simulator in WSL (from Windows Host) ==="
	@wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/exchange-sim && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
endif

build-go:
	@echo "=== Building Go Ingestion Backend ==="
ifneq ($(IS_WSL),)
	# Inside WSL: Cross-compile server.exe for the Windows Host
	@echo "Detected WSL: Compiling Windows executable..."
	@cd backend-go && (command -v go >/dev/null 2>&1 && GOOS=windows go build -o server.exe ./cmd/server/ || go.exe build -o server.exe ./cmd/server/)
else ifeq ($(UNAME_S),Linux)
	# Inside Native Linux: Compile native Linux server binary
	@echo "Detected Native Linux: Compiling Linux executable..."
	@cd backend-go && go build -o server ./cmd/server/
else
	# On Windows Host: Compile Windows server.exe binary
	@echo "Detected Windows Host: Compiling Windows executable..."
	@cd backend-go && go build -o server.exe ./cmd/server/
endif

run: all
	@bash ./scripts/run_local.sh

stop:
	@bash ./scripts/stop_local.sh

tail:
	@tail -f logs/*.log

# ── C++ Risk Engine Benchmarks ────────────────────────────────────────────────
# make bench       — build + run all core-cpp benchmarks (latency, order book,
#                    matching engine, risk engine) and save JSON to results/
# make bench-risk  — build + run risk engine benchmark only (faster, ~15 sec)
# make test-risk   — compile and run the C++ risk engine unit tests
# ──────────────────────────────────────────────────────────────────────────────

bench:
	@echo "=== Running All C++ Core Benchmarks ==="
ifeq ($(UNAME_S),Linux)
	@cd core-cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev && cmake --build build
	@cd core-cpp && python3 scripts/run_benchmarks.py
else
	@wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/core-cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev && cmake --build build && python3 scripts/run_benchmarks.py"
endif

bench-risk:
	@echo "=== Running Risk Engine Latency Benchmark ==="
ifeq ($(UNAME_S),Linux)
	@cd core-cpp && python3 scripts/run_risk_bench.py
else
	@wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/core-cpp && python3 scripts/run_risk_bench.py"
endif

test-risk:
	@echo "=== Building & Running Risk Engine Tests ==="
ifeq ($(UNAME_S),Linux)
	@cd core-cpp && cmake -S . -B build -Wno-dev && cmake --build build --target test_risk && ./build/test_risk
else
	@wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/core-cpp && cmake -S . -B build -Wno-dev && cmake --build build --target test_risk && ./build/test_risk"
endif

clean:
	@echo "=== Cleaning build files ==="
ifeq ($(UNAME_S),Linux)
	@rm -rf exchange-sim/build core-cpp/build
	@rm -f backend-go/server backend-go/server.exe
else
	@wsl rm -rf exchange-sim/build core-cpp/build || rm -rf exchange-sim/build
	@rm -f backend-go/server.exe
endif
	@rm -rf logs
