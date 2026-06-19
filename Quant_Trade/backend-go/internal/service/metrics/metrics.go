package metrics

import (
	"sync"
	"time"
)

type LatencyStats struct {
	MinNs   int64
	MaxNs   int64
	TotalNs int64
	Count   int64
}

type Service struct {
	mu            sync.RWMutex
	latencies     map[string]*LatencyStats
	throughput    map[string]int64
	lastResetTime time.Time
}

func New() *Service {
	return &Service{
		latencies:     make(map[string]*LatencyStats),
		throughput:    make(map[string]int64),
		lastResetTime: time.Now(),
	}
}

func (s *Service) RecordLatency(name string, duration time.Duration) {
	s.mu.Lock()
	defer s.mu.Unlock()

	ns := duration.Nanoseconds()
	stats, exists := s.latencies[name]
	if !exists {
		stats = &LatencyStats{
			MinNs: ns,
			MaxNs: ns,
		}
		s.latencies[name] = stats
	}

	if ns < stats.MinNs {
		stats.MinNs = ns
	}
	if ns > stats.MaxNs {
		stats.MaxNs = ns
	}
	stats.TotalNs += ns
	stats.Count++
}

func (s *Service) IncrementCounter(name string, delta int64) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.throughput[name] += delta
}

func (s *Service) GetMetrics() map[string]interface{} {
	s.mu.RLock()
	defer s.mu.RUnlock()

	report := make(map[string]interface{})
	
	latenciesReport := make(map[string]map[string]interface{})
	for name, stats := range s.latencies {
		avg := int64(0)
		if stats.Count > 0 {
			avg = stats.TotalNs / stats.Count
		}
		latenciesReport[name] = map[string]interface{}{
			"min_ns": stats.MinNs,
			"max_ns": stats.MaxNs,
			"avg_ns": avg,
			"count":  stats.Count,
		}
	}
	report["latencies"] = latenciesReport

	countersReport := make(map[string]int64)
	for name, val := range s.throughput {
		countersReport[name] = val
	}
	report["counters"] = countersReport
	report["uptime_s"] = time.Since(s.lastResetTime).Seconds()

	return report
}
