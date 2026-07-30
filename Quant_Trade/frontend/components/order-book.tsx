'use client'

import { motion, AnimatePresence } from 'framer-motion'
import type { OrderBook as OrderBookType } from '@/lib/types'
import { cn } from '@/lib/utils'

interface OrderBookProps {
  data: OrderBookType
}

function LevelRow({
  price,
  size,
  volume,
  maxVol,
  side,
}: {
  price: number
  size: number
  volume: number
  maxVol: number
  side: 'ask' | 'bid'
}) {
  const pct = Math.min((volume / maxVol) * 100, 100)
  const isAsk = side === 'ask'

  return (
    <div className="relative flex items-center text-xs font-mono h-6 px-2 overflow-hidden group hover:bg-white/5 transition-colors cursor-default">
      {/* Volume bar */}
      <div
        className={cn(
          'absolute inset-y-0 rounded-sm opacity-20 group-hover:opacity-30 transition-opacity',
          isAsk ? 'right-0 bg-red-500' : 'right-0 bg-emerald-500'
        )}
        style={{ width: `${pct}%` }}
      />
      <span className={cn('w-1/3 tabular-nums', isAsk ? 'text-red-400' : 'text-emerald-400')}>
        {price.toFixed(2)}
      </span>
      <span className="w-1/3 tabular-nums text-right text-foreground/80">{size}</span>
      <span className="w-1/3 tabular-nums text-right text-muted-foreground">
        {(volume / 1000).toFixed(1)}K
      </span>
    </div>
  )
}

export function OrderBook({ data }: OrderBookProps) {
  const maxAskVol = Math.max(...data.asks.map((a) => a.volume))
  const maxBidVol = Math.max(...data.bids.map((b) => b.volume))

  return (
    <div className="glass-card rounded-xl overflow-hidden glow-blue">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">LIMIT ORDER BOOK</span>
        <span className="text-xs font-mono text-muted-foreground">ES1!</span>
      </div>

      {/* Column headers */}
      <div className="flex items-center text-[10px] font-mono text-muted-foreground px-2 py-1 border-b border-border/50">
        <span className="w-1/3">PRICE</span>
        <span className="w-1/3 text-right">SIZE</span>
        <span className="w-1/3 text-right">VOL</span>
      </div>

      {/* Asks (top, reversed so best ask is nearest spread) */}
      <div className="py-0.5">
        <div className="text-[10px] font-mono text-red-400/60 px-2 py-0.5">ASKS</div>
        {[...data.asks].reverse().map((level, i) => (
          <LevelRow
            key={`ask-${i}`}
            {...level}
            maxVol={maxAskVol}
            side="ask"
          />
        ))}
      </div>

      {/* Spread */}
      <div className="flex items-center justify-between px-2 py-1.5 bg-blue-500/5 border-y border-blue-500/20">
        <span className="text-[10px] font-mono text-muted-foreground">SPREAD</span>
        <motion.span
          key={data.spread}
          initial={{ color: '#f59e0b' }}
          animate={{ color: '#94a3b8' }}
          transition={{ duration: 0.5 }}
          className="text-xs font-mono font-bold tabular-nums"
        >
          {data.spread.toFixed(2)}
        </motion.span>
        <span className="text-[10px] font-mono text-blue-400">
          MID {data.midprice.toFixed(2)}
        </span>
      </div>

      {/* Bids */}
      <div className="py-0.5">
        <div className="text-[10px] font-mono text-emerald-400/60 px-2 py-0.5">BIDS</div>
        {data.bids.map((level, i) => (
          <LevelRow
            key={`bid-${i}`}
            {...level}
            maxVol={maxBidVol}
            side="bid"
          />
        ))}
      </div>
    </div>
  )
}
