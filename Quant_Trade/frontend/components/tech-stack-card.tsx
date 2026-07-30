'use client'

import { motion } from 'framer-motion'
import { cn } from '@/lib/utils'
import type { ReactNode } from 'react'

interface TechStackCardProps {
  language: string
  color: 'blue' | 'emerald' | 'amber'
  icon: ReactNode
  features: string[]
  badge: string
  delay?: number
}

const colorMap = {
  blue: {
    border: 'border-blue-500/20 hover:border-blue-400/40',
    badge: 'bg-blue-500/10 text-blue-400 border border-blue-500/30',
    icon: 'text-blue-400',
    glow: 'hover:shadow-blue-500/10',
    dot: 'bg-blue-400',
    featureColor: 'text-blue-300/60',
  },
  emerald: {
    border: 'border-emerald-500/20 hover:border-emerald-400/40',
    badge: 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30',
    icon: 'text-emerald-400',
    glow: 'hover:shadow-emerald-500/10',
    dot: 'bg-emerald-400',
    featureColor: 'text-emerald-300/60',
  },
  amber: {
    border: 'border-amber-500/20 hover:border-amber-400/40',
    badge: 'bg-amber-500/10 text-amber-400 border border-amber-500/30',
    icon: 'text-amber-400',
    glow: 'hover:shadow-amber-500/10',
    dot: 'bg-amber-400',
    featureColor: 'text-amber-300/60',
  },
}

export function TechStackCard({ language, color, icon, features, badge, delay = 0 }: TechStackCardProps) {
  const c = colorMap[color]

  return (
    <motion.div
      initial={{ opacity: 0, y: 30 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true }}
      transition={{ duration: 0.5, delay }}
      whileHover={{ y: -4 }}
      className={cn(
        'glass-card rounded-xl p-6 flex flex-col gap-4 border transition-all duration-300',
        c.border,
        c.glow
      )}
    >
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className={cn('w-10 h-10 rounded-lg bg-background/50 flex items-center justify-center', c.icon)}>
          {icon}
        </div>
        <span className={cn('text-xs font-mono px-2 py-0.5 rounded-md', c.badge)}>{badge}</span>
      </div>

      {/* Language name */}
      <div>
        <h3 className="text-xl font-bold text-foreground font-mono">{language}</h3>
      </div>

      {/* Features */}
      <ul className="space-y-2">
        {features.map((f, i) => (
          <motion.li
            key={f}
            initial={{ opacity: 0, x: -10 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true }}
            transition={{ delay: delay + i * 0.08 }}
            className="flex items-center gap-2.5 text-sm text-muted-foreground"
          >
            <span className={cn('w-1.5 h-1.5 rounded-full flex-shrink-0', c.dot)} />
            {f}
          </motion.li>
        ))}
      </ul>
    </motion.div>
  )
}
