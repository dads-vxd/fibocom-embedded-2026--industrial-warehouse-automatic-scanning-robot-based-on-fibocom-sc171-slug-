package services

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"backend-go/models"

	"github.com/google/uuid"
	"gorm.io/gorm"
)

type BarcodeService struct{}

func (BarcodeService) Create(db *gorm.DB, data string, imageURL string, categoryID int64, fieldValues models.FieldValues) (*models.Barcode, error) {
	barcode := models.Barcode{
		Data:        data,
		Image:       imageURL,
		CategoryID:  categoryID,
		FieldValues: fieldValues,
	}
	if err := db.Create(&barcode).Error; err != nil {
		return nil, err
	}
	return &barcode, nil
}

func (BarcodeService) FindByPage(db *gorm.DB, page, pageSize, categoryID int64, fieldFilters map[string]string) (*models.PaginatedResponse, error) {
	query := db.Model(&models.Barcode{}).Where("category_id = ?", categoryID)

	for key, value := range fieldFilters {
		if isValidFieldKey(key) {
			query = query.Where("field_values->>? LIKE ?", key, "%"+value+"%")
		}
	}

	var total int64
	query.Count(&total)

	var barcodes []models.Barcode
	offset := (page - 1) * pageSize
	if err := query.Order("created_at DESC").Limit(int(pageSize)).Offset(int(offset)).Find(&barcodes).Error; err != nil {
		return nil, err
	}

	resp := models.NewPaginatedResponse(barcodes, total, page, pageSize)
	return &resp, nil
}

func (BarcodeService) FindByID(db *gorm.DB, id int64) (*models.Barcode, error) {
	var barcode models.Barcode
	if err := db.First(&barcode, id).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("Barcode with id %d not found", id)
		}
		return nil, err
	}
	return &barcode, nil
}

func (BarcodeService) Update(db *gorm.DB, id int64, data string, imageURL string, fieldValues models.FieldValues) (*models.Barcode, error) {
	var barcode models.Barcode
	if err := db.First(&barcode, id).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("Barcode with id %d not found", id)
		}
		return nil, err
	}

	barcode.Data = data
	if imageURL != "" {
		barcode.Image = imageURL
	}
	barcode.FieldValues = fieldValues

	if err := db.Save(&barcode).Error; err != nil {
		return nil, err
	}
	return &barcode, nil
}

func (BarcodeService) Delete(db *gorm.DB, id int64) error {
	result := db.Delete(&models.Barcode{}, id)
	if result.RowsAffected == 0 {
		return fmt.Errorf("Barcode with id %d not found", id)
	}
	return nil
}

func (BarcodeService) StatsByCategory(db *gorm.DB) ([]models.GroupCountRow, error) {
	var rows []models.GroupCountRow
	if err := db.Raw(`
		SELECT c.name, COUNT(*) as count
		FROM barcodes b
		JOIN categories c ON b.category_id = c.id
		GROUP BY c.name
		ORDER BY count DESC
	`).Scan(&rows).Error; err != nil {
		return nil, err
	}
	return rows, nil
}

func SaveImage(filename string, fileBytes []byte) (string, error) {
	ext := strings.ToLower(filepath.Ext(filename))
	if ext == "" {
		ext = ".png"
	}
	newFilename := uuid.New().String() + ext
	dst := filepath.Join("asset/images", newFilename)
	if err := writeFile(dst, fileBytes); err != nil {
		return "", err
	}
	return "/images/" + newFilename, nil
}

func writeFile(path string, data []byte) error {
	return os.WriteFile(path, data, 0644)
}

func isValidFieldKey(key string) bool {
	for _, c := range key {
		if !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
			return false
		}
	}
	return len(key) > 0
}
