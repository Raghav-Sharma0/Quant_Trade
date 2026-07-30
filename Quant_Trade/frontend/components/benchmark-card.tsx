'use client'

import { useEffect, useState, useRef } from 'react'
import { motion } from 'framer-motion'
import { cn } from '@/lib/utils'

interface BenchmarkCardProps {
  label: string
  value: string
  numericTarget: number
  suffix: string
  prefix?: string
  color: 'blue' | 'emerald'
  delay?: number
}

export function BenchmarkCard({
  label,
  value,
  numericTarget,
  suffix,
  prefix = '',
  color,
  delay = 0,
}: BenchmarkCardProps) {
  const [displayNum, setDisplayNum] = useState(0)
  const [started, setStarted] = useState(false)
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) {
          setStarted(true)
          observer.disconnect()
        }
      },
      { threshold: 0.3 }
    )
    if (ref.current) observer.observe(ref.current)
    return () => observer.disconnect()
  }, [])

  useEffect(() => {
    if (!started) return
    const timer = setTimeout(() => {
      let start = 0
      const duration = 1800
      const startTime = performance.now()
      const animate = (now: number) => {
        const elapsed = now - startTime
        const progress = Math.min(elapsed / duration, 1)
        const eased = 1 - Math.pow(1 - progress, 3)
        setDisplayNum(Math.round(eased * numericTarget))
        if (progress < 1) requestAnimationFrame(animate)
      }
      requestAnimationFrame(animate)
    }, delay)
    return () => clearTimeout(timer)
  }, [started, numericTarget, delay])

  return (
    <motion.div
      ref={ref}
      initial={{ opacity: 0, y: 20 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true }}
      transition={{ duration: 0.5, delay: delay / 1000 }}
      whileHover={{ scale: 1.02 }}
      className={cn(
        'glass-card rounded-xl p-5 flex flex-col gap-2 cursor-default transition-shadow duration-300',
        color === 'blue' ? 'glow-blue hover:glow-blue' : 'glow-emerald hover:glow-emerald'
      )}
    >
      <span className="text-xs font-mono text-muted-foreground uppercase tracking-widest">
        {label}
      </span>
      <div
        className={cn(
          'text-3xl font-mono font-bold tabular-nums',
          color === 'blue' ? 'text-blue-400' : 'text-emerald-400'
        )}
      >
        {prefix}
        {displayNum.toLocaleString()}
        <span className="text-lg ml-1 text-muted-foreground">{suffix}</span>
      </div>
      <div
        className={cn(
          'text-sm font-mono font-semibold',
          color === 'blue' ? 'text-blue-300/70' : 'text-emerald-300/70'
        )}
      >
        {value}
      </div>
      {/* Decorative bar */}
      <motion.div
        initial={{ scaleX: 0 }}
        whileInView={{ scaleX: 1 }}
        viewport={{ once: true }}
        transition={{ duration: 1, delay: delay / 1000 + 0.3 }}
        className={cn(
          'h-0.5 rounded-full origin-left',
          color === 'blue' ? 'bg-blue-500/40' : 'bg-emerald-500/40'
        )}
      />
    </motion.div>
  )
}
