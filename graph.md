flowchart LR

EX["Market Data Exchange"]

subgraph GO["Go Backend Server"]
direction TB

WS["WebSocket Server :8080"]
ING["8x Concurrent Ingestion Workers"]
VAL["Tick Validator"]
HUB["Broadcast Hub"]
PW["Parquet Writer"]

WSG["WebSocket Gateway :8081"]
GRPC["gRPC Market Data Service :9090"]

WS --> ING
ING --> VAL
VAL --> HUB
ING --> PW
HUB --> WSG
HUB --> GRPC

end

subgraph PY["Python ML Pipeline"]
direction TB

REC["Data Recorder"]
DATA["Training Dataset (Parquet)"]

INF["Inference Server :50051"]
MODEL["XGBoost Model"]

REC --> DATA
MODEL --> INF

end

subgraph CPP["C++ Strategy Engine"]
direction TB

STRAT["Simple Spread Strategy"]
EXEC["Execution Engine"]

STRAT --> INF
STRAT --> EXEC

end

EX --> WS
WSG --> REC
GRPC --> STRAT
PW -.-> DATA
