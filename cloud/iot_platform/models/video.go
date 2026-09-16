package models

import "time"

type Video struct {
	ID        int64     `gorm:"primaryKey;autoIncrement" json:"id"`
	Filename  string    `gorm:"not null;uniqueIndex" json:"filename"`
	Duration  int       `json:"duration"`
	Size      int64     `json:"size"`
	CreatedAt time.Time `gorm:"autoCreateTime" json:"created_at"`
	UpdatedAt time.Time `gorm:"autoUpdateTime" json:"updated_at"`
}

func (v *Video) URL() string {
	return "/video-files/" + v.Filename
}
