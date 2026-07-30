'use client'

import { motion } from 'framer-motion'
import { cn } from '@/lib/utils'

const nodes = [
  { label: 'Exchange\nSimulator', color: 'blue', sub: 'UDP Multicast' },
  { label: 'Go\nIngestion', color: 'emerald', sub: '100K+ ticks/s' },
  { label: 'Feature\nEngineering', color: 'amber', sub: 'C++ / Python' },
  { label: 'ML\nInference', color: 'purple', sub: '<2.1ms' },
  { label: 'Strategy\nEngine', color: 'blue', sub: 'Lock-Free' },
  { label: 'Execution', color: 'emerald', sub: 'FIX / STP' },
]

const colorMap: Record<string, { bg: string; border: string; text: string; glow: string }> = {
  blue: {
    bg: 'bg-blue-500/10',
    border: 'border-blue-500/30',
    text: 'text-blue-400',
    glow: 'shadow-blue-500/20',
  },
  emerald: {
    bg: 'bg-emerald-500/10',
    border: 'border-emerald-500/30',
    text: 'text-emerald-400',
    glow: 'shadow-emerald-500/20',
  },
  amber: {
    bg: 'bg-amber-500/10',
    border: 'border-amber-500/30',
    text: 'text-amber-400',
    glow: 'shadow-amber-500/20',
  },
  purple: {
    bg: 'bg-purple-500/10',
    border: 'border-purple-500/30',
    text: 'text-purple-400',
    glow: 'shadow-purple-500/20',
  },
}

export function ArchitectureFlow() {
  return (
    <div className="relative">
      <div className="flex flex-wrap items-center justify-center gap-2 md:gap-0 md:flex-nowrap">
        {nodes.map((node, i) => {
          const c = colorMap[node.color]
          return (
            <div key={node.label} className="flex items-center">
              <motion.div
                initial={{ opacity: 0, scale: 0.8 }}
                whileInView={{ opacity: 1, scale: 1 }}
                viewport={{ once: true }}
                transition={{ duration: 0.4, delay: i * 0.1 }}
                whileHover={{ scale: 1.05 }}
                className={cn(
                  'glass-card rounded-xl px-4 py-3 text-center min-w-[100px] border shadow-lg cursor-default',
                  c.bg,
                  c.border,
                  c.glow
                )}
              >
                <div className={cn('text-xs font-mono font-bold leading-tight whitespace-pre-line', c.text)}>
                  {node.label}
                </div>
                <div className="text-[10px] text-muted-foreground mt-1 font-mono">{node.sub}</div>
              </motion.div>

              {/* Arrow */}
              {i < nodes.length - 1 && (
                <div className="hidden md:flex items-center mx-1 relative w-10">
                  <div className="w-full h-px bg-blue-500/20" />
                  {/* Animated particle */}
                  <motion.div
                    className="absolute w-2 h-2 rounded-full bg-blue-400"
                    style={{ boxShadow: '0 0 6px 2px rgba(59,130,246,0.6)' }}
                    animate={{ x: [0, 36, 0], opacity: [0, 1, 0] }}
                    transition={{
                      repeat: Infinity,
                      duration: 1.5,
                      delay: i * 0.25,
                      ease: 'easeInOut',
                    }}
                  />
                  <div
                    className="absolute right-0 w-0 h-0"
                    style={{
                      borderTop: '4px solid transparent',
                      borderBottom: '4px solid transparent',
                      borderLeft: '6px solid rgba(59,130,246,0.4)',
                    }}
                  />
                </div>
              )}
            </div>
          )
        })}
      </div>
    </div>
  )
}
