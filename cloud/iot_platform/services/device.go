package services

import (
	"bufio"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"strings"
	"sync"
	"time"

	"backend-go/models"

	"gorm.io/gorm"
)

type BarcodeTx struct {
	mu    sync.RWMutex
	chans map[chan string]struct{}
}

func NewBarcodeTx() *BarcodeTx {
	return &BarcodeTx{chans: make(map[chan string]struct{})}
}

func (tx *BarcodeTx) Subscribe() chan string {
	ch := make(chan string, 100)
	tx.mu.Lock()
	tx.chans[ch] = struct{}{}
	tx.mu.Unlock()
	return ch
}

func (tx *BarcodeTx) Unsubscribe(ch chan string) {
	tx.mu.Lock()
	delete(tx.chans, ch)
	tx.mu.Unlock()
}

func (tx *BarcodeTx) Send(msg string) {
	tx.mu.RLock()
	defer tx.mu.RUnlock()
	for ch := range tx.chans {
		select {
		case ch <- msg:
		default:
		}
	}
}

func DecodeBase64Image(dataURI string) (string, error) {
	if !strings.HasPrefix(dataURI, "data:image/") {
		return dataURI, nil
	}

	parts := strings.SplitN(dataURI[5:], ";", 2)
	if len(parts) != 2 || !strings.HasPrefix(parts[1], "base64,") {
		return dataURI, nil
	}

	ext := ".png"
	dotIdx := strings.LastIndex(parts[0], ".")
	if dotIdx >= 0 {
		ext = parts[0][dotIdx:]
	}

	b64Data := strings.TrimSpace(parts[1][7:])
	fileBytes, err := base64.StdEncoding.DecodeString(b64Data)
	if err != nil {
		return "", fmt.Errorf("base64 decode error: %w", err)
	}

	return SaveImage("upload"+ext, fileBytes)
}

func StartTCPServer(port uint16, tx *BarcodeTx, db *gorm.DB) {
	ln, err := net.Listen("tcp", fmt.Sprintf("0.0.0.0:%d", port))
	if err != nil {
		log.Fatalf("Failed to listen on TCP port %d: %v", port, err)
	}
	log.Printf("Device TCP server listening on port %d", port)
	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Printf("TCP accept error: %v", err)
			continue
		}
		go handleDevice(conn, tx, db)
	}
}

type DeviceMessage struct {
	Type       string            `json:"type"`
	CategoryID int64             `json:"category_id,omitempty"`
	Data       string            `json:"data,omitempty"`
	Image      string            `json:"image,omitempty"`
	Fields     map[string]string `json:"fields,omitempty"`
}

type ServerResponse struct {
	Type    string      `json:"type"`
	Data    interface{} `json:"data,omitempty"`
	Error   string      `json:"error,omitempty"`
	Message string      `json:"message,omitempty"`
}

type WSBroadcast struct {
	Event string      `json:"event"`
	Data  interface{} `json:"data"`
}

func handleDevice(conn net.Conn, tx *BarcodeTx, db *gorm.DB) {
	addr := conn.RemoteAddr().String()
	log.Printf("Device connected from %s", addr)
	defer func() {
		conn.Close()
		log.Printf("Device %s disconnected", addr)
	}()

	scanner := bufio.NewScanner(conn)
	scanner.Buffer(make([]byte, 10*1024*1024), 10*1024*1024)

	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}

		var msg DeviceMessage
		if err := json.Unmarshal(line, &msg); err != nil {
			sendResponse(conn, ServerResponse{Type: "error", Error: "invalid json"})
			continue
		}

		switch msg.Type {
		case "get_categories":
			var categories []models.Category
			if err := db.Order("id ASC").Find(&categories).Error; err != nil {
				sendResponse(conn, ServerResponse{Type: "error", Error: err.Error()})
				continue
			}
			sendResponse(conn, ServerResponse{Type: "categories", Data: categories})

	case "upload_barcode":
		imageURL := msg.Image
		if imageURL != "" && !strings.HasPrefix(imageURL, "/") && !strings.HasPrefix(imageURL, "http") {
			if !strings.HasPrefix(imageURL, "data:") {
				imageURL = "data:image/png;base64," + imageURL
			}
			resolved, err := DecodeBase64Image(imageURL)
			if err != nil {
				sendResponse(conn, ServerResponse{Type: "error", Error: "image decode: " + err.Error()})
				continue
			}
			imageURL = resolved
		}
			barcode, err := BarcodeService{}.Create(db, msg.Data, imageURL, msg.CategoryID, models.FieldValues(msg.Fields))
			if err != nil {
				sendResponse(conn, ServerResponse{Type: "error", Error: err.Error()})
				continue
			}

			msg := WSBroadcast{Event: "barcode", Data: barcode}
			msgBytes, _ := json.Marshal(msg)
			tx.Send(string(msgBytes))

			sendResponse(conn, ServerResponse{Type: "ok", Message: "barcode created"})

		default:
			sendResponse(conn, ServerResponse{Type: "error", Error: "unknown message type"})
		}
	}
}

func sendResponse(conn net.Conn, resp ServerResponse) {
	data, _ := json.Marshal(resp)
	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	conn.Write(append(data, '\n'))
}
