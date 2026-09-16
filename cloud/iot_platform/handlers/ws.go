package handlers

import (
	"encoding/json"
	"log"
	"net/http"
	"strings"
	"sync"

	"backend-go/models"
	"backend-go/services"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"gorm.io/gorm"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

func WebSocketHandler(tx *services.BarcodeTx) gin.HandlerFunc {
	return func(c *gin.Context) {
		conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			log.Printf("WebSocket upgrade error: %v", err)
			return
		}

		ch := tx.Subscribe()
		defer func() {
			tx.Unsubscribe(ch)
			conn.Close()
		}()

		done := make(chan struct{})

		go func() {
			defer close(done)
			for {
				_, _, err := conn.ReadMessage()
				if err != nil {
					return
				}
			}
		}()

		for {
			select {
			case <-done:
				return
			case msg, ok := <-ch:
				if !ok {
					return
				}
				if err := conn.WriteMessage(websocket.TextMessage, []byte(msg)); err != nil {
					return
				}
			}
		}
	}
}

func DeviceWebSocketHandler(tx *services.BarcodeTx) gin.HandlerFunc {
	return func(c *gin.Context) {
		conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			log.Printf("Device WebSocket upgrade error: %v", err)
			return
		}

		conn.SetReadLimit(20 * 1024 * 1024)

		db := getDB(c)
		addr := conn.RemoteAddr().String()
		log.Printf("Device WebSocket connected from %s", addr)
		defer func() {
			conn.Close()
			log.Printf("Device WebSocket %s disconnected", addr)
		}()

		for {
			_, message, err := conn.ReadMessage()
			if err != nil {
				return
			}

			if len(message) == 0 {
				continue
			}

			var msg services.DeviceMessage
			if err := json.Unmarshal(message, &msg); err != nil {
				writeWSResponse(conn, services.ServerResponse{Type: "error", Error: "invalid json"})
				continue
			}

			switch msg.Type {
			case "get_categories":
				handleWSGetCategories(conn, db)
			case "upload_barcode":
				handleWSUploadBarcode(conn, db, tx, msg)
			default:
				writeWSResponse(conn, services.ServerResponse{Type: "error", Error: "unknown message type"})
			}
		}
	}
}

func handleWSGetCategories(conn *websocket.Conn, db *gorm.DB) {
	var categories []models.Category
	if err := db.Order("id ASC").Find(&categories).Error; err != nil {
		writeWSResponse(conn, services.ServerResponse{Type: "error", Error: err.Error()})
		return
	}
	writeWSResponse(conn, services.ServerResponse{Type: "categories", Data: categories})
}

func handleWSUploadBarcode(conn *websocket.Conn, db *gorm.DB, tx *services.BarcodeTx, msg services.DeviceMessage) {
	imageURL := msg.Image
	if imageURL != "" && !strings.HasPrefix(imageURL, "/") && !strings.HasPrefix(imageURL, "http") {
		if !strings.HasPrefix(imageURL, "data:") {
			imageURL = "data:image/png;base64," + imageURL
		}
		resolved, err := services.DecodeBase64Image(imageURL)
		if err != nil {
			writeWSResponse(conn, services.ServerResponse{Type: "error", Error: "image decode: " + err.Error()})
			return
		}
		imageURL = resolved
	}
	barcode, err := services.BarcodeService{}.Create(db, msg.Data, imageURL, msg.CategoryID, models.FieldValues(msg.Fields))
	if err != nil {
		writeWSResponse(conn, services.ServerResponse{Type: "error", Error: err.Error()})
		return
	}

	barcodeJSON, _ := json.Marshal(services.WSBroadcast{Event: "barcode", Data: barcode})
	tx.Send(string(barcodeJSON))

	writeWSResponse(conn, services.ServerResponse{Type: "ok", Message: "barcode created"})
}

func writeWSResponse(conn *websocket.Conn, resp services.ServerResponse) {
	data, _ := json.Marshal(resp)
	conn.WriteMessage(websocket.TextMessage, data)
}

// ControlFrontendHandler handles the browser-side control WebSocket at
// /ws/control. Commands sent by the frontend ({"command":"..."}) are forwarded
// verbatim to the connected device, and log broadcasts from the device are
// pushed down to the frontend.
func ControlFrontendHandler(hub *services.ControlHub) gin.HandlerFunc {
	return func(c *gin.Context) {
		conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			log.Printf("Control frontend WebSocket upgrade error: %v", err)
			return
		}
		defer conn.Close()

		ch := hub.RegisterFrontend(conn)
		defer hub.UnregisterFrontend(conn)

		var writeMu sync.Mutex
		writeMsg := func(b []byte) error {
			writeMu.Lock()
			defer writeMu.Unlock()
			return conn.WriteMessage(websocket.TextMessage, b)
		}

		done := make(chan struct{})

		go func() {
			defer close(done)
			for {
				_, message, err := conn.ReadMessage()
				if err != nil {
					return
				}
				if len(message) == 0 {
					continue
				}
				if !hub.SendToDevice(message) {
					writeMsg([]byte(`{"error":"device offline"}`))
				}
			}
		}()

		for {
			select {
			case <-done:
				return
			case msg, ok := <-ch:
				if !ok {
					return
				}
				if err := writeMsg(msg); err != nil {
					return
				}
			}
		}
	}
}

// ControlDeviceHandler handles the device-side control WebSocket at
// /ws/device-control. Messages sent by the device (e.g. {"log":"..."}) are
// broadcast verbatim to all connected frontend control clients. The hub may
// also push {"command":"..."} messages down to the device.
func ControlDeviceHandler(hub *services.ControlHub) gin.HandlerFunc {
	return func(c *gin.Context) {
		conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			log.Printf("Control device WebSocket upgrade error: %v", err)
			return
		}
		defer conn.Close()

		hub.RegisterDevice(conn)
		defer hub.UnregisterDevice(conn)

		log.Printf("Control device WebSocket connected from %s", conn.RemoteAddr())

		for {
			_, message, err := conn.ReadMessage()
			if err != nil {
				return
			}
			if len(message) == 0 {
				continue
			}
			hub.BroadcastToFront(message)
		}
	}
}
