FROM golang:1.26.2-alpine AS builder

RUN apk add --no-cache git ca-certificates

WORKDIR /build

ENV GOPROXY=https://goproxy.cn,direct

COPY . .

RUN go mod init backend-go \
    && go mod tidy \
    && CGO_ENABLED=0 go build -o backend-go .

FROM alpine:3.21

RUN apk add --no-cache ca-certificates tzdata

WORKDIR /app

COPY --from=builder /build/backend-go /app/backend-go
RUN mkdir -p /app/asset/images /app/asset/videos

RUN chmod +x /app/backend-go

EXPOSE 3000 9000

CMD ["./backend-go"]