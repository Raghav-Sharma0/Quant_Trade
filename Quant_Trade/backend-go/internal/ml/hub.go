package ml

import (
	"sync"
	"sync/atomic"

	"github.com/anshul/hft/backend/generated/proto/prediction"
)

// PredictionHub broadcasts ML prediction results to all WebSocket subscribers.
type PredictionHub struct {
	mu             sync.RWMutex
	subscribers    map[chan *prediction.PredictionResponse]struct{}
	broadcastCount int64
}

func NewPredictionHub() *PredictionHub {
	return &PredictionHub{
		subscribers: make(map[chan *prediction.PredictionResponse]struct{}),
	}
}

func (h *PredictionHub) Subscribe() chan *prediction.PredictionResponse {
	h.mu.Lock()
	defer h.mu.Unlock()
	ch := make(chan *prediction.PredictionResponse, 64)
	h.subscribers[ch] = struct{}{}
	return ch
}

func (h *PredictionHub) Unsubscribe(ch chan *prediction.PredictionResponse) {
	h.mu.Lock()
	defer h.mu.Unlock()
	if _, ok := h.subscribers[ch]; ok {
		delete(h.subscribers, ch)
		close(ch)
	}
}

func (h *PredictionHub) Broadcast(resp *prediction.PredictionResponse) {
	atomic.AddInt64(&h.broadcastCount, 1)
	h.mu.RLock()
	defer h.mu.RUnlock()
	for ch := range h.subscribers {
		select {
		case ch <- resp:
		default:
			// slow consumer — skip rather than block
		}
	}
}

func (h *PredictionHub) TotalBroadcasts() int64 {
	return atomic.LoadInt64(&h.broadcastCount)
}
