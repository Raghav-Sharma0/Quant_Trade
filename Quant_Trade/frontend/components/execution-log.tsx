'use client'

import { motion, AnimatePresence } from 'framer-motion'
import type { ExecutionEntry } from '@/lib/types'
import { cn } from '@/lib/utils'

interface ExecutionLogProps {
  executions: ExecutionEntry[]
}

const healthColors: Record<string, string> = {
  OK: 'text-emerald-400 bg-emerald-500/10',
  WARN: 'text-amber-400 bg-amber-500/10',
  ERR: 'text-red-400 bg-red-500/10',
}

const modeColors: Record<string, string> = {
  MARKET: 'text-blue-400',
  LIMIT: 'text-emerald-400',
  IOC: 'text-amber-400',
  FOK: 'text-purple-400',
}

export function ExecutionLog({ executions }: ExecutionLogProps) {
  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">EXECUTION LOG</span>
        <span className="text-[10px] font-mono text-muted-foreground">{executions.length} fills</span>
      </div>

      {/* Headers */}
      <div className="grid grid-cols-7 gap-1 px-3 py-1.5 border-b border-border/50 text-[9px] font-mono text-muted-foreground">
        <span>TIME</span>
        <span>SYM</span>
        <span className="text-right">PRICE</span>
        <span className="text-right">SIZE</span>
        <span>MODE</span>
        <span>SEQ</span>
        <span className="text-right">HLTH</span>
      </div>

      {/* Rows */}
      <div className="h-[180px] overflow-y-auto">
        <AnimatePresence initial={false}>
          {executions.map((e) => (
            <motion.div
              key={e.id}
              initial={{ opacity: 0, backgroundColor: 'rgba(59,130,246,0.08)' }}
              animate={{ opacity: 1, backgroundColor: 'rgba(0,0,0,0)' }}
              transition={{ duration: 0.4 }}
              className="grid grid-cols-7 gap-1 px-3 py-1 text-[10px] font-mono hover:bg-white/5 transition-colors"
            >
              <span className="text-muted-foreground tabular-nums">{e.timestamp}</span>
              <span className="text-blue-300/80">{e.symbol}</span>
              <span className="text-right tabular-nums text-foreground/80">{e.price.toFixed(2)}</span>
              <span className="text-right tabular-nums text-foreground/70">{e.size}</span>
              <span className={modeColors[e.mode] ?? 'text-foreground/60'}>{e.mode}</span>
              <span className="text-muted-foreground tabular-nums truncate">{e.sequenceId}</span>
              <span className={cn('text-right text-[9px] px-1 rounded font-bold', healthColors[e.health])}>
                {e.health}
              </span>
            </motion.div>
          ))}
        </AnimatePresence>
      </div>
    </div>
  )
}
