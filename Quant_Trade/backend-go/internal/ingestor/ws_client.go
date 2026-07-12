package ingestor

import (
	"bytes"
	"context"
	"encoding/binary"
	"net/url"
	"time"

	"github.com/anshul/hft/backend/generated/proto/marketdata"
	"github.com/anshul/hft/backend/internal/config"
	"github.com/anshul/hft/backend/internal/hub"
	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

type WSClient struct {
	cfg        config.ExchangeConfig
	logger     *zap.Logger
	ringBuffer *RingBuffer
	breaker    *CircuitBreaker
}

func NewWSClient(cfg config.ExchangeConfig, logger *zap.Logger, rb *RingBuffer, cb *CircuitBreaker) *WSClient {
	return &WSClient{
		cfg:        cfg,
		logger:     logger,
		ringBuffer: rb,
		breaker:    cb,
	}
}

func (c *WSClient) Start(ctx context.Context, symbolMap map[uint16]string) {
	go c.connectLoop(ctx, symbolMap)
}

func (c *WSClient) connectLoop(ctx context.Context, symbolMap map[uint16]string) {
	u, err := url.Parse(c.cfg.WSURL)
	if err != nil {
		c.logger.Error("Invalid exchange WS URL", zap.String("url", c.cfg.WSURL), zap.Error(err))
		return
	}

	delay := time.Duration(c.cfg.ReconnectDelayS * float64(time.Second))
	maxDelay := time.Duration(c.cfg.MaxReconnectDelayS * float64(time.Second))

	dialer := websocket.Dialer{
		ReadBufferSize:  1024 * 1024, // 1MB buffer
		WriteBufferSize: 1024 * 1024,
	}

	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		if !c.breaker.Allow() {
			time.Sleep(delay)
			continue
		}

		c.logger.Info("Connecting to exchange simulator WebSocket", zap.String("url", u.String()))
		conn, _, err := dialer.Dial(u.String(), nil)
		if err != nil {
			c.logger.Warn("Failed to connect to exchange, retrying...", zap.Error(err), zap.Duration("delay", delay))
			c.breaker.RecordFailure()
			select {
			case <-ctx.Done():
				return
			case <-time.After(delay):
				delay = c.backoff(delay, maxDelay)
				continue
			}
		}

		c.logger.Info("Connected to exchange simulator WebSocket successfully")
		c.breaker.RecordSuccess()
		delay = time.Duration(c.cfg.ReconnectDelayS * float64(time.Second)) // Reset delay on successful connection

		// Setup Ping/Pong and Deadlines
		conn.SetReadDeadline(time.Now().Add(15 * time.Second))
		conn.SetPingHandler(func(string) error {
			conn.SetReadDeadline(time.Now().Add(15 * time.Second))
			return conn.WriteControl(websocket.PongMessage, []byte{}, time.Now().Add(time.Second))
		})

		// Start reading messages
		c.readMessages(ctx, conn, symbolMap)

		c.breaker.RecordFailure()
		c.logger.Warn("Connection closed, scheduled for reconnect")
	}
}

func (c *WSClient) readMessages(ctx context.Context, conn *websocket.Conn, symbolMap map[uint16]string) {
	defer conn.Close()

	// Handle close signal from context
	go func() {
		<-ctx.Done()
		conn.WriteMessage(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseNormalClosure, ""))
		conn.Close()
	}()

	for {
		_, message, err := conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				c.logger.Error("Read error from exchange WebSocket", zap.Error(err))
			} else {
				c.logger.Info("Exchange simulator closed WebSocket connection cleanly")
			}
			return
		}

		// Reset deadline on successful read
		conn.SetReadDeadline(time.Now().Add(15 * time.Second))

		if len(message) < 1 {
			continue
		}

		msgType := message[0]
		reader := bytes.NewReader(message)

		if msgType == 1 && len(message) == 43 {
			// WireTick
			var msgType uint8
			var symId uint16
			var ts int64
			var bid, ask, bidsz, asksz, lp, vol uint32
			var seq int64
 
			binary.Read(reader, binary.LittleEndian, &msgType)
			binary.Read(reader, binary.LittleEndian, &symId)
			binary.Read(reader, binary.LittleEndian, &ts)
			binary.Read(reader, binary.LittleEndian, &bid)
			binary.Read(reader, binary.LittleEndian, &ask)
			binary.Read(reader, binary.LittleEndian, &bidsz)
			binary.Read(reader, binary.LittleEndian, &asksz)
			binary.Read(reader, binary.LittleEndian, &lp)
			binary.Read(reader, binary.LittleEndian, &vol)
			binary.Read(reader, binary.LittleEndian, &seq)
 
			symbolStr, ok := symbolMap[symId]
			if !ok {
				continue
			}
 
			tick := &marketdata.Tick{
				TimestampNs: ts,
				Symbol:      symbolStr,
				Bid:         float64(bid),
				Ask:         float64(ask),
				BidSz:       float64(bidsz),
				AskSz:       float64(asksz),
				LastPrice:   float64(lp),
				Volume:      float64(vol),
				Sequence:    seq,
				SeqGap:      false,
			}
			c.ringBuffer.Push(tick)
 
		} else if msgType == 2 && len(message) == 51 {
			// WireTrade
			var msgType uint8
			var symId uint16
			var ts int64
			var price, qty uint32
			var tradeId, bidId, askId uint64
			var seq int64

			binary.Read(reader, binary.LittleEndian, &msgType)
			binary.Read(reader, binary.LittleEndian, &symId)
			binary.Read(reader, binary.LittleEndian, &ts)
			binary.Read(reader, binary.LittleEndian, &price)
			binary.Read(reader, binary.LittleEndian, &qty)
			binary.Read(reader, binary.LittleEndian, &tradeId)
			binary.Read(reader, binary.LittleEndian, &bidId)
			binary.Read(reader, binary.LittleEndian, &askId)
			binary.Read(reader, binary.LittleEndian, &seq)

			symbolStr, ok := symbolMap[symId]
			if !ok {
				continue
			}

			trade := &hub.Trade{
				TimestampNs: ts,
				Symbol:      symbolStr,
				TradeID:     tradeId,
				Price:       float64(price),
				Quantity:    float64(qty),
				BidOrderID:  bidId,
				AskOrderID:  askId,
				Sequence:    seq,
			}
			c.ringBuffer.Push(trade)
		}
	}
}

func (c *WSClient) backoff(current, max time.Duration) time.Duration {
	next := current * 2
	if next > max {
		return max
	}
	return next
}
