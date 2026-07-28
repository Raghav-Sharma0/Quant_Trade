'use client'

import { motion } from 'framer-motion'
import type { Position } from '@/lib/types'
import { cn } from '@/lib/utils'

interface PositionCardProps {
  data: Position
}

function PnlRow({ label, value, big }: { label: string; value: number; big?: boolean }) {
  const pos = value >= 0
  return (
    <div className="flex items-center justify-between py-1.5 border-b border-border/50 last:border-0">
      <span className="text-[11px] font-mono text-muted-foreground">{label}</span>
      <motion.span
        key={value.toFixed(0)}
        initial={{ color: pos ? '#10b981' : '#ef4444' }}
        animate={{ color: pos ? '#10b981' : '#ef4444' }}
        className={cn('font-mono tabular-nums', big ? 'text-sm font-bold' : 'text-xs')}
      >
        {pos ? '+' : '-'}${Math.abs(value).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
      </motion.span>
    </div>
  )
}

export function PositionCard({ data }: PositionCardProps) {
  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">POSITION TRACKER</span>
        <span className="text-xs font-mono text-blue-400">ES1!</span>
      </div>
      <div className="px-3 py-2">
        {/* Net position */}
        <div className="flex items-center justify-between py-1.5 border-b border-border/50">
          <span className="text-[11px] font-mono text-muted-foreground">NET POSITION</span>
          <span className={cn('text-sm font-mono font-bold', data.netPosition > 0 ? 'text-emerald-400' : 'text-red-400')}>
            {data.netPosition > 0 ? '+' : ''}{data.netPosition} LOTS
          </span>
        </div>
        {/* Avg entry */}
        <div className="flex items-center justify-between py-1.5 border-b border-border/50">
          <span className="text-[11px] font-mono text-muted-foreground">AVG ENTRY</span>
          <span className="text-xs font-mono text-foreground/80">{data.avgEntry.toFixed(2)}</span>
        </div>
        <PnlRow label="UNREALIZED PNL" value={data.unrealizedPnl} />
        <PnlRow label="REALIZED PNL" value={data.realizedPnl} />
        <PnlRow label="DAILY PNL" value={data.dailyPnl} big />
      </div>
    </div>
  )
}
