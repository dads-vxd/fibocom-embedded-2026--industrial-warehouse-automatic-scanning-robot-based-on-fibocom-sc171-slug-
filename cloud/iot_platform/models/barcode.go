package models

import (
	"database/sql/driver"
	"encoding/json"
	"time"
)

type FieldValues map[string]string

func (f *FieldValues) Scan(value interface{}) error {
	if value == nil {
		*f = FieldValues{}
		return nil
	}
	return json.Unmarshal(value.([]byte), f)
}

func (f FieldValues) Value() (driver.Value, error) {
	if f == nil {
		return json.Marshal(FieldValues{})
	}
	return json.Marshal(f)
}

type Barcode struct {
	ID          int64       `gorm:"primaryKey;autoIncrement" json:"id"`
	Data        string      `gorm:"not null" json:"data"`
	Image       string      `json:"image"`
	CategoryID  int64       `gorm:"not null;index" json:"category_id"`
	FieldValues FieldValues `gorm:"type:jsonb" json:"field_values"`
	CreatedAt   time.Time   `gorm:"autoCreateTime" json:"created_at"`
	UpdatedAt   time.Time   `gorm:"autoUpdateTime" json:"updated_at"`
}

type GroupCountRow struct {
	Name  string `json:"name"`
	Count int64  `json:"count"`
}
