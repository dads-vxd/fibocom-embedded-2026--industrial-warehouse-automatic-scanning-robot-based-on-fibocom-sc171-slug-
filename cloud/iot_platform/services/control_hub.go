package services

import (
	"log"
	"sync"

	"github.com/gorilla/websocket"
)

type deviceLink struct {
	conn *websocket.Conn
	wmu  sync.Mutex
}

type ControlHub struct {
	mu     sync.Mutex
	device *deviceLink
	fronts map[*websocket.Conn]chan []byte
}

func NewControlHub() *ControlHub {
	return &ControlHub{fronts: make(map[*websocket.Conn]chan []byte)}
}

// RegisterDevice records a new device control connection. Any previously
// registered device connection is closed and replaced.
func (h *ControlHub) RegisterDevice(conn *websocket.Conn) {
	h.mu.Lock()
	prev := h.device
	h.device = &deviceLink{conn: conn}
	h.mu.Unlock()
	if prev != nil {
		prev.conn.Close()
		log.Printf("ControlHub: replaced previous device control connection")
	}
}

// UnregisterDevice clears the device connection if it still points at conn.
func (h *ControlHub) UnregisterDevice(conn *websocket.Conn) {
	h.mu.Lock()
	if h.device != nil && h.device.conn == conn {
		h.device = nil
	}
	h.mu.Unlock()
}

// SendToDevice forwards a raw message to the connected device. Returns false
// when no device is connected or the write fails.
func (h *ControlHub) SendToDevice(msg []byte) bool {
	h.mu.Lock()
	dl := h.device
	h.mu.Unlock()
	if dl == nil {
		return false
	}
	dl.wmu.Lock()
	err := dl.conn.WriteMessage(websocket.TextMessage, msg)
	dl.wmu.Unlock()
	if err != nil {
		log.Printf("ControlHub: device write error: %v", err)
		return false
	}
	return true
}

// RegisterFrontend subscribes a frontend connection to broadcasts and returns
// the channel it should read from.
func (h *ControlHub) RegisterFrontend(conn *websocket.Conn) chan []byte {
	ch := make(chan []byte, 100)
	h.mu.Lock()
	h.fronts[conn] = ch
	h.mu.Unlock()
	return ch
}

// UnregisterFrontend removes a frontend connection and closes its channel.
func (h *ControlHub) UnregisterFrontend(conn *websocket.Conn) {
	h.mu.Lock()
	ch, ok := h.fronts[conn]
	if ok {
		delete(h.fronts, conn)
	}
	h.mu.Unlock()
	if ok {
		close(ch)
	}
}

// BroadcastToFront fans a raw message out to every connected frontend.
func (h *ControlHub) BroadcastToFront(msg []byte) {
	h.mu.Lock()
	for _, ch := range h.fronts {
		select {
		case ch <- msg:
		default:
		}
	}
	h.mu.Unlock()
}
