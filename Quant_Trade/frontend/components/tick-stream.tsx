'use client'

import { motion, AnimatePresence } from 'framer-motion'
import type { TickEntry } from '@/lib/types'
import { cn } from '@/lib/utils'

interface TickStreamProps {
  ticks: TickEntry[]
}

export function TickStream({ ticks }: TickStreamProps) {
  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">LIVE TICK STREAM</span>
        <motion.span
          animate={{ opacity: [1, 0.3, 1] }}
          transition={{ repeat: Infinity, duration: 1 }}
          className="text-[10px] font-mono text-emerald-400"
        >
          ● STREAMING
        </motion.span>
      </div>

      {/* Headers */}
      <div className="grid grid-cols-5 gap-1 px-3 py-1.5 border-b border-border/50 text-[10px] font-mono text-muted-foreground">
        <span>TIME</span>
        <span className="text-right">PRICE</span>
        <span className="text-right">VOL</span>
        <span className="text-right">SIDE</span>
        <span className="text-right">SEQ</span>
      </div>

      {/* Rows */}
      <div className="h-[160px] overflow-hidden relative">
        <div className="overflow-y-auto h-full">
          <AnimatePresence initial={false}>
            {ticks.slice(0, 20).map((tick) => (
              <motion.div
                key={tick.id}
                initial={{ opacity: 0, y: -12, backgroundColor: tick.side === 'BUY' ? 'rgba(16,185,129,0.08)' : 'rgba(239,68,68,0.08)' }}
                animate={{ opacity: 1, y: 0, backgroundColor: 'rgba(0,0,0,0)' }}
                transition={{ duration: 0.3 }}
                className="grid grid-cols-5 gap-1 px-3 py-1 text-xs font-mono hover:bg-white/5 transition-colors"
              >
                <span className="text-muted-foreground tabular-nums">{tick.timestamp}</span>
                <span className="text-right tabular-nums text-foreground/80">{tick.price.toFixed(2)}</span>
                <span className="text-right tabular-nums text-muted-foreground">{tick.volume}</span>
                <span
                  className={cn(
                    'text-right font-semibold',
                    tick.side === 'BUY' ? 'text-emerald-400' : 'text-red-400'
                  )}
                >
                  {tick.side}
                </span>
                <span className="text-right text-muted-foreground tabular-nums">{tick.sequence}</span>
              </motion.div>
            ))}
          </AnimatePresence>
        </div>
      </div>
    </div>
  )
}
