package main

import (
	"log"

	"backend-go/config"
	"backend-go/models"
	"backend-go/router"
	"backend-go/services"

	"github.com/gin-gonic/gin"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func main() {
	cfg := config.Load()

	log.Println("Connecting to database...")
	db, err := gorm.Open(postgres.Open(cfg.DatabaseURL), &gorm.Config{})
	if err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	log.Println("Database connection established")

	log.Println("Running migrations...")
	if err := models.AutoMigrate(db); err != nil {
		log.Fatalf("Failed to run migrations: %v", err)
	}
	log.Println("Migrations completed")

	log.Println("Ensuring admin account exists...")
	services.UserService{}.EnsureAdminAccount(db, cfg.AdminEmail, cfg.AdminPassword)

	tx := services.NewBarcodeTx()
	hub := services.NewControlHub()

	go services.StartTCPServer(cfg.DeviceTCPPort, tx, db)

	r := gin.Default()
	router.Setup(r, cfg, db, tx, hub)

	addr := cfg.Addr()
	log.Printf("Server running on http://%s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
