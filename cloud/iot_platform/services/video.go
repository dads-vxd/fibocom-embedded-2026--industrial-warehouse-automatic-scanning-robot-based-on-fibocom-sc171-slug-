package services

import (
	"fmt"
	"os"
	"path/filepath"

	"backend-go/models"

	"github.com/google/uuid"
	"gorm.io/gorm"
)

type VideoService struct{}

func (VideoService) Upload(db *gorm.DB, filename string, fileBytes []byte, duration int) (*models.Video, error) {
	ext := filepath.Ext(filename)
	if ext == "" {
		ext = ".mp4"
	}
	newFilename := uuid.New().String() + ext

	if err := os.MkdirAll("asset/videos", 0755); err != nil {
		return nil, fmt.Errorf("failed to create videos directory: %w", err)
	}

	dst := filepath.Join("asset/videos", newFilename)
	if err := os.WriteFile(dst, fileBytes, 0644); err != nil {
		return nil, fmt.Errorf("failed to save video: %w", err)
	}

	video := models.Video{
		Filename: newFilename,
		Duration: duration,
		Size:     int64(len(fileBytes)),
	}

	if err := db.Create(&video).Error; err != nil {
		os.Remove(dst)
		return nil, err
	}

	return &video, nil
}

func (VideoService) FindByPage(db *gorm.DB, page, pageSize int64) (*models.PaginatedResponse, error) {
	var total int64
	db.Model(&models.Video{}).Count(&total)

	var videos []models.Video
	offset := (page - 1) * pageSize
	if err := db.Order("created_at DESC").Limit(int(pageSize)).Offset(int(offset)).Find(&videos).Error; err != nil {
		return nil, err
	}

	resp := models.NewPaginatedResponse(videos, total, page, pageSize)
	return &resp, nil
}

func (VideoService) FindByID(db *gorm.DB, id int64) (*models.Video, error) {
	var video models.Video
	if err := db.First(&video, id).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("video with id %d not found", id)
		}
		return nil, err
	}
	return &video, nil
}

func (VideoService) Delete(db *gorm.DB, id int64) error {
	var video models.Video
	if err := db.First(&video, id).Error; err != nil {
		return fmt.Errorf("video with id %d not found", id)
	}

	filePath := filepath.Join("asset/videos", video.Filename)
	os.Remove(filePath)

	return db.Delete(&video).Error
}
