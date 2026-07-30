'use client'

import { BarChart, Bar, XAxis, YAxis, ResponsiveContainer, Cell, Tooltip } from 'recharts'
import type { OrderBook } from '@/lib/types'

interface MarketDepthChartProps {
  data: OrderBook
}

export function MarketDepthChart({ data }: MarketDepthChartProps) {
  const bidsData = data.bids.slice(0, 6).map((b) => ({
    price: b.price.toFixed(1),
    size: b.size,
    type: 'bid',
  }))
  const asksData = data.asks.slice(0, 6).map((a) => ({
    price: a.price.toFixed(1),
    size: a.size,
    type: 'ask',
  }))
  const chartData = [...bidsData.reverse(), ...asksData]

  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border">
        <span className="text-xs font-mono font-semibold text-foreground/80">MARKET DEPTH</span>
      </div>
      <div className="p-2">
        <ResponsiveContainer width="100%" height={120}>
          <BarChart data={chartData} layout="vertical" margin={{ left: -10, right: 4, top: 4, bottom: 4 }}>
            <XAxis
              type="number"
              tick={{ fontSize: 9, fill: '#64748b', fontFamily: 'JetBrains Mono, monospace' }}
              axisLine={false}
              tickLine={false}
            />
            <YAxis
              type="category"
              dataKey="price"
              tick={{ fontSize: 9, fill: '#64748b', fontFamily: 'JetBrains Mono, monospace' }}
              axisLine={false}
              tickLine={false}
              width={50}
            />
            <Tooltip
              contentStyle={{
                background: '#0d1626',
                border: '1px solid rgba(59,130,246,0.2)',
                borderRadius: '8px',
                fontSize: '11px',
                fontFamily: 'JetBrains Mono, monospace',
              }}
              cursor={{ fill: 'rgba(255,255,255,0.03)' }}
            />
            <Bar dataKey="size" radius={[0, 3, 3, 0]}>
              {chartData.map((entry, i) => (
                <Cell
                  key={`cell-${i}`}
                  fill={entry.type === 'bid' ? 'rgba(16,185,129,0.7)' : 'rgba(239,68,68,0.7)'}
                />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </div>
    </div>
  )
}
