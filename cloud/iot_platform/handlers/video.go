package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"backend-go/models"
	"backend-go/services"

	"github.com/gin-gonic/gin"
)

type VideoHandler struct {
	Service services.VideoService
}

func (h *VideoHandler) Upload(c *gin.Context) {
	file, err := c.FormFile("video")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Missing video file"})
		return
	}

	durationStr := c.DefaultPostForm("duration", "30")
	duration, _ := strconv.Atoi(durationStr)
	if duration <= 0 {
		duration = 30
	}

	fileBytes, _ := file.Open()
	defer fileBytes.Close()
	buf := make([]byte, file.Size)
	fileBytes.Read(buf)

	video, err := h.Service.Upload(getDB(c), file.Filename, buf, duration)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	tx, exists := c.Get("tx")
	if exists {
		barcodeTx := tx.(*services.BarcodeTx)
		msg := services.WSBroadcast{
			Event: "video_uploaded",
			Data:  videoWithURL(video),
		}
		msgBytes, _ := json.Marshal(msg)
		barcodeTx.Send(string(msgBytes))
	}

	c.JSON(http.StatusOK, videoWithURL(video))
}

func (h *VideoHandler) List(c *gin.Context) {
	page, _ := strconv.ParseInt(c.DefaultQuery("page", "1"), 10, 64)
	pageSize, _ := strconv.ParseInt(c.DefaultQuery("page_size", "20"), 10, 64)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 {
		pageSize = 20
	}

	resp, err := h.Service.FindByPage(getDB(c), page, pageSize)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	videos := resp.Data.([]models.Video)
	type videoResponse struct {
		models.Video
		URL string `json:"url"`
	}
	var result []videoResponse
	for _, v := range videos {
		result = append(result, videoResponse{Video: v, URL: v.URL()})
	}
	resp.Data = result

	c.JSON(http.StatusOK, resp)
}

func (h *VideoHandler) GetByID(c *gin.Context) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid video ID"})
		return
	}

	video, err := h.Service.FindByID(getDB(c), id)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, videoWithURL(video))
}

func (h *VideoHandler) Delete(c *gin.Context) {
	id, err := strconv.ParseInt(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid video ID"})
		return
	}

	if err := h.Service.Delete(getDB(c), id); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Video deleted successfully"})
}

type videoWithURLResponse struct {
	models.Video
	URL string `json:"url"`
}

func videoWithURL(v *models.Video) videoWithURLResponse {
	return videoWithURLResponse{Video: *v, URL: v.URL()}
}
