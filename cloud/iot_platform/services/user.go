package services

import (
	"crypto/tls"
	"fmt"
	"math/rand"
	"net/smtp"
	"sync"
	"time"

	"backend-go/config"
	"backend-go/models"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

var codeCache sync.Map

type codeEntry struct {
	Code      string
	ExpiresAt time.Time
}

type UserService struct{}

func (UserService) EnsureAdminAccount(db *gorm.DB, email, password string) {
	var user models.User
	if err := db.Where("role = 1").First(&user).Error; err == nil {
		fmt.Println("Admin account already exists")
		return
	}

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		fmt.Printf("Failed to hash admin password: %v\n", err)
		return
	}

	admin := models.User{
		ID:       uuid.New(),
		Username: "admin",
		Email:    email,
		Password: string(hashedPassword),
		Role:     1,
	}

	if err := db.Create(&admin).Error; err != nil {
		fmt.Printf("Failed to create admin account: %v\n", err)
		return
	}
	fmt.Printf("Admin account created: %s\n", email)
}

func (UserService) SendCode(cfg *config.Config, email string) error {
	code := fmt.Sprintf("%06d", rand.Intn(1000000))
	codeCache.Store(email, codeEntry{
		Code:      code,
		ExpiresAt: time.Now().Add(5 * time.Minute),
	})

	return sendEmail(cfg, email, code)
}

func (UserService) Register(db *gorm.DB, input models.RegisterInput) (*models.UserResponse, error) {
	val, ok := codeCache.Load(input.Email)
	if !ok {
		return nil, fmt.Errorf("Verification code not found, please request one first")
	}
	entry := val.(codeEntry)
	if time.Now().After(entry.ExpiresAt) || entry.Code != input.Code {
		return nil, fmt.Errorf("Verification code expired or invalid")
	}
	codeCache.Delete(input.Email)

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(input.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("Password hashing failed")
	}

	user := models.User{
		ID:       uuid.New(),
		Username: input.Email,
		Email:    input.Email,
		Password: string(hashedPassword),
		Role:     0,
	}

	if err := db.Create(&user).Error; err != nil {
		return nil, fmt.Errorf("Email already registered")
	}

	resp := user.ToResponse()
	return &resp, nil
}

func (UserService) Login(db *gorm.DB, jwtSecret string, input models.LoginInput) (*models.LoginResponse, error) {
	var user models.User
	if err := db.Where("email = ?", input.Email).First(&user).Error; err != nil {
		return nil, fmt.Errorf("Invalid email or password")
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(input.Password)); err != nil {
		return nil, fmt.Errorf("Invalid email or password")
	}

	claims := models.Claims{
		Sub:  user.ID.String(),
		Role: user.Role,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(24 * time.Hour)),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, err := token.SignedString([]byte(jwtSecret))
	if err != nil {
		return nil, fmt.Errorf("Token generation failed")
	}

	return &models.LoginResponse{
		Token: tokenStr,
		User:  user.ToResponse(),
	}, nil
}

func (UserService) Create(db *gorm.DB, input models.CreateUserInput) (*models.UserResponse, error) {
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(input.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("Password hashing failed")
	}

	user := models.User{
		ID:       uuid.New(),
		Username: input.Username,
		Email:    input.Email,
		Password: string(hashedPassword),
		Role:     0,
	}

	if err := db.Create(&user).Error; err != nil {
		return nil, err
	}

	resp := user.ToResponse()
	return &resp, nil
}

func (UserService) FindAll(db *gorm.DB) ([]models.UserResponse, error) {
	var users []models.User
	if err := db.Order("created_at DESC").Find(&users).Error; err != nil {
		return nil, err
	}

	responses := make([]models.UserResponse, len(users))
	for i, u := range users {
		responses[i] = u.ToResponse()
	}
	return responses, nil
}

func (UserService) FindByID(db *gorm.DB, id uuid.UUID) (*models.UserResponse, error) {
	var user models.User
	if err := db.First(&user, "id = ?", id).Error; err != nil {
		return nil, fmt.Errorf("User with id %s not found", id)
	}
	resp := user.ToResponse()
	return &resp, nil
}

func (UserService) Update(db *gorm.DB, id uuid.UUID, input models.UpdateUserInput) (*models.UserResponse, error) {
	var user models.User
	if err := db.First(&user, "id = ?", id).Error; err != nil {
		return nil, fmt.Errorf("User with id %s not found", id)
	}

	if input.Username != nil {
		user.Username = *input.Username
	}
	if input.Email != nil {
		user.Email = *input.Email
	}
	if input.Password != nil {
		hashedPassword, err := bcrypt.GenerateFromPassword([]byte(*input.Password), bcrypt.DefaultCost)
		if err != nil {
			return nil, fmt.Errorf("Password hashing failed")
		}
		user.Password = string(hashedPassword)
	}

	if err := db.Save(&user).Error; err != nil {
		return nil, err
	}

	resp := user.ToResponse()
	return &resp, nil
}

func (UserService) Delete(db *gorm.DB, id uuid.UUID) error {
	result := db.Delete(&models.User{}, "id = ?", id)
	if result.RowsAffected == 0 {
		return fmt.Errorf("User with id %s not found", id)
	}
	return nil
}

func sendEmail(cfg *config.Config, to, code string) error {
	htmlBody := fmt.Sprintf(`
	<div style="font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; max-width: 600px; margin: 0 auto; background-color: #ffffff; border-radius: 12px; overflow: hidden; box-shadow: 0 4px 12px rgba(0,0,0,0.1);">
		<div style="background-color: #4A90E2; height: 8px; width: 100%%;"></div>
		<div style="padding: 40px 30px; text-align: center;">
			<h2 style="color: #333333; margin-top: 0; font-weight: 600;">👋你为什么会找到这里？</h2>
			<div style="background-color: #F4F7F6; border: 2px dashed #4A90E2; border-radius: 8px; padding: 20px; margin: 30px 0; display: inline-block;">
				<span style="font-size: 36px; letter-spacing: 4px; color: #4A90E2; font-weight: bold; font-family: monospace;">%s</span>
			</div>
			<p style="color: #666666; font-size: 16px; line-height: 1.6; margin: 0;">
				不管怎么说，这串代码 <strong>5分钟</strong> 后就会消失的！<br>
			</p>
			<div style="text-align: center; margin-top: 20px;">
				<img src="https://gt.yelob.vip/static/noya.jpg" alt="" style="max-width: 100%%; height: auto; border-radius: 8px;" />
			</div>
		</div>
		<div style="background-color: #F9F9F9; padding: 20px; text-align: center; border-top: 1px solid #eeeeee;">
			<p style="color: #999999; font-size: 12px; margin: 0;">
				如果不是你操作的，那就是我误操作了QWQ
			</p>
		</div>
	</div>
	`, code)

	addr := fmt.Sprintf("%s:%d", cfg.SMTPHost, cfg.SMTPPort)
	auth := smtp.PlainAuth("", cfg.SMTPUser, cfg.SMTPPassword, cfg.SMTPHost)

	msg := fmt.Sprintf("From: %s\r\nTo: %s\r\nSubject: Your Verification Code\r\nMIME-version: 1.0;\r\nContent-Type: text/html; charset=\"UTF-8\";\r\n\r\n%s", cfg.SMTPUser, to, htmlBody)

	if cfg.SMTPPort == 465 {
		tlsConfig := &tls.Config{ServerName: cfg.SMTPHost}
		conn, err := tls.Dial("tcp", addr, tlsConfig)
		if err != nil {
			return fmt.Errorf("TLS dial error: %v", err)
		}
		client, err := smtp.NewClient(conn, cfg.SMTPHost)
		if err != nil {
			return fmt.Errorf("SMTP client error: %v", err)
		}
		defer client.Close()

		if err := client.Auth(auth); err != nil {
			return fmt.Errorf("SMTP auth error: %v", err)
		}
		if err := client.Mail(cfg.SMTPUser); err != nil {
			return fmt.Errorf("SMTP mail error: %v", err)
		}
		if err := client.Rcpt(to); err != nil {
			return fmt.Errorf("SMTP rcpt error: %v", err)
		}
		w, err := client.Data()
		if err != nil {
			return fmt.Errorf("SMTP data error: %v", err)
		}
		if _, err := w.Write([]byte(msg)); err != nil {
			return fmt.Errorf("SMTP write error: %v", err)
		}
		if err := w.Close(); err != nil {
			return fmt.Errorf("SMTP close error: %v", err)
		}
		return client.Quit()
	}

	return smtp.SendMail(addr, auth, cfg.SMTPUser, []string{to}, []byte(msg))
}
