graph TD
    subgraph C++ Exchange Simulator
        ME[Matching Engine] -->|Quotes| MM[Market Maker Agent]
        ME -->|Orders| NT[Noise Trader Agent]
        ME -->|Executions/Quotes| WSP[WebSocket Publisher :8080]
    end

    subgraph Go Backend Server
        WSC[WebSocket Client] <-- Connects to :8080 --- WSP
        WSC -->|Raw JSON Queue| Workers[8x Concurrent Ingestion Workers]
        Workers -->|Parse & Validate| VAL[Tick Validator]
        Workers -->|Async Queue| PW[Parquet Storage Writer]
        Workers -->|Valid Ticks/Trades| HUB[Broadcast Hub]
        HUB -->|WS Gateway :8081| WSG[WebSocket Gateway]
        HUB -->|gRPC Server :9090| GRPC[gRPC Market Data Service]
    end

    subgraph Python ML Pipeline
        REC[Data Recorder] <-- Subscribe to :8081 --- WSG
        REC -->|Store Training Data| PAR[Parquet Files]
        
        INF[Inference Server :50051] -->|Load XGBoost Model| ML[model.pkl]
    end

    subgraph C++ Strategy Engine
        STRAT[SimpleSpreadStrategy] -->|gRPC Query| INF
        STRAT -->|Execution Orders| ME
    end
