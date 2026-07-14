# Detect Operating System and WSL environment
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
IS_WSL  := $(shell uname -a 2>/dev/null | grep -i microsoft)

.PHONY: all build-cpp build-go run stop tail clean

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

clean:
	@echo "=== Cleaning build files ==="
ifeq ($(UNAME_S),Linux)
	@rm -rf exchange-sim/build
	@rm -f backend-go/server backend-go/server.exe
else
	@wsl rm -rf exchange-sim/build || rm -rf exchange-sim/build
	@rm -f backend-go/server.exe
endif
	@rm -rf logs


