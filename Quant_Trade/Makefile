# Detect if we are running inside WSL (Linux with Microsoft kernel)
IS_WSL := $(shell uname -a 2>/dev/null | grep -i microsoft)

.PHONY: all build-cpp build-go run stop tail clean

all: build-cpp build-go

build-cpp:
ifdef IS_WSL
	@echo "=== Building C++ Exchange Simulator Natively in WSL ==="
	@cd exchange-sim && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
else
	@echo "=== Building C++ Exchange Simulator in WSL (from Windows Host) ==="
	@wsl sh -c "cd /mnt/d/HFT/Quant_Trade/Quant_Trade/exchange-sim && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
endif

build-go:
	@echo "=== Building Go Ingestion Backend ==="
ifdef IS_WSL
	@cd backend-go && (command -v go >/dev/null 2>&1 && GOOS=windows go build -o server.exe ./cmd/server/ || go.exe build -o server.exe ./cmd/server/)
else
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
	@wsl rm -rf exchange-sim/build || rm -rf exchange-sim/build
	@rm -f backend-go/server.exe
	@rm -rf logs

