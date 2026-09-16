package models

type PaginatedResponse struct {
	Data       interface{} `json:"data"`
	Total      int64       `json:"total"`
	Page       int64       `json:"page"`
	PageSize   int64       `json:"page_size"`
	TotalPages int64       `json:"total_pages"`
}

func NewPaginatedResponse(data interface{}, total, page, pageSize int64) PaginatedResponse {
	totalPages := (total + pageSize - 1) / pageSize
	return PaginatedResponse{
		Data:       data,
		Total:      total,
		Page:       page,
		PageSize:   pageSize,
		TotalPages: totalPages,
	}
}
