package orders

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/anshul/hft/backend/generated/proto/order"
)

type Service struct {
	mu     sync.RWMutex
	orders map[uint64]*order.Order
}

func New() *Service {
	return &Service{
		orders: make(map[uint64]*order.Order),
	}
}

func (s *Service) SubmitOrder(ctx context.Context, ord *order.Order) (*order.Order, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	ord.Timestamp = uint64(time.Now().UnixNano())
	ord.Status = order.OrderStatus_ORDER_STATUS_ACCEPTED
	s.orders[ord.OrderId] = ord
	return ord, nil
}

func (s *Service) CancelOrder(ctx context.Context, orderID uint64) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	ord, exists := s.orders[orderID]
	if !exists {
		return fmt.Errorf("order %d not found", orderID)
	}

	ord.Status = order.OrderStatus_ORDER_STATUS_CANCELLED
	return nil
}

func (s *Service) GetOrder(orderID uint64) (*order.Order, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	ord, exists := s.orders[orderID]
	return ord, exists
}
