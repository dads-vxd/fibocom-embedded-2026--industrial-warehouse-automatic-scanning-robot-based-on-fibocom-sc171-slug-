package models

import (
	"database/sql/driver"
	"encoding/json"
	"time"
)

type FieldDef struct {
	Key   string `json:"key"`
	Label string `json:"label"`
}

type FieldDefs []FieldDef

func (f *FieldDefs) Scan(value interface{}) error {
	if value == nil {
		*f = FieldDefs{}
		return nil
	}
	return json.Unmarshal(value.([]byte), f)
}

func (f FieldDefs) Value() (driver.Value, error) {
	if f == nil {
		return json.Marshal([]FieldDef{})
	}
	return json.Marshal(f)
}

type Category struct {
	ID        int64     `gorm:"primaryKey;autoIncrement" json:"id"`
	Name      string    `gorm:"uniqueIndex;size:255;not null" json:"name"`
	FieldDefs FieldDefs `gorm:"type:jsonb" json:"field_defs"`
	CreatedAt time.Time `gorm:"autoCreateTime" json:"created_at"`
	UpdatedAt time.Time `gorm:"autoUpdateTime" json:"updated_at"`
}

type CreateCategoryInput struct {
	Name      string     `json:"name" binding:"required"`
	FieldDefs []FieldDef `json:"field_defs"`
}

type UpdateCategoryInput struct {
	Name      string     `json:"name" binding:"required"`
	FieldDefs []FieldDef `json:"field_defs"`
}
