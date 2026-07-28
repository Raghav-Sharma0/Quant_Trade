'use client'

import { motion } from 'framer-motion'
import { LineChart, Line, ResponsiveContainer } from 'recharts'
import type { RiskState } from '@/lib/types'
import { cn } from '@/lib/utils'

interface RiskCardProps {
  data: RiskState
}

export function RiskCard({ data }: RiskCardProps) {
  const isActive = data.status === 'ACTIVE'
  const exposurePct = (data.currentExposure / data.maxExposure) * 100
  const sparkData = data.sparkline.map((v) => ({ v }))

  return (
    <div className={cn('glass-card rounded-xl overflow-hidden', isActive ? 'glow-emerald' : 'glow-red')}>
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">RISK ENGINE</span>
        <motion.div
          key={data.status}
          initial={{ scale: 0.9 }}
          animate={{ scale: 1 }}
          className={cn(
            'flex items-center gap-1.5 text-xs font-mono font-bold px-2 py-0.5 rounded-md border',
            isActive
              ? 'text-emerald-400 bg-emerald-500/15 border-emerald-500/30'
              : 'text-red-400 bg-red-500/15 border-red-500/30'
          )}
        >
          <motion.span
            animate={{ opacity: [1, 0.3, 1] }}
            transition={{ repeat: Infinity, duration: 1 }}
            className={cn('w-1.5 h-1.5 rounded-full', isActive ? 'bg-emerald-400' : 'bg-red-400')}
          />
          {data.status}
        </motion.div>
      </div>

      <div className="px-3 py-2 space-y-2.5">
        {/* Latency + Sparkline */}
        <div className="flex items-center justify-between">
          <div>
            <div className="text-[10px] font-mono text-muted-foreground">LATENCY</div>
            <div className="text-sm font-mono font-bold text-foreground/90">{data.latency.toFixed(1)}ns</div>
          </div>
          <div className="w-24 h-8">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={sparkData}>
                <Line
                  type="monotone"
                  dataKey="v"
                  stroke={isActive ? '#10b981' : '#ef4444'}
                  strokeWidth={1.5}
                  dot={false}
                  isAnimationActive={false}
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Risk score */}
        <div className="flex items-center justify-between">
          <span className="text-[10px] font-mono text-muted-foreground">RISK SCORE</span>
          <span
            className={cn(
              'text-sm font-mono font-bold',
              data.riskScore > 75 ? 'text-red-400' : data.riskScore > 50 ? 'text-amber-400' : 'text-emerald-400'
            )}
          >
            {data.riskScore.toFixed(1)}
          </span>
        </div>

        {/* Exposure bar */}
        <div className="space-y-1">
          <div className="flex items-center justify-between">
            <span className="text-[10px] font-mono text-muted-foreground">EXPOSURE</span>
            <span className="text-[10px] font-mono text-foreground/70">
              ${(data.currentExposure / 1000).toFixed(0)}K / ${(data.maxExposure / 1000).toFixed(0)}K
            </span>
          </div>
          <div className="h-1.5 rounded-full bg-white/10 overflow-hidden">
            <motion.div
              className={cn(
                'h-full rounded-full',
                exposurePct > 90 ? 'bg-red-400' : exposurePct > 70 ? 'bg-amber-400' : 'bg-emerald-400'
              )}
              initial={{ width: 0 }}
              animate={{ width: `${exposurePct}%` }}
              transition={{ duration: 0.5 }}
            />
          </div>
        </div>
      </div>
    </div>
  )
}
