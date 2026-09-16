package handlers

import (
	"net/http"
	"strconv"

	"backend-go/models"
	"backend-go/services"

	"github.com/gin-gonic/gin"
)

type BarcodeHandler struct {
	Service services.BarcodeService
}

func (h *BarcodeHandler) Create(c *gin.Context) {
	data := c.PostForm("data")
	if data == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Missing field: data"})
		return
	}

	categoryIDStr := c.PostForm("category_id")
	if categoryIDStr == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Missing field: category_id"})
		return
	}
	categoryID, err := strconv.ParseInt(categoryIDStr, 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid category_id"})
		return
	}

	var imageURL string
	file, err := c.FormFile("image")
	if err == nil {
		fileBytes, _ := file.Open()
		defer fileBytes.Close()
		buf := make([]byte, file.Size)
		fileBytes.Read(buf)
		imageURL, _ = services.SaveImage(file.Filename, buf)
	}

	fieldValues := models.FieldValues{}
	for key, values := range c.Request.PostForm {
		if key == "data" || key == "category_id" || key == "image" {
			continue
		}
		if len(values) > 0 && values[0] != "" {
			fieldValues[key] = values[0]
		}
	}

	barcode, err := h.Service.Create(getDB(c), data, imageURL, categoryID, fieldValues)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, barcode)
}

func (h *BarcodeHandler) GetByPage(c *gin.Context) {
	page, _ := strconv.ParseInt(c.DefaultQuery("page", "1"), 10, 64)
	pageSize, _ := strconv.ParseInt(c.DefaultQuery("page_size", "10"), 10, 64)
	categoryIDStr := c.Query("category_id")

	if categoryIDStr == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "category_id is required"})
		return
	}
	categoryID, err := strconv.ParseInt(categoryIDStr, 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid category_id"})
		return
	}

	if page < 1 {
		page = 1
	}
	if pageSize < 1 {
		pageSize = 10
	}

	fieldFilters := map[string]string{}
	for key, values := range c.Request.URL.Query() {
		if key == "page" || key == "page_size" || key == "category_id" {
			continue
		}
		if len(values) > 0 && values[0] != "" {
			fieldFilters[key] = values[0]
		}
	}

	resp, err := h.Service.FindByPage(getDB(c), page, pageSize, categoryID, fieldFilters)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, resp)
}

func (h *BarcodeHandler) GetByID(c *gin.Context) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid barcode ID"})
		return
	}

	barcode, err := h.Service.FindByID(getDB(c), id)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, barcode)
}

func (h *BarcodeHandler) Update(c *gin.Context) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid barcode ID"})
		return
	}

	data := c.PostForm("data")
	if data == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Missing field: data"})
		return
	}

	var imageURL string
	file, err := c.FormFile("image")
	if err == nil {
		fileBytes, _ := file.Open()
		defer fileBytes.Close()
		buf := make([]byte, file.Size)
		fileBytes.Read(buf)
		imageURL, _ = services.SaveImage(file.Filename, buf)
	}

	fieldValues := models.FieldValues{}
	for key, values := range c.Request.PostForm {
		if key == "data" || key == "image" {
			continue
		}
		if len(values) > 0 && values[0] != "" {
			fieldValues[key] = values[0]
		}
	}

	barcode, err := h.Service.Update(getDB(c), id, data, imageURL, fieldValues)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, barcode)
}

func (h *BarcodeHandler) Delete(c *gin.Context) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid barcode ID"})
		return
	}

	if err := h.Service.Delete(getDB(c), id); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Barcode deleted successfully"})
}

func (h *BarcodeHandler) StatsByCategory(c *gin.Context) {
	rows, err := h.Service.StatsByCategory(getDB(c))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, rows)
}
