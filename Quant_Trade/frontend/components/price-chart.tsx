'use client'

import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts'
import type { PricePoint } from '@/lib/types'

interface PriceChartProps {
  data: PricePoint[]
}

export function PriceChart({ data }: PriceChartProps) {
  const slice = data.slice(-40)
  const allPrices = slice.flatMap((d) => [d.midprice, d.microprice]).filter(Boolean)
  const min = allPrices.length ? Math.min(...allPrices) - 2 : 4300
  const max = allPrices.length ? Math.max(...allPrices) + 2 : 4350

  return (
    <div className="glass-card rounded-xl overflow-hidden glow-blue">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">MIDPRICE vs MICROPRICE</span>
        <span className="text-xs font-mono text-blue-400">LIVE</span>
      </div>
      <div className="p-3">
        <ResponsiveContainer width="100%" height={180}>
          <LineChart data={slice} margin={{ top: 4, right: 4, left: 0, bottom: 0 }}>
            <CartesianGrid strokeDasharray="3 3" stroke="rgba(59,130,246,0.06)" />
            <XAxis
              dataKey="time"
              tick={{ fontSize: 9, fill: '#64748b', fontFamily: 'JetBrains Mono, monospace' }}
              axisLine={false}
              tickLine={false}
              interval="preserveStartEnd"
            />
            <YAxis
              domain={[min, max]}
              tick={{ fontSize: 9, fill: '#64748b', fontFamily: 'JetBrains Mono, monospace' }}
              axisLine={false}
              tickLine={false}
              width={52}
              tickFormatter={(v) => v.toFixed(1)}
            />
            <Tooltip
              contentStyle={{
                background: '#0d1626',
                border: '1px solid rgba(59,130,246,0.2)',
                borderRadius: '8px',
                fontSize: '11px',
                fontFamily: 'JetBrains Mono, monospace',
              }}
              labelStyle={{ color: '#94a3b8' }}
            />
            <Legend
              wrapperStyle={{ fontSize: '10px', fontFamily: 'JetBrains Mono, monospace' }}
            />
            <Line
              type="monotone"
              dataKey="midprice"
              stroke="#3b82f6"
              strokeWidth={1.5}
              dot={false}
              isAnimationActive={false}
              name="Mid"
            />
            <Line
              type="monotone"
              dataKey="microprice"
              stroke="#10b981"
              strokeWidth={1.5}
              dot={false}
              strokeDasharray="4 2"
              isAnimationActive={false}
              name="Micro"
            />
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  )
}
