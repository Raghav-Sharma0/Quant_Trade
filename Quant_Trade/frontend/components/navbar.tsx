'use client'

import { useState, useEffect } from 'react'
import Link from 'next/link'
import { usePathname } from 'next/navigation'
import { motion } from 'framer-motion'
import { Activity, Wifi, WifiOff, ChevronRight } from 'lucide-react'
import { cn } from '@/lib/utils'

interface NavbarProps {
  price?: number
}

export function Navbar({ price }: NavbarProps) {
  const pathname = usePathname()
  const isDashboard = pathname === '/dashboard'
  const [time, setTime] = useState('')
  const [connected, setConnected] = useState(true)

  useEffect(() => {
    const update = () => setTime(new Date().toISOString().slice(11, 19) + ' UTC')
    update()
    const t = setInterval(update, 1000)
    return () => clearInterval(t)
  }, [])

  return (
    <header className="sticky top-0 z-50 w-full glass-card border-b border-[var(--glass-border)]">
      <div className="flex items-center justify-between px-4 md:px-6 h-14">
        {/* Logo */}
        <Link href="/" className="flex items-center gap-2 group">
          <div className="relative w-7 h-7">
            <div className="absolute inset-0 rounded-md bg-blue-500/20 border border-blue-500/40 group-hover:border-blue-400/70 transition-colors" />
            <Activity className="absolute inset-0.5 w-6 h-6 text-blue-400" />
          </div>
          <span className="font-mono text-sm font-bold tracking-wider text-blue-400 hidden sm:block">
            HFT<span className="text-foreground/60">://</span>POLY
          </span>
        </Link>

        {/* Center title on dashboard */}
        {isDashboard && (
          <div className="hidden md:flex items-center gap-2 text-xs font-mono text-muted-foreground">
            <ChevronRight className="w-3 h-3 text-blue-500" />
            <span className="text-foreground/80">LIVE TRADING DASHBOARD</span>
            <ChevronRight className="w-3 h-3 text-blue-500" />
            {price !== undefined && (
              <span className="text-emerald-400 font-semibold">
                ES1! {price.toFixed(2)}
              </span>
            )}
          </div>
        )}

        {/* Right side */}
        <div className="flex items-center gap-3 md:gap-4">
          {isDashboard && (
            <div className="flex items-center gap-1.5">
              <motion.div
                animate={{ opacity: [1, 0.3, 1] }}
                transition={{ repeat: Infinity, duration: 1.5 }}
                className={cn('w-2 h-2 rounded-full', connected ? 'bg-emerald-400' : 'bg-red-400')}
              />
              <span className="text-xs font-mono text-muted-foreground hidden sm:block">
                {connected ? 'LIVE' : 'DISCONNECTED'}
              </span>
              {connected ? (
                <Wifi className="w-3 h-3 text-emerald-400" />
              ) : (
                <WifiOff className="w-3 h-3 text-red-400" />
              )}
            </div>
          )}

          <span className="text-xs font-mono text-muted-foreground hidden sm:block">{time}</span>

          {!isDashboard && (
            <Link
              href="/dashboard"
              className="px-3 py-1.5 rounded-md bg-blue-500/10 border border-blue-500/30 text-blue-400 text-xs font-mono font-medium hover:bg-blue-500/20 hover:border-blue-400/50 transition-all"
            >
              Dashboard
            </Link>
          )}
        </div>
      </div>
    </header>
  )
}
