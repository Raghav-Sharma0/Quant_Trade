'use client'

import { useState, useEffect, useCallback, useRef } from 'react'
import type {
  OrderBook,
  TickEntry,
  PricePoint,
  ExecutionEntry,
  MLPrediction,
  RiskState,
  WalkForwardStatus,
  Position,
  OBIValue,
} from '@/lib/types'

const BASE_PRICE = 4312.75
const SYMBOLS = ['ES1!', 'NQ1!', 'RTY1!', 'YM1!', 'CL1!']
const EXEC_MODES = ['MARKET', 'LIMIT', 'IOC', 'FOK'] as const
const MODEL_HASHES = ['a3f9b2c1', 'd7e4f8a2', 'b1c6d9e3', 'f2a8b5c7']

function rand(min: number, max: number, decimals = 2): number {
  return parseFloat((Math.random() * (max - min) + min).toFixed(decimals))
}

function randInt(min: number, max: number): number {
  return Math.floor(Math.random() * (max - min + 1)) + min
}

function now(): string {
  return new Date().toISOString().slice(11, 23)
}

function generateOrderBook(basePrice: number): OrderBook {
  const spread = rand(0.25, 1.5)
  const midprice = basePrice
  const askBase = midprice + spread / 2
  const bidBase = midprice - spread / 2

  let askTotal = 0
  const asks = Array.from({ length: 10 }, (_, i) => {
    const price = parseFloat((askBase + i * rand(0.25, 0.75)).toFixed(2))
    const size = randInt(1, 50)
    const volume = parseFloat((size * price).toFixed(0))
    askTotal += size
    return { price, size, volume, total: askTotal }
  })

  let bidTotal = 0
  const bids = Array.from({ length: 10 }, (_, i) => {
    const price = parseFloat((bidBase - i * rand(0.25, 0.75)).toFixed(2))
    const size = randInt(1, 80)
    const volume = parseFloat((size * price).toFixed(0))
    bidTotal += size
    return { price, size, volume, total: bidTotal }
  })

  const totalBidSize = bids.reduce((a, b) => a + b.size, 0)
  const totalAskSize = asks.reduce((a, b) => a + b.size, 0)
  const microprice = parseFloat(
    ((bidBase * totalAskSize + askBase * totalBidSize) / (totalBidSize + totalAskSize)).toFixed(2)
  )

  return { asks, bids, spread, midprice, microprice }
}

function generateMLPrediction(): MLPrediction {
  const buy = rand(0.05, 0.95)
  const signal = buy > 0.6 ? 'BUY' : buy < 0.4 ? 'SELL' : 'HOLD'
  return {
    buyProbability: parseFloat((buy * 100).toFixed(1)),
    signal,
    modelVersion: `v${randInt(3, 5)}.${randInt(0, 9)}.${randInt(0, 99)}`,
    inferenceLatency: rand(0.8, 3.2),
  }
}

function generateRiskState(prev?: RiskState): RiskState {
  const sparkline = prev
    ? [...prev.sparkline.slice(-19), rand(10, 95)]
    : Array.from({ length: 20 }, () => rand(10, 95))
  return {
    status: Math.random() > 0.05 ? 'ACTIVE' : 'BLOCKED',
    latency: rand(42, 98),
    sparkline,
    riskScore: rand(0, 100),
    maxExposure: 500000,
    currentExposure: rand(50000, 480000),
  }
}

export function useMarketData() {
  const [price, setPrice] = useState(BASE_PRICE)
  const [orderBook, setOrderBook] = useState<OrderBook>(() => generateOrderBook(BASE_PRICE))
  const [ticks, setTicks] = useState<TickEntry[]>([])
  const [priceHistory, setPriceHistory] = useState<PricePoint[]>([])
  const [executions, setExecutions] = useState<ExecutionEntry[]>([])
  const [mlPrediction, setMlPrediction] = useState<MLPrediction>(generateMLPrediction())
  const [riskState, setRiskState] = useState<RiskState>(() => generateRiskState())
  const [obi, setObi] = useState<OBIValue>({ value: 0, history: [] })
  const [walkForward, setWalkForward] = useState<WalkForwardStatus>({
    modelStatus: 'LIVE',
    versionHash: MODEL_HASHES[0],
    gapRecovery: 'OK',
    latency: 1.8,
    health: 'HEALTHY',
  })
  const [position, setPosition] = useState<Position>({
    netPosition: 12,
    avgEntry: 4298.5,
    unrealizedPnl: 1753.0,
    realizedPnl: 5820.0,
    dailyPnl: 7573.0,
  })

  const tickIdRef = useRef(1000)
  const execIdRef = useRef(1)
  const priceRef = useRef(BASE_PRICE)

  const tick = useCallback(() => {
    // Price walk
    const delta = (Math.random() - 0.499) * rand(0.1, 2.5)
    const newPrice = parseFloat((priceRef.current + delta).toFixed(2))
    priceRef.current = newPrice
    setPrice(newPrice)

    // Order book
    setOrderBook(generateOrderBook(newPrice))

    // Tick stream
    const newTick: TickEntry = {
      id: tickIdRef.current++,
      timestamp: now(),
      price: newPrice,
      volume: randInt(1, 120),
      side: Math.random() > 0.5 ? 'BUY' : 'SELL',
      sequence: tickIdRef.current,
    }
    setTicks((prev) => [newTick, ...prev].slice(0, 50))

    // Price history
    const pt: PricePoint = {
      time: now(),
      midprice: newPrice,
      microprice: parseFloat((newPrice + rand(-0.5, 0.5)).toFixed(2)),
    }
    setPriceHistory((prev) => [...prev, pt].slice(-60))

    // OBI
    const bidVol = randInt(100, 500)
    const askVol = randInt(100, 500)
    const obiVal = parseFloat(((bidVol - askVol) / (bidVol + askVol)).toFixed(3))
    setObi((prev) => ({ value: obiVal, history: [...(prev.history || []), obiVal].slice(-30) }))

    // ML Prediction (slower update)
    if (Math.random() > 0.6) setMlPrediction(generateMLPrediction())

    // Risk state
    setRiskState((prev) => generateRiskState(prev))

    // Position
    setPosition((prev) => {
      const newUnrealized = parseFloat(
        (prev.netPosition * (newPrice - prev.avgEntry) * 50).toFixed(2)
      )
      const dailyDelta = parseFloat((rand(-500, 800)).toFixed(2))
      return {
        ...prev,
        unrealizedPnl: newUnrealized,
        realizedPnl: parseFloat((prev.realizedPnl + (Math.random() > 0.9 ? dailyDelta * 0.2 : 0)).toFixed(2)),
        dailyPnl: parseFloat((prev.unrealizedPnl + prev.realizedPnl).toFixed(2)),
      }
    })

    // Walk forward (very occasional)
    if (Math.random() > 0.97) {
      setWalkForward({
        modelStatus: 'LIVE',
        versionHash: MODEL_HASHES[randInt(0, 3)],
        gapRecovery: Math.random() > 0.1 ? 'OK' : 'RECOVERING',
        latency: rand(0.9, 2.8),
        health: Math.random() > 0.1 ? 'HEALTHY' : 'DEGRADED',
      })
    }

    // Executions (occasional)
    if (Math.random() > 0.65) {
      const exec: ExecutionEntry = {
        id: execIdRef.current++,
        timestamp: now(),
        symbol: SYMBOLS[randInt(0, 4)],
        price: newPrice,
        size: randInt(1, 20),
        mode: EXEC_MODES[randInt(0, 3)],
        sequenceId: randInt(100000, 999999),
        health: Math.random() > 0.1 ? 'OK' : Math.random() > 0.5 ? 'WARN' : 'ERR',
      }
      setExecutions((prev) => [exec, ...prev].slice(0, 40))
    }
  }, [])

  useEffect(() => {
    const interval = setInterval(tick, 1000)
    // Initial data burst
    for (let i = 0; i < 20; i++) {
      const t = parseFloat((BASE_PRICE + (Math.random() - 0.5) * 10).toFixed(2))
      setPriceHistory((prev) => [
        ...prev,
        {
          time: new Date(Date.now() - (20 - i) * 1000).toISOString().slice(11, 23),
          midprice: t,
          microprice: parseFloat((t + (Math.random() - 0.5) * 0.5).toFixed(2)),
        },
      ])
    }
    return () => clearInterval(interval)
  }, [tick])

  return {
    price,
    orderBook,
    ticks,
    priceHistory,
    executions,
    mlPrediction,
    riskState,
    obi,
    walkForward,
    position,
  }
}
