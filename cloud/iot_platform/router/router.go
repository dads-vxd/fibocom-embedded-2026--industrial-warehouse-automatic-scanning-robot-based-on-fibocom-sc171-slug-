package router

import (
	"backend-go/config"
	"backend-go/handlers"
	"backend-go/middleware"
	"backend-go/services"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

func Setup(r *gin.Engine, cfg *config.Config, db *gorm.DB, tx *services.BarcodeTx, hub *services.ControlHub) {
	r.Use(func(c *gin.Context) {
		c.Set("config", cfg)
		c.Set("db", db)
		c.Set("tx", tx)
		c.Set("hub", hub)
		c.Next()
	})

	r.Use(CORSMiddleware())

	userHandler := &handlers.UserHandler{Service: services.UserService{}}
	categoryHandler := &handlers.CategoryHandler{Service: services.CategoryService{}}
	barcodeHandler := &handlers.BarcodeHandler{Service: services.BarcodeService{}}
	videoHandler := &handlers.VideoHandler{Service: services.VideoService{}}

	auth := middleware.AuthMiddleware(cfg)
	admin := middleware.AdminMiddleware()

	r.POST("/users", auth, admin, userHandler.Create)
	r.GET("/users", auth, userHandler.GetAll)
	r.GET("/users/:id", auth, userHandler.GetByID)
	r.PUT("/users/:id", auth, admin, userHandler.Update)
	r.DELETE("/users/:id", auth, admin, userHandler.Delete)

	r.POST("/categories", auth, admin, categoryHandler.Create)
	r.GET("/categories", auth, categoryHandler.GetByPage)
	r.GET("/categories/all", auth, categoryHandler.GetAll)
	r.GET("/categories/:id", auth, categoryHandler.GetByID)
	r.PUT("/categories/:id", auth, admin, categoryHandler.Update)
	r.DELETE("/categories/:id", auth, admin, categoryHandler.Delete)

	r.POST("/barcodes", auth, admin, barcodeHandler.Create)
	r.GET("/barcodes", auth, barcodeHandler.GetByPage)
	r.GET("/barcodes/stats/by-category", auth, barcodeHandler.StatsByCategory)
	r.GET("/barcodes/:id", auth, barcodeHandler.GetByID)
	r.PUT("/barcodes/:id", auth, admin, barcodeHandler.Update)
	r.DELETE("/barcodes/:id", auth, admin, barcodeHandler.Delete)

	r.POST("/auth/send-code", userHandler.SendCode)
	r.POST("/auth/register", userHandler.Register)
	r.POST("/auth/login", userHandler.Login)

	r.GET("/ws/barcode", handlers.WebSocketHandler(tx))
	r.GET("/ws/device", handlers.DeviceWebSocketHandler(tx))
	r.GET("/ws/control", handlers.ControlFrontendHandler(hub))
	r.GET("/ws/device-control", handlers.ControlDeviceHandler(hub))

	r.POST("/videos", videoHandler.Upload)
	r.GET("/videos", videoHandler.List)
	r.GET("/videos/:id", videoHandler.GetByID)
	r.DELETE("/videos/:id", videoHandler.Delete)

	r.Static("/images", "./asset/images")
	r.Static("/video-files", "./asset/videos")
}

func CORSMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Origin, Content-Type, Authorization")
		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	}
}
