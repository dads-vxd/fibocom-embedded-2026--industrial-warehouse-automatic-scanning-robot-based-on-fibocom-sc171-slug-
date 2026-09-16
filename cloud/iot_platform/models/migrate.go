package models

import (
	"gorm.io/gorm"
)

func AutoMigrate(db *gorm.DB) error {
	if err := db.AutoMigrate(&User{}, &Category{}, &Barcode{}, &Video{}); err != nil {
		return err
	}
	seedCategories(db)
	return nil
}

func seedCategories(db *gorm.DB) {
	var count int64
	db.Model(&Category{}).Count(&count)
	if count > 0 {
		return
	}

	categories := []struct {
		Name   string
		Fields []FieldDef
	}{
		{
			Name: "七匹狼(纯雅)条盒(二维1)",
			Fields: []FieldDef{
				{Key: "quantity", Label: "数量"},
				{Key: "auxiliary_code", Label: "辅料代码"},
				{Key: "company", Label: "公司"},
				{Key: "model", Label: "型号"},
				{Key: "production_date", Label: "生产日期"},
			},
		},
		{
			Name: "七匹狼(纯雅)小盒(二维1)",
			Fields: []FieldDef{
				{Key: "quantity", Label: "数量"},
				{Key: "auxiliary_code", Label: "辅料代码"},
				{Key: "company", Label: "公司"},
				{Key: "model", Label: "型号"},
				{Key: "production_date", Label: "生产日期"},
			},
		},
		{
			Name: "七匹狼(纯雅)内衬纸-1",
			Fields: []FieldDef{
				{Key: "weight", Label: "重量(公斤)"},
				{Key: "auxiliary_code", Label: "辅料代码"},
				{Key: "company", Label: "公司"},
				{Key: "joint", Label: "接头"},
				{Key: "production_date", Label: "生产日期"},
			},
		},
		{
			Name: "七匹狼(纯雅)框架纸(厦门)",
			Fields: []FieldDef{
				{Key: "weight", Label: "重量(公斤)"},
				{Key: "auxiliary_code", Label: "辅料代码"},
				{Key: "company", Label: "公司"},
				{Key: "joint", Label: "接头"},
				{Key: "production_date", Label: "生产日期"},
			},
		},
	}

	for _, c := range categories {
		db.Create(&Category{
			Name:      c.Name,
			FieldDefs: FieldDefs(c.Fields),
		})
	}
}
