import type { Metadata, Viewport } from 'next'
import { JetBrains_Mono, Inter } from 'next/font/google'
import './globals.css'

const inter = Inter({ subsets: ['latin'], variable: '--font-sans-var' })
const jetbrainsMono = JetBrains_Mono({ subsets: ['latin'], variable: '--font-mono-var' })

export const metadata: Metadata = {
  title: 'HFT Polyglot Platform — ML Trading Orchestration',
  description:
    'Sub-millisecond market data ingestion, lock-free pre-trade risk checks, and real-time walk-forward ML inference for high-frequency trading.',
  keywords: ['HFT', 'trading', 'machine learning', 'polyglot', 'C++', 'Go', 'Python', 'XGBoost'],
  generator: 'v0.app',
}

export const viewport: Viewport = {
  colorScheme: 'dark',
  themeColor: '#080c14',
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode
}>) {
  return (
    <html lang="en" className={`bg-background ${inter.variable} ${jetbrainsMono.variable}`}>
      <body className="antialiased font-sans">{children}</body>
    </html>
  )
}
