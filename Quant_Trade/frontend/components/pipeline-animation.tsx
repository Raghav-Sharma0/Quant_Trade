'use client'

import { useEffect, useRef } from 'react'

interface Particle {
  progress: number
  speed: number
  color: string
  opacity: number
  size: number
}

const NODES = [
  { x: 0.1, label: 'C++', color: '#3b82f6', desc: 'Matching Engine' },
  { x: 0.5, label: 'Go', color: '#10b981', desc: 'WebSocket Ingestion' },
  { x: 0.9, label: 'Python', color: '#f59e0b', desc: 'ML Inference' },
]

export function PipelineAnimation() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const particlesRef = useRef<Particle[]>([])
  const animRef = useRef<number>(0)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    const resize = () => {
      canvas.width = canvas.offsetWidth * window.devicePixelRatio
      canvas.height = canvas.offsetHeight * window.devicePixelRatio
      ctx.scale(window.devicePixelRatio, window.devicePixelRatio)
    }
    resize()
    window.addEventListener('resize', resize)

    // Initialize particles
    particlesRef.current = Array.from({ length: 30 }, (_, i) => ({
      progress: i / 30,
      speed: 0.003 + Math.random() * 0.004,
      color: ['#3b82f6', '#10b981', '#f59e0b'][Math.floor(Math.random() * 3)],
      opacity: 0.3 + Math.random() * 0.7,
      size: 2 + Math.random() * 3,
    }))

    const draw = () => {
      const w = canvas.offsetWidth
      const h = canvas.offsetHeight
      ctx.clearRect(0, 0, w, h)

      const cy = h / 2

      // Draw pipeline track
      ctx.beginPath()
      ctx.moveTo(w * 0.05, cy)
      ctx.lineTo(w * 0.95, cy)
      ctx.strokeStyle = 'rgba(59,130,246,0.1)'
      ctx.lineWidth = 2
      ctx.stroke()

      // Draw secondary tracks
      const offsets = [-20, 20]
      offsets.forEach((offset) => {
        ctx.beginPath()
        ctx.moveTo(w * 0.05, cy + offset)
        ctx.lineTo(w * 0.95, cy + offset)
        ctx.strokeStyle = 'rgba(59,130,246,0.04)'
        ctx.lineWidth = 1
        ctx.stroke()
      })

      // Draw nodes
      NODES.forEach(({ x, label, color, desc }) => {
        const nx = w * x
        const nodeRadius = 28

        // Outer glow ring
        const gradient = ctx.createRadialGradient(nx, cy, 0, nx, cy, nodeRadius * 2.5)
        gradient.addColorStop(0, color + '30')
        gradient.addColorStop(1, 'transparent')
        ctx.beginPath()
        ctx.arc(nx, cy, nodeRadius * 2.5, 0, Math.PI * 2)
        ctx.fillStyle = gradient
        ctx.fill()

        // Node circle
        ctx.beginPath()
        ctx.arc(nx, cy, nodeRadius, 0, Math.PI * 2)
        ctx.fillStyle = 'rgba(8,12,20,0.9)'
        ctx.fill()
        ctx.strokeStyle = color + '70'
        ctx.lineWidth = 1.5
        ctx.stroke()

        // Inner circle
        ctx.beginPath()
        ctx.arc(nx, cy, nodeRadius * 0.6, 0, Math.PI * 2)
        ctx.fillStyle = color + '20'
        ctx.fill()

        // Label
        ctx.fillStyle = color
        ctx.font = 'bold 11px JetBrains Mono, monospace'
        ctx.textAlign = 'center'
        ctx.textBaseline = 'middle'
        ctx.fillText(label, nx, cy)

        // Description
        ctx.fillStyle = 'rgba(148,163,184,0.7)'
        ctx.font = '9px JetBrains Mono, monospace'
        ctx.fillText(desc, nx, cy + nodeRadius + 14)
      })

      // Animate particles
      particlesRef.current.forEach((p) => {
        p.progress += p.speed
        if (p.progress > 1) p.progress = 0

        const px = w * (0.05 + p.progress * 0.9)

        // Glow effect
        const glowGrad = ctx.createRadialGradient(px, cy, 0, px, cy, p.size * 3)
        glowGrad.addColorStop(0, p.color + 'ff')
        glowGrad.addColorStop(1, 'transparent')
        ctx.beginPath()
        ctx.arc(px, cy, p.size * 3, 0, Math.PI * 2)
        ctx.fillStyle = glowGrad
        ctx.globalAlpha = p.opacity * 0.4
        ctx.fill()

        // Core particle
        ctx.beginPath()
        ctx.arc(px, cy, p.size, 0, Math.PI * 2)
        ctx.fillStyle = p.color
        ctx.globalAlpha = p.opacity
        ctx.fill()
        ctx.globalAlpha = 1
      })

      animRef.current = requestAnimationFrame(draw)
    }
    draw()

    return () => {
      cancelAnimationFrame(animRef.current)
      window.removeEventListener('resize', resize)
    }
  }, [])

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-32 md:h-44"
      aria-label="Animated pipeline showing C++ → Go → Python data flow"
    />
  )
}
