package services

import (
	"fmt"

	"backend-go/models"

	"gorm.io/gorm"
)

type CategoryService struct{}

func (CategoryService) Create(db *gorm.DB, input models.CreateCategoryInput) (*models.Category, error) {
	category := models.Category{
		Name:      input.Name,
		FieldDefs: models.FieldDefs(input.FieldDefs),
	}
	if err := db.Create(&category).Error; err != nil {
		return nil, err
	}
	return &category, nil
}

func (CategoryService) FindAll(db *gorm.DB) ([]models.Category, error) {
	var categories []models.Category
	if err := db.Order("id ASC").Find(&categories).Error; err != nil {
		return nil, err
	}
	return categories, nil
}

func (CategoryService) FindByPage(db *gorm.DB, page, pageSize int64) (*models.PaginatedResponse, error) {
	var total int64
	db.Model(&models.Category{}).Count(&total)

	var categories []models.Category
	offset := (page - 1) * pageSize
	if err := db.Order("id ASC").Limit(int(pageSize)).Offset(int(offset)).Find(&categories).Error; err != nil {
		return nil, err
	}

	resp := models.NewPaginatedResponse(categories, total, page, pageSize)
	return &resp, nil
}

func (CategoryService) FindByID(db *gorm.DB, id int64) (*models.Category, error) {
	var category models.Category
	if err := db.First(&category, id).Error; err != nil {
		return nil, fmt.Errorf("Category with id %d not found", id)
	}
	return &category, nil
}

func (CategoryService) Update(db *gorm.DB, id int64, input models.UpdateCategoryInput) (*models.Category, error) {
	var category models.Category
	if err := db.First(&category, id).Error; err != nil {
		return nil, fmt.Errorf("Category with id %d not found", id)
	}

	category.Name = input.Name
	category.FieldDefs = models.FieldDefs(input.FieldDefs)

	if err := db.Save(&category).Error; err != nil {
		return nil, err
	}
	return &category, nil
}

func (CategoryService) Delete(db *gorm.DB, id int64) error {
	result := db.Delete(&models.Category{}, id)
	if result.RowsAffected == 0 {
		return fmt.Errorf("Category with id %d not found", id)
	}
	return nil
}
