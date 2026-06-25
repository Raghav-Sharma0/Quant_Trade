package websocket

import (
	"encoding/json"
	"net/http"
	"time"

	mdsvc "github.com/anshul/hft/backend/internal/service/marketdata"
	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 4096,
	CheckOrigin:     func(r *http.Request) bool { return true }, // dev only
}

type Gateway struct {
	svc    *mdsvc.Service
	logger *zap.Logger
}

func NewGateway(svc *mdsvc.Service, logger *zap.Logger) *Gateway {
	return &Gateway{svc: svc, logger: logger}
}

type subscribeMsg struct {
	Action  string   `json:"action"`
	Symbols []string `json:"symbols"`
}

// readSubscribeFilter waits up to 5s for a {"action":"subscribe","symbols":["AAPL"]} message.
// Returns nil if no valid subscription message arrives (means: stream all symbols).
func (g *Gateway) readSubscribeFilter(conn *websocket.Conn) map[string]struct{} {
	conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	_, raw, err := conn.ReadMessage()
	if err != nil {
		return nil
	}
	conn.SetReadDeadline(time.Time{})

	var msg subscribeMsg
	if err := json.Unmarshal(raw, &msg); err != nil || msg.Action != "subscribe" {
		return nil
	}
	return mdsvc.SymbolFilter(msg.Symbols)
}

// ServeMarketData handles /ws/market-data connections.
// Upgrades HTTP → WebSocket, reads optional symbol filter, then streams live ticks as JSON.
func (g *Gateway) HandleMarketData(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		g.logger.Warn("WebSocket upgrade failed", zap.Error(err))
		return
	}
	defer conn.Close()

	g.logger.Info("Client connected to /ws/market-data", zap.String("remote", r.RemoteAddr))

	filter := g.readSubscribeFilter(conn)

	tickChan := g.svc.TickHub.Subscribe()
	defer g.svc.TickHub.Unsubscribe(tickChan)

	for {
		tick, ok := <-tickChan
		if !ok {
			return
		}

		if !mdsvc.MatchesFilter(tick.Symbol, filter) {
			continue
		}

		data, err := g.svc.MarshalTick(tick)
		if err != nil {
			g.logger.Error("Failed to marshal tick", zap.Error(err))
			continue
		}

		if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
			g.logger.Info("Client disconnected from /ws/market-data", zap.String("remote", r.RemoteAddr))
			return
		}
	}
}

// ServeTrades handles /ws/trades connections.
// Upgrades HTTP → WebSocket, reads optional symbol filter, then streams live trades as JSON.
func (g *Gateway) HandleTrades(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		g.logger.Warn("WebSocket upgrade failed", zap.Error(err))
		return
	}
	defer conn.Close()

	g.logger.Info("Client connected to /ws/trades", zap.String("remote", r.RemoteAddr))

	filter := g.readSubscribeFilter(conn)

	tradeChan := g.svc.TradeHub.Subscribe()
	defer g.svc.TradeHub.Unsubscribe(tradeChan)

	for {
		trade, ok := <-tradeChan
		if !ok {
			return
		}

		if !mdsvc.MatchesFilter(trade.Symbol, filter) {
			continue
		}

		data, err := g.svc.MarshalTrade(trade)
		if err != nil {
			g.logger.Error("Failed to marshal trade", zap.Error(err))
			continue
		}

		if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
			g.logger.Info("Client disconnected from /ws/trades", zap.String("remote", r.RemoteAddr))
			return
		}
	}
}
