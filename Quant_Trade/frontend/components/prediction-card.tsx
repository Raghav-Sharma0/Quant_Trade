'use client'

import { motion } from 'framer-motion'
import { RadialBarChart, RadialBar, ResponsiveContainer } from 'recharts'
import type { MLPrediction } from '@/lib/types'
import { cn } from '@/lib/utils'

interface PredictionCardProps {
  data: MLPrediction
}

const signalConfig = {
  BUY: { color: '#10b981', bg: 'bg-emerald-500/15', border: 'border-emerald-500/40', text: 'text-emerald-400' },
  SELL: { color: '#ef4444', bg: 'bg-red-500/15', border: 'border-red-500/40', text: 'text-red-400' },
  HOLD: { color: '#94a3b8', bg: 'bg-slate-500/15', border: 'border-slate-500/40', text: 'text-slate-400' },
}

export function PredictionCard({ data }: PredictionCardProps) {
  const sig = signalConfig[data.signal]
  const chartData = [{ value: data.buyProbability, fill: sig.color }]

  return (
    <div className="glass-card rounded-xl overflow-hidden glow-blue">
      <div className="px-3 py-2.5 border-b border-border flex items-center justify-between">
        <span className="text-xs font-mono font-semibold text-foreground/80">ML PREDICTION</span>
        <span className="text-[10px] font-mono text-muted-foreground">{data.modelVersion}</span>
      </div>

      <div className="p-4 flex flex-col items-center gap-3">
        {/* Radial Progress */}
        <div className="relative w-28 h-28">
          <ResponsiveContainer width="100%" height="100%">
            <RadialBarChart
              cx="50%"
              cy="50%"
              innerRadius="65%"
              outerRadius="90%"
              data={chartData}
              startAngle={90}
              endAngle={-270}
            >
              <RadialBar
                dataKey="value"
                cornerRadius={6}
                background={{ fill: 'rgba(255,255,255,0.05)' }}
                isAnimationActive
              />
            </RadialBarChart>
          </ResponsiveContainer>

          {/* Center label */}
          <div className="absolute inset-0 flex flex-col items-center justify-center">
            <motion.span
              key={data.buyProbability}
              initial={{ scale: 1.2, opacity: 0.5 }}
              animate={{ scale: 1, opacity: 1 }}
              className="text-2xl font-mono font-bold tabular-nums"
              style={{ color: sig.color }}
            >
              {data.buyProbability.toFixed(0)}%
            </motion.span>
            <span className="text-[9px] font-mono text-muted-foreground">BUY PROB</span>
          </div>
        </div>

        {/* Signal badge */}
        <motion.div
          key={data.signal}
          initial={{ scale: 0.9, opacity: 0 }}
          animate={{ scale: 1, opacity: 1 }}
          className={cn(
            'px-5 py-1.5 rounded-full border text-sm font-mono font-bold tracking-widest',
            sig.bg,
            sig.border,
            sig.text
          )}
        >
          {data.signal}
        </motion.div>

        {/* Latency */}
        <div className="text-[10px] font-mono text-muted-foreground">
          Inference: <span className="text-foreground/70">{data.inferenceLatency.toFixed(2)}ms</span>
        </div>
      </div>
    </div>
  )
}
