package ingestor

import (
	"errors"
	"sync"
	"time"

	"go.uber.org/zap"
)

type CircuitState int

const (
	StateClosed CircuitState = iota
	StateOpen
	StateHalfOpen
)

var ErrCircuitOpen = errors.New("circuit breaker is open")

type CircuitBreaker struct {
	mu           sync.RWMutex
	state        CircuitState
	failureCount int
	threshold    int
	timeout      time.Duration
	lastFailure  time.Time
	logger       *zap.Logger
}

func NewCircuitBreaker(threshold int, timeout time.Duration, logger *zap.Logger) *CircuitBreaker {
	return &CircuitBreaker{
		state:     StateClosed,
		threshold: threshold,
		timeout:   timeout,
		logger:    logger,
	}
}

func (cb *CircuitBreaker) Allow() bool {
	cb.mu.RLock()
	state := cb.state
	lastFailure := cb.lastFailure
	cb.mu.RUnlock()

	if state == StateClosed {
		return true
	}

	if state == StateOpen {
		if time.Since(lastFailure) > cb.timeout {
			cb.mu.Lock()
			// Double-check inside lock
			if cb.state == StateOpen {
				cb.logger.Info("Circuit breaker transitioning to Half-Open")
				cb.state = StateHalfOpen
			}
			cb.mu.Unlock()
			return true // Allow one test request
		}
		return false
	}

	// StateHalfOpen: only allow one request to pass through by checking externally
	// If it fails, it trips back to Open. If it succeeds, it goes to Closed.
	return true
}

func (cb *CircuitBreaker) RecordSuccess() {
	cb.mu.Lock()
	defer cb.mu.Unlock()

	if cb.state == StateHalfOpen || cb.state == StateOpen {
		cb.logger.Info("Circuit breaker transitioning to Closed")
	}
	cb.state = StateClosed
	cb.failureCount = 0
}

func (cb *CircuitBreaker) RecordFailure() {
	cb.mu.Lock()
	defer cb.mu.Unlock()

	cb.failureCount++
	cb.lastFailure = time.Now()

	if cb.state == StateHalfOpen || cb.failureCount >= cb.threshold {
		if cb.state != StateOpen {
			cb.logger.Warn("Circuit breaker transitioning to Open", zap.Int("failures", cb.failureCount))
		}
		cb.state = StateOpen
	}
}
