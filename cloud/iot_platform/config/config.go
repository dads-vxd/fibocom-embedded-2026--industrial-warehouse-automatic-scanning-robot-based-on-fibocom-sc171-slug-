package config

import (
	"fmt"
	"os"
	"strconv"

	"github.com/joho/godotenv"
)

type Config struct {
	DatabaseURL   string
	ServerHost    string
	ServerPort    uint16
	JWTSecret     string
	SMTPHost      string
	SMTPPort      int
	SMTPUser      string
	SMTPPassword  string
	AdminEmail    string
	AdminPassword string
	DeviceTCPPort uint16
}

func Load() *Config {
	godotenv.Load()
	return &Config{
		DatabaseURL:   getEnv("DATABASE_URL", "postgres://postgres:postgres@localhost:5432/backend_rust"),
		ServerHost:    getEnv("SERVER_HOST", "127.0.0.1"),
		ServerPort:    getEnvUint16("SERVER_PORT", 3000),
		JWTSecret:     getEnv("JWT_SECRET", "default-secret-change-me"),
		SMTPHost:      getEnv("SMTP_HOST", "smtp.qq.com"),
		SMTPPort:      getEnvInt("SMTP_PORT", 465),
		SMTPUser:      getEnv("SMTP_USER", ""),
		SMTPPassword:  getEnv("SMTP_PASSWORD", ""),
		AdminEmail:    getEnv("ADMIN_EMAIL", "admin@admin.com"),
		AdminPassword: getEnv("ADMIN_PASSWORD", "admin123456"),
		DeviceTCPPort: getEnvUint16("DEVICE_TCP_PORT", 9000),
	}
}

func (c *Config) Addr() string {
	return fmt.Sprintf("%s:%d", c.ServerHost, c.ServerPort)
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func getEnvUint16(key string, fallback uint16) uint16 {
	v := os.Getenv(key)
	if v == "" {
		return fallback
	}
	n, err := strconv.ParseUint(v, 10, 16)
	if err != nil {
		return fallback
	}
	return uint16(n)
}

func getEnvInt(key string, fallback int) int {
	v := os.Getenv(key)
	if v == "" {
		return fallback
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return fallback
	}
	return n
}
