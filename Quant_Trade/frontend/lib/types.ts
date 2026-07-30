export interface OrderLevel {
  price: number
  size: number
  volume: number
  total: number
}

export interface OrderBook {
  asks: OrderLevel[]
  bids: OrderLevel[]
  spread: number
  midprice: number
  microprice: number
}

export interface TickEntry {
  id: number
  timestamp: string
  price: number
  volume: number
  side: 'BUY' | 'SELL'
  sequence: number
}

export interface PricePoint {
  time: string
  midprice: number
  microprice: number
}

export interface ExecutionEntry {
  id: number
  timestamp: string
  symbol: string
  price: number
  size: number
  mode: 'MARKET' | 'LIMIT' | 'IOC' | 'FOK'
  sequenceId: number
  health: 'OK' | 'WARN' | 'ERR'
}

export interface MLPrediction {
  buyProbability: number
  signal: 'BUY' | 'SELL' | 'HOLD'
  modelVersion: string
  inferenceLatency: number
}

export interface RiskState {
  status: 'ACTIVE' | 'BLOCKED'
  latency: number
  sparkline: number[]
  riskScore: number
  maxExposure: number
  currentExposure: number
}

export interface WalkForwardStatus {
  modelStatus: 'TRAINING' | 'LIVE' | 'STALE' | 'FAILED'
  versionHash: string
  gapRecovery: 'OK' | 'RECOVERING' | 'FAILED'
  latency: number
  health: 'HEALTHY' | 'DEGRADED' | 'CRITICAL'
}

export interface Position {
  netPosition: number
  avgEntry: number
  unrealizedPnl: number
  realizedPnl: number
  dailyPnl: number
}

export interface OBIValue {
  value: number
  history: number[]
}
