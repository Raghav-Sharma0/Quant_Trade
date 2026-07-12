package ingestor

import (
	"sync/atomic"
)

// RingBuffer is a high-performance Lock-Free Single-Producer Single-Consumer (SPSC) circular queue.
type RingBuffer struct {
	buffer []interface{}
	mask   uint64
	_pad0  [56]byte // CPU cache line padding to prevent false sharing
	head   uint64   // written by producer
	_pad1  [56]byte
	tail   uint64   // read by consumer
	_pad2  [56]byte
}

func NewRingBuffer(size uint64) *RingBuffer {
	// Round up to next power of 2 for fast bitwise masking instead of modulo
	powerOfTwo := uint64(1)
	for powerOfTwo < size {
		powerOfTwo <<= 1
	}
	return &RingBuffer{
		buffer: make([]interface{}, powerOfTwo),
		mask:   powerOfTwo - 1,
		head:   0,
		tail:   0,
	}
}

// Push adds an item to the buffer. Returns false if full.
func (rb *RingBuffer) Push(item interface{}) bool {
	head := atomic.LoadUint64(&rb.head)
	tail := atomic.LoadUint64(&rb.tail)

	if head-tail >= uint64(len(rb.buffer)) {
		return false // Buffer Full
	}

	rb.buffer[head&rb.mask] = item
	atomic.StoreUint64(&rb.head, head+1)
	return true
}

// Pop removes and returns an item from the buffer. Returns false if empty.
func (rb *RingBuffer) Pop() (interface{}, bool) {
	tail := atomic.LoadUint64(&rb.tail)
	head := atomic.LoadUint64(&rb.head)

	if tail == head {
		return nil, false // Buffer Empty
	}

	item := rb.buffer[tail&rb.mask]
	atomic.StoreUint64(&rb.tail, tail+1)
	return item, true
}
