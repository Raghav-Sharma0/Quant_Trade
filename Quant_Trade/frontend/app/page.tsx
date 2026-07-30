'use client'

import Link from 'next/link'
import { motion } from 'framer-motion'
import {
  Cpu,
  Network,
  Brain,
  ArrowRight,
  ChevronRight,
  Zap,
  Shield,
  BarChart2,
  Activity,
  Github,
  Code2,
} from 'lucide-react'
import { Navbar } from '@/components/navbar'
import { BenchmarkCard } from '@/components/benchmark-card'
import { TechStackCard } from '@/components/tech-stack-card'
import { ArchitectureFlow } from '@/components/architecture-flow'
import { PipelineAnimation } from '@/components/pipeline-animation'
import { BackgroundBeams } from '@/components/ui/background-beams'

const benchmarks = [
  {
    label: 'Pre-Trade Risk Check',
    value: '<100 ns',
    numericTarget: 100,
    suffix: ' ns',
    prefix: '<',
    color: 'blue' as const,
    delay: 0,
  },
  {
    label: 'SPSC Queue Transfer',
    value: '<50 ns',
    numericTarget: 50,
    suffix: ' ns',
    prefix: '<',
    color: 'blue' as const,
    delay: 150,
  },
  {
    label: 'Go Throughput',
    value: '100,000+ ticks/sec',
    numericTarget: 100000,
    suffix: '+',
    prefix: '',
    color: 'emerald' as const,
    delay: 300,
  },
  {
    label: 'ML Inference',
    value: '<2.1 ms',
    numericTarget: 2,
    suffix: '.1 ms',
    prefix: '<',
    color: 'emerald' as const,
    delay: 450,
  },
]

const techStack = [
  {
    language: 'C++',
    color: 'blue' as const,
    icon: <Cpu className="w-5 h-5" />,
    badge: 'CORE',
    features: ['Matching Engine', 'Lock-Free Queues', 'Risk Engine'],
    delay: 0,
  },
  {
    language: 'Go',
    color: 'emerald' as const,
    icon: <Network className="w-5 h-5" />,
    badge: 'INGEST',
    features: ['WebSocket Ingestion', 'Binary Deserialization', 'High Throughput'],
    delay: 0.1,
  },
  {
    language: 'Python',
    color: 'amber' as const,
    icon: <Brain className="w-5 h-5" />,
    badge: 'ML',
    features: ['Feature Engineering', 'XGBoost', 'Real-Time Inference'],
    delay: 0.2,
  },
]

export default function LandingPage() {
  return (
    <div className="min-h-screen flex flex-col">
      <Navbar />

      {/* Hero */}
      <section className="relative flex flex-col items-center justify-center text-center px-4 py-20 md:py-28 overflow-hidden">
        {/* Background grid glow */}
        <div className="absolute inset-0 pointer-events-none">
          <div className="absolute top-1/4 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[600px] h-[300px] rounded-full bg-blue-500/5 blur-3xl" />
          <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[400px] h-[200px] rounded-full bg-emerald-500/5 blur-3xl" />
        </div>
        <BackgroundBeams />

        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.5 }}
          className="flex items-center gap-2 px-3 py-1.5 rounded-full border border-blue-500/20 bg-blue-500/5 mb-6"
        >
          <motion.div
            animate={{ opacity: [1, 0.3, 1] }}
            transition={{ repeat: Infinity, duration: 1.5 }}
            className="w-1.5 h-1.5 rounded-full bg-emerald-400"
          />
          <span className="text-xs font-mono text-blue-300/80">PRODUCTION SYSTEM — SIMULATED DATA</span>
        </motion.div>

        <motion.h1
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6, delay: 0.1 }}
          className="text-3xl sm:text-4xl md:text-5xl lg:text-6xl font-bold text-balance leading-tight max-w-4xl mb-6"
        >
          <span className="text-foreground">High-Frequency </span>
          <span className="text-blue-400 text-glow-blue">Polyglot</span>
          <span className="text-foreground"> Trading &amp; </span>
          <span className="text-emerald-400 text-glow-emerald">ML</span>
          <span className="text-foreground"> Orchestration Platform</span>
        </motion.h1>

        <motion.p
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6, delay: 0.2 }}
          className="text-base md:text-lg text-muted-foreground text-balance max-w-2xl mb-8 leading-relaxed"
        >
          Sub-millisecond market data ingestion, lock-free pre-trade risk checks, and real-time
          walk-forward ML inference.
        </motion.p>

        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6, delay: 0.3 }}
          className="flex flex-col sm:flex-row items-center gap-3 mb-14"
        >
          <Link
            href="/dashboard"
            className="flex items-center gap-2 px-6 py-3 rounded-lg bg-blue-500 hover:bg-blue-400 text-white font-semibold text-sm transition-all hover:shadow-[0_0_20px_rgba(59,130,246,0.4)] active:scale-95"
          >
            <Activity className="w-4 h-4" />
            Launch Live Dashboard
            <ArrowRight className="w-4 h-4" />
          </Link>
          <a
            href="#architecture"
            className="flex items-center gap-2 px-6 py-3 rounded-lg border border-border hover:border-blue-500/40 bg-white/5 hover:bg-white/10 text-foreground/80 font-semibold text-sm transition-all"
          >
            <Code2 className="w-4 h-4" />
            View Architecture
          </a>
        </motion.div>

        {/* Pipeline Animation */}
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ duration: 1, delay: 0.5 }}
          className="w-full max-w-2xl"
        >
          <PipelineAnimation />
        </motion.div>
      </section>

      {/* Benchmarks */}
      <section className="px-4 py-16 md:py-20 max-w-6xl mx-auto w-full">
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="text-center mb-10"
        >
          <div className="flex items-center justify-center gap-2 mb-3">
            <Zap className="w-4 h-4 text-blue-400" />
            <span className="text-xs font-mono text-blue-400 uppercase tracking-widest">Performance Benchmarks</span>
          </div>
          <h2 className="text-2xl md:text-3xl font-bold text-foreground text-balance">
            Built for Sub-Microsecond Latency
          </h2>
        </motion.div>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          {benchmarks.map((b) => (
            <BenchmarkCard key={b.label} {...b} />
          ))}
        </div>
      </section>

      {/* Polyglot Stack */}
      <section className="px-4 py-16 md:py-20 max-w-6xl mx-auto w-full">
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="text-center mb-10"
        >
          <div className="flex items-center justify-center gap-2 mb-3">
            <BarChart2 className="w-4 h-4 text-emerald-400" />
            <span className="text-xs font-mono text-emerald-400 uppercase tracking-widest">Polyglot Stack</span>
          </div>
          <h2 className="text-2xl md:text-3xl font-bold text-foreground text-balance">
            Right Tool for Every Layer
          </h2>
        </motion.div>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-5">
          {techStack.map((t) => (
            <TechStackCard key={t.language} {...t} />
          ))}
        </div>
      </section>

      {/* Architecture Flow */}
      <section id="architecture" className="px-4 py-16 md:py-20 max-w-6xl mx-auto w-full">
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="text-center mb-10"
        >
          <div className="flex items-center justify-center gap-2 mb-3">
            <Shield className="w-4 h-4 text-blue-400" />
            <span className="text-xs font-mono text-blue-400 uppercase tracking-widest">System Architecture</span>
          </div>
          <h2 className="text-2xl md:text-3xl font-bold text-foreground text-balance">
            End-to-End Execution Pipeline
          </h2>
          <p className="text-sm text-muted-foreground mt-2">
            From raw market feed to execution — fully automated, lock-free, and resilient
          </p>
        </motion.div>
        <ArchitectureFlow />
      </section>

      {/* CTA */}
      <section className="px-4 py-16 md:py-20 max-w-6xl mx-auto w-full">
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          className="glass-card rounded-2xl p-8 md:p-12 text-center border border-blue-500/20 glow-blue relative overflow-hidden"
        >
          <div className="absolute inset-0 pointer-events-none">
            <div className="absolute top-0 left-1/2 -translate-x-1/2 w-48 h-24 bg-blue-500/10 blur-2xl rounded-full" />
          </div>
          <div className="relative">
            <h2 className="text-2xl md:text-3xl font-bold text-foreground mb-3 text-balance">
              See the System in Action
            </h2>
            <p className="text-muted-foreground text-sm mb-6 max-w-md mx-auto">
              Explore the live simulated dashboard with real-time order book, ML predictions, risk engine, and execution logs.
            </p>
            <Link
              href="/dashboard"
              className="inline-flex items-center gap-2 px-8 py-3 rounded-lg bg-blue-500 hover:bg-blue-400 text-white font-semibold text-sm transition-all hover:shadow-[0_0_24px_rgba(59,130,246,0.5)] active:scale-95"
            >
              <Activity className="w-4 h-4" />
              Open Dashboard
              <ChevronRight className="w-4 h-4" />
            </Link>
          </div>
        </motion.div>
      </section>

      {/* Footer */}
      <footer className="border-t border-border/50 py-6 px-4">
        <div className="max-w-6xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-3">
          <div className="flex items-center gap-2">
            <Activity className="w-4 h-4 text-blue-400" />
            <span className="text-xs font-mono text-muted-foreground">
              HFT Polyglot Platform — Simulated data. No real financial advice.
            </span>
          </div>
          <div className="flex items-center gap-4 text-xs font-mono text-muted-foreground">
            <span>C++ · Go · Python</span>
            <span className="text-blue-400">XGBoost · Recharts</span>
          </div>
        </div>
      </footer>
    </div>
  )
}
