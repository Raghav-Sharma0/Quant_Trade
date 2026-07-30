'use client'

import { motion } from 'framer-motion'
import type { WalkForwardStatus } from '@/lib/types'
import { cn } from '@/lib/utils'

interface WalkForwardCardProps {
  data: WalkForwardStatus
}

const statusColors: Record<string, string> = {
  TRAINING: 'text-amber-400 bg-amber-500/15 border-amber-500/30',
  LIVE: 'text-emerald-400 bg-emerald-500/15 border-emerald-500/30',
  STALE: 'text-orange-400 bg-orange-500/15 border-orange-500/30',
  FAILED: 'text-red-400 bg-red-500/15 border-red-500/30',
  OK: 'text-emerald-400 bg-emerald-500/15 border-emerald-500/30',
  RECOVERING: 'text-amber-400 bg-amber-500/15 border-amber-500/30',
  HEALTHY: 'text-emerald-400 bg-emerald-500/15 border-emerald-500/30',
  DEGRADED: 'text-amber-400 bg-amber-500/15 border-amber-500/30',
  CRITICAL: 'text-red-400 bg-red-500/15 border-red-500/30',
}

function StatusBadge({ value }: { value: string }) {
  return (
    <motion.span
      key={value}
      initial={{ opacity: 0, scale: 0.9 }}
      animate={{ opacity: 1, scale: 1 }}
      className={cn(
        'text-[10px] font-mono px-2 py-0.5 rounded-md border',
        statusColors[value] ?? 'text-muted-foreground bg-muted border-border'
      )}
    >
      {value}
    </motion.span>
  )
}

function Row({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between py-1.5 border-b border-border/50 last:border-0">
      <span className="text-[11px] font-mono text-muted-foreground">{label}</span>
      {children}
    </div>
  )
}

export function WalkForwardCard({ data }: WalkForwardCardProps) {
  return (
    <div className="glass-card rounded-xl overflow-hidden">
      <div className="px-3 py-2.5 border-b border-border">
        <span className="text-xs font-mono font-semibold text-foreground/80">WALK FORWARD STATUS</span>
      </div>
      <div className="px-3 py-2">
        <Row label="Model Status">
          <StatusBadge value={data.modelStatus} />
        </Row>
        <Row label="Version Hash">
          <span className="text-[11px] font-mono text-blue-400">{data.versionHash}</span>
        </Row>
        <Row label="Gap Recovery">
          <StatusBadge value={data.gapRecovery} />
        </Row>
        <Row label="Latency">
          <span className="text-[11px] font-mono text-foreground/70">{data.latency.toFixed(2)}ms</span>
        </Row>
        <Row label="Health">
          <StatusBadge value={data.health} />
        </Row>
      </div>
    </div>
  )
}
