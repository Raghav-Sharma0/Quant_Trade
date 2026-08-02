# Multistage build for Go Ingestion backend
FROM golang:1.23-alpine AS builder

WORKDIR /app

# Install git and certificates
RUN apk add --no-cache git ca-certificates

# Copy go module files from backend-go
COPY backend-go/go.mod backend-go/go.sum ./
RUN go mod download

# Copy backend-go source code
COPY backend-go/ .

# Build the server binary
RUN CGO_ENABLED=0 GOOS=linux go build -o server ./cmd/server/main.go

# Final slim stage
FROM alpine:latest

WORKDIR /app

# Copy ca-certificates for any SSL dependencies
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/server .

# Copy default configs from root
COPY configs/ ./configs/

EXPOSE 8081 9090

ENTRYPOINT ["./server"]
CMD ["--config", "configs/prod.yaml"]
