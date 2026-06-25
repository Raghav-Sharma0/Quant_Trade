package ingestor

import (
	"context"
	"encoding/json"

	"github.com/anshul/hft/backend/generated/proto/marketdata"
	"github.com/anshul/hft/backend/internal/config"
	"github.com/anshul/hft/backend/internal/hub"
	"github.com/anshul/hft/backend/internal/storage"
	"go.uber.org/zap"
)

type Ingestor struct {
	cfg       config.ExchangeConfig
	symbolMap map[uint16]string
	hub       *hub.Hub
	tradeHub  *hub.TradeHub
	writer    *storage.ParquetWriter
	validator *TickValidator
	logger    *zap.Logger
}

func NewIngestor(cfg config.ExchangeConfig, symbolMap map[uint16]string,
	h *hub.Hub, th *hub.TradeHub, w *storage.ParquetWriter, logger *zap.Logger) *Ingestor {
	validator := NewTickValidator(500.0, 5.0, 0.0, logger)
	return &Ingestor{
		cfg:       cfg,
		symbolMap: symbolMap,
		hub:       h,
		tradeHub:  th,
		writer:    w,
		validator: validator,
		logger:    logger,
	}
}

// Run starts the WSClient and begins processing messages from the exchange simulator.
// This is the main loop — called as a goroutine from main.go.
func (n *Ingestor) Run(ctx context.Context) {
	client := NewWSClient(n.cfg, n.logger)
	msgChan := client.Start(ctx) // connects to ws://localhost:8080/market-data

	n.logger.Info("Ingestor started, consuming from exchange simulator",
		zap.String("url", n.cfg.WSURL),
	)

	for {
		select {
		case <-ctx.Done():
			n.validator.Report()
			n.logger.Info("Ingestor stopped")
			return
		case msg, ok := <-msgChan:
			if !ok {
				return
			}
			n.processMessage(msg)
		}
	}
}

func (n *Ingestor) processMessage(msg []byte) {
	var meta struct {
		Type string `json:"type"`
	}
	if err := json.Unmarshal(msg, &meta); err != nil {
		n.logger.Debug("Non-JSON message received", zap.Error(err))
		return
	}

	switch meta.Type {
	case "tick":
		raw, err := ParseRawTick(msg)
		if err != nil {
			n.logger.Error("Failed to parse raw exchange tick", zap.Error(err))
			return
		}
		symbolStr, exists := n.symbolMap[raw.SymbolID]
		if !exists {
			n.logger.Warn("Tick with unknown SymbolID", zap.Uint16("symbol_id", raw.SymbolID))
			return
		}
		tick := &marketdata.Tick{
			TimestampNs: raw.TimestampNs,
			Symbol:      symbolStr,
			Bid:         float64(raw.BestBidPrice),
			Ask:         float64(raw.BestAskPrice),
			BidSz:       float64(raw.BestBidQty),
			AskSz:       float64(raw.BestAskQty),
			LastPrice:   float64(raw.LastTradePrice),
			Volume:      float64(raw.Volume),
			Sequence:    raw.Sequence,
			SeqGap:      false,
		}
		res := n.validator.Validate(tick)
		if !res.Valid {
			return
		}
		n.writer.Add(tick)
		n.hub.Broadcast(tick)

	case "trade":
		raw, err := ParseRawTrade(msg)
		if err != nil {
			n.logger.Error("Failed to parse raw exchange trade", zap.Error(err))
			return
		}
		symbolStr, exists := n.symbolMap[raw.SymbolID]
		if !exists {
			n.logger.Warn("Trade with unknown SymbolID", zap.Uint16("symbol_id", raw.SymbolID))
			return
		}
		trade := &hub.Trade{
			TimestampNs: raw.TimestampNs,
			Symbol:      symbolStr,
			TradeID:     raw.TradeID,
			Price:       float64(raw.Price),
			Quantity:    float64(raw.Quantity),
			BidOrderID:  raw.BidOrderID,
			AskOrderID:  raw.AskOrderID,
			Sequence:    raw.Sequence,
		}
		n.tradeHub.Broadcast(trade)
	}
}