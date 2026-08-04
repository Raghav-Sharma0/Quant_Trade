package ml

import (
	"context"
	"time"

	"github.com/anshul/hft/backend/generated/proto/marketdata"
	"go.uber.org/zap"
)

// Worker subscribes to the tick hub and calls the ML predictor for every tick,
// then broadcasts the prediction result to the PredictionHub.
type Worker struct {
	client  *Client
	hub     *PredictionHub
	tickSub chan *marketdata.Tick
	logger  *zap.Logger
}

// NewWorker creates a Worker. tickSub is a channel already subscribed from hub.Hub.
func NewWorker(client *Client, hub *PredictionHub, tickSub chan *marketdata.Tick, logger *zap.Logger) *Worker {
	return &Worker{
		client:  client,
		hub:     hub,
		tickSub: tickSub,
		logger:  logger,
	}
}

// Run starts the prediction loop. It blocks until ctx is cancelled.
// Call as a goroutine: go worker.Run(ctx)
func (w *Worker) Run(ctx context.Context) {
	w.logger.Info("ML prediction worker started")
	// throttle: one prediction per 200ms per symbol to avoid overloading ML service
	ticker := time.NewTicker(200 * time.Millisecond)
	defer ticker.Stop()

	var pending *marketdata.Tick

	for {
		select {
		case <-ctx.Done():
			w.logger.Info("ML prediction worker stopped")
			return

		case tick, ok := <-w.tickSub:
			if !ok {
				return
			}
			// buffer latest tick; we only send the most recent one on each interval
			pending = tick

		case <-ticker.C:
			if pending == nil {
				continue
			}
			tick := pending
			pending = nil

			if !w.client.IsReady() {
				continue
			}

			resp, err := w.client.Predict(
				tick.Symbol,
				tick.Bid, tick.Ask,
				tick.BidSz, tick.AskSz,
				tick.TimestampNs,
			)
			if err != nil {
				w.logger.Debug("ML predict error", zap.String("symbol", tick.Symbol), zap.Error(err))
				continue
			}

			w.hub.Broadcast(resp)
		}
	}
}
