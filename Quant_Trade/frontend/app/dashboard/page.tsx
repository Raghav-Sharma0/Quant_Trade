'use client'

import { Navbar } from '@/components/navbar'
import { OrderBook } from '@/components/order-book'
import { MarketDepthChart } from '@/components/market-depth-chart'
import { PriceChart } from '@/components/price-chart'
import { OBIGauge } from '@/components/obi-gauge'
import { TickStream } from '@/components/tick-stream'
import { PredictionCard } from '@/components/prediction-card'
import { WalkForwardCard } from '@/components/walk-forward-card'
import { RiskCard } from '@/components/risk-card'
import { PositionCard } from '@/components/position-card'
import { ExecutionLog } from '@/components/execution-log'
import { useMarketData } from '@/hooks/use-market-data'

export default function DashboardPage() {
  const {
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
  } = useMarketData()

  return (
    <div className="min-h-screen flex flex-col">
      <Navbar price={price} />

      <main className="flex-1 p-3 md:p-4">
        {/* 3-column grid */}
        <div className="grid grid-cols-1 lg:grid-cols-[minmax(220px,280px)_1fr_minmax(220px,280px)] gap-3">

          {/* LEFT COLUMN */}
          <div className="flex flex-col gap-3">
            <OrderBook data={orderBook} />
            <MarketDepthChart data={orderBook} />
          </div>

          {/* CENTER COLUMN */}
          <div className="flex flex-col gap-3">
            <PriceChart data={priceHistory} />
            <OBIGauge data={obi} />
            <TickStream ticks={ticks} />
          </div>

          {/* RIGHT COLUMN */}
          <div className="flex flex-col gap-3">
            <PredictionCard data={mlPrediction} />
            <WalkForwardCard data={walkForward} />
            <RiskCard data={riskState} />
            <PositionCard data={position} />
            <ExecutionLog executions={executions} />
          </div>

        </div>
      </main>
    </div>
  )
}
