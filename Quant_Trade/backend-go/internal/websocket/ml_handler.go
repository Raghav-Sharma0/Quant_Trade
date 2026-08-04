package websocket

import (
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

// MLPredictionJSON is the JSON message sent to the frontend over /ws/ml-predictions
type MLPredictionJSON struct {
	Type           string  `json:"type"`
	Symbol         string  `json:"symbol"`
	PriceDirection float64 `json:"price_direction"` // 1=buy, 0=sell
	PredictedValue float64 `json:"predicted_value"` // buy probability [0,1]
	TimestampNs    int64   `json:"timestamp_ns"`
	Connected      bool    `json:"connected"`
}

// HandleMLPredictions streams ML inference results to the frontend.
func (g *Gateway) HandleMLPredictions(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		g.logger.Error("ws upgrade failed", zap.String("path", "/ws/ml-predictions"), zap.Error(err))
		return
	}
	defer conn.Close()

	g.logger.Info("ML predictions client connected")

	predChan := g.mlHub.Subscribe()
	defer g.mlHub.Unsubscribe(predChan)

	var writeMu sync.Mutex
	done := make(chan struct{})
	go g.pingLoop(conn, done, &writeMu)

	// Send an initial connection status message
	status := MLPredictionJSON{Type: "ml_status", Connected: g.mlClient.IsReady()}
	if data, err := json.Marshal(status); err == nil {
		writeMu.Lock()
		_ = conn.WriteMessage(websocket.TextMessage, data)
		writeMu.Unlock()
	}

	// Periodic heartbeat to keep the connection status updated on the frontend
	heartbeat := time.NewTicker(5 * time.Second)
	defer heartbeat.Stop()

	for {
		select {
		case <-done:
			return

		case <-heartbeat.C:
			// Send connection status update
			status := MLPredictionJSON{Type: "ml_status", Connected: g.mlClient.IsReady()}
			data, err := json.Marshal(status)
			if err != nil {
				continue
			}
			writeMu.Lock()
			conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
				writeMu.Unlock()
				close(done)
				return
			}
			writeMu.Unlock()

		case pred, ok := <-predChan:
			if !ok {
				return
			}
			msg := MLPredictionJSON{
				Type:           "ml_prediction",
				Symbol:         pred.Symbol,
				PriceDirection: pred.PriceDirection,
				PredictedValue: pred.PredictedValue,
				TimestampNs:    pred.TimestampNs,
				Connected:      true,
			}
			data, err := json.Marshal(msg)
			if err != nil {
				continue
			}
			writeMu.Lock()
			conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
				writeMu.Unlock()
				g.logger.Debug("ML predictions client disconnected", zap.Error(err))
				close(done)
				return
			}
			writeMu.Unlock()
		}
	}
}


