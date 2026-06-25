package websocket

import (

	"sync"
	"time"
"net/http"
	"go.uber.org/zap"
	mdsvc "github.com/anshul/hft/backend/internal/service/marketdata"
	"github.com/anshul/hft/backend/internal/hub"

	"github.com/gorilla/websocket"

)


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

func (g *Gateway) writeTrade(conn *websocket.Conn, trade *hub.Trade, mu *sync.Mutex) error {
	payload, err := g.svc.MarshalTrade(trade)
	if err != nil {
		return err
	}
	mu.Lock()
	defer mu.Unlock()
	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	return conn.WriteMessage(websocket.TextMessage, payload)
}

func (g *Gateway) pingLoop(conn *websocket.Conn, done <-chan struct{}, mu *sync.Mutex) {
	ticker := time.NewTicker(20 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-done:
			return
		case <-ticker.C:
			mu.Lock()
			conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			err := conn.WriteMessage(websocket.PingMessage, nil)
			mu.Unlock()
			if err != nil {
				return
			}
		}
	}
}