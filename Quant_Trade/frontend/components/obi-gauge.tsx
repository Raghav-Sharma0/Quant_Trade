'use client'

import { motion } from 'framer-motion'
import type { OBIValue } from '@/lib/types'
import { cn } from '@/lib/utils'

interface OBIGaugeProps {
  data: OBIValue
}

export function OBIGauge({ data }: OBIGaugeProps) {
  const v = Math.max(-1, Math.min(1, data.value))
  // Map -1..1 to 0..100
  const pct = ((v + 1) / 2) * 100
  // Arc: 210° sweep, starting at 195°
  const startAngle = 195
  const totalSweep = 150
  const valueAngle = startAngle + (pct / 100) * totalSweep

  // SVG arc path helpers
  const toRad = (deg: number) => (deg * Math.PI) / 180
  const cx = 80
  const cy = 80
  const r = 60

  const arcPath = (start: number, end: number, radius: number) => {
    const s = toRad(start)
    const e = toRad(end)
    const x1 = cx + radius * Math.cos(s)
    const y1 = cy + radius * Math.sin(s)
    const x2 = cx + radius * Math.cos(e)
    const y2 = cy + radius * Math.sin(e)
    const large = Math.abs(end - start) > 180 ? 1 : 0
    return `M ${x1} ${y1} A ${radius} ${radius} 0 ${large} 1 ${x2} ${y2}`
  }

  // Needle position
  const needleAngle = valueAngle
  const needleLen = 44
  const nx = cx + needleLen * Math.cos(toRad(needleAngle))
  const ny = cy + needleLen * Math.sin(toRad(needleAngle))

  const color = v > 0.2 ? '#10b981' : v < -0.2 ? '#ef4444' : '#94a3b8'
  const label = v > 0.2 ? 'BID PRESSURE' : v < -0.2 ? 'ASK PRESSURE' : 'BALANCED'

  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">ORDER BOOK IMBALANCE</span>
        <span className="text-xs font-mono text-muted-foreground">OBI</span>
      </div>
      <div className="flex items-center justify-center py-2">
        <div className="relative flex flex-col items-center">
          <svg width="160" height="100" viewBox="0 0 160 110" aria-label={`OBI: ${v.toFixed(3)}`}>
            {/* Background track */}
            <path
              d={arcPath(startAngle, startAngle + totalSweep, r)}
              fill="none"
              stroke="rgba(255,255,255,0.06)"
              strokeWidth={10}
              strokeLinecap="round"
            />
            {/* Red zone (-1 to -0.2) */}
            <path
              d={arcPath(startAngle, startAngle + totalSweep * 0.4, r)}
              fill="none"
              stroke="rgba(239,68,68,0.25)"
              strokeWidth={10}
              strokeLinecap="round"
            />
            {/* Green zone (0.2 to 1) */}
            <path
              d={arcPath(startAngle + totalSweep * 0.6, startAngle + totalSweep, r)}
              fill="none"
              stroke="rgba(16,185,129,0.25)"
              strokeWidth={10}
              strokeLinecap="round"
            />
            {/* Value arc */}
            <path
              d={arcPath(startAngle, valueAngle, r)}
              fill="none"
              stroke={color}
              strokeWidth={10}
              strokeLinecap="round"
              style={{ filter: `drop-shadow(0 0 4px ${color}80)` }}
            />
            {/* Needle */}
            <motion.line
              key={valueAngle}
              initial={false}
              animate={{ x2: nx, y2: ny }}
              transition={{ type: 'spring', stiffness: 120, damping: 14 }}
              x1={cx}
              y1={cy}
              x2={nx}
              y2={ny}
              stroke={color}
              strokeWidth={2}
              strokeLinecap="round"
            />
            <circle cx={cx} cy={cy} r={5} fill={color} />
            {/* Labels */}
            <text x={24} y={100} fill="#ef4444" fontSize="8" fontFamily="JetBrains Mono, monospace" textAnchor="middle">-1</text>
            <text x={80} y={28} fill="#94a3b8" fontSize="8" fontFamily="JetBrains Mono, monospace" textAnchor="middle">0</text>
            <text x={136} y={100} fill="#10b981" fontSize="8" fontFamily="JetBrains Mono, monospace" textAnchor="middle">+1</text>
          </svg>

          {/* Center value */}
          <motion.div
            key={v.toFixed(2)}
            initial={{ scale: 1.1 }}
            animate={{ scale: 1 }}
            className="absolute top-10 flex flex-col items-center"
          >
            <span
              className="text-2xl font-mono font-bold tabular-nums"
              style={{ color }}
            >
              {v >= 0 ? '+' : ''}{v.toFixed(3)}
            </span>
          </motion.div>

          <div className="text-[10px] font-mono mt-1" style={{ color }}>
            {label}
          </div>
        </div>
      </div>
    </div>
  )
}
