package ingestor

import (
	"context"
	"time"

	"github.com/anshul/hft/backend/internal/config"
	"github.com/anshul/hft/backend/internal/hub"
	"github.com/anshul/hft/backend/internal/storage"
	"go.uber.org/zap"
)

type Ingestor struct {
	cfg        config.ExchangeConfig
	symbolMap  map[uint16]string
	hub        *hub.Hub
	tradeHub   *hub.TradeHub
	writer     *storage.ParquetWriter
	validator  *TickValidator
	logger     *zap.Logger
	ringBuffer *RingBuffer
	breaker    *CircuitBreaker
	dispatcher *Dispatcher
}

func NewIngestor(cfg config.ExchangeConfig, symbolMap map[uint16]string,
	h *hub.Hub, th *hub.TradeHub, w *storage.ParquetWriter, logger *zap.Logger) *Ingestor {
	validator := NewTickValidator(500.0, 5.0, 0.0, logger)
	
	// Create high-performance pipeline components
	rb := NewRingBuffer(131072) // 128k power-of-two size
	cb := NewCircuitBreaker(5, 10*time.Second, logger)
	dispatcher := NewDispatcher(rb, validator, w, h, th, logger)

	return &Ingestor{
		cfg:        cfg,
		symbolMap:  symbolMap,
		hub:        h,
		tradeHub:   th,
		writer:     w,
		validator:  validator,
		logger:     logger,
		ringBuffer: rb,
		breaker:    cb,
		dispatcher: dispatcher,
	}
}

// Run starts the WSClient and Dispatcher.
func (n *Ingestor) Run(ctx context.Context) {
	n.logger.Info("Ingestor starting high-performance pipeline",
		zap.String("url", n.cfg.WSURL),
	)

	// Start the Fan-Out Dispatcher
	n.dispatcher.Start(ctx)

	// Start the Binary WS Client (pushes directly to RingBuffer)
	client := NewWSClient(n.cfg, n.logger, n.ringBuffer, n.breaker)
	client.Start(ctx, n.symbolMap)

	<-ctx.Done()
	
	n.validator.Report()
	n.logger.Info("Ingestor stopped")
}