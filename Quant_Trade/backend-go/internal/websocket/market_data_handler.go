package websocket

import (

	"sync"
	"time"

	"github.com/anshul/hft/backend/generated/proto/marketdata"
	
	"github.com/gorilla/websocket"

)


func (g *Gateway) writeTick(conn *websocket.Conn, tick *marketdata.Tick, mu *sync.Mutex) error {
	payload, err := g.svc.MarshalTick(tick)
	if err != nil {
		return err
	}
	mu.Lock()
	defer mu.Unlock()
	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	return conn.WriteMessage(websocket.TextMessage, payload)
}

func filterKeys(filter map[string]struct{}) []string {
	if filter == nil {
		return nil
	}
	out := make([]string, 0, len(filter))
	for k := range filter {
		out = append(out, k)
	}
	return out
}