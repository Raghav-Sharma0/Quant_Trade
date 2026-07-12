package ingestor

import (
	"context"
	"time"

	"github.com/anshul/hft/backend/generated/proto/marketdata"
	"github.com/anshul/hft/backend/internal/hub"
	"github.com/anshul/hft/backend/internal/storage"
	"go.uber.org/zap"
)

type Dispatcher struct {
	ringBuffer *RingBuffer
	validator  *TickValidator
	writer     *storage.ParquetWriter
	hub        *hub.Hub
	tradeHub   *hub.TradeHub
	logger     *zap.Logger
}

func NewDispatcher(rb *RingBuffer, val *TickValidator, w *storage.ParquetWriter, h *hub.Hub, th *hub.TradeHub, logger *zap.Logger) *Dispatcher {
	return &Dispatcher{
		ringBuffer: rb,
		validator:  val,
		writer:     w,
		hub:        h,
		tradeHub:   th,
		logger:     logger,
	}
}

func (d *Dispatcher) Start(ctx context.Context) {
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			default:
			}

			item, ok := d.ringBuffer.Pop()
			if !ok {
				// Tiny backoff to avoid 100% CPU lock when empty, while maintaining low latency
				time.Sleep(time.Microsecond * 10)
				continue
			}

			switch v := item.(type) {
			case *marketdata.Tick:
				res := d.validator.Validate(v)
				if res.Valid {
					d.writer.Add(v)
					d.hub.Broadcast(v)
				}
			case *hub.Trade:
				d.tradeHub.Broadcast(v)
			default:
				d.logger.Error("Dispatcher received unknown type from ring buffer")
			}
		}
	}()
}
