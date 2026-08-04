package http

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	mlpkg "github.com/anshul/hft/backend/internal/ml"
	mdsvc "github.com/anshul/hft/backend/internal/service/marketdata"
	"github.com/anshul/hft/backend/internal/websocket"
	"go.uber.org/zap"
)

type Server struct {
	port     int
	svc      *mdsvc.Service
	mlClient *mlpkg.Client
	mlHub    *mlpkg.PredictionHub
	logger   *zap.Logger
	httpSrv  *http.Server
}

func NewServer(port int, svc *mdsvc.Service, mlClient *mlpkg.Client, mlHub *mlpkg.PredictionHub, logger *zap.Logger) *Server {
	return &Server{
		port:     port,
		svc:      svc,
		mlClient: mlClient,
		mlHub:    mlHub,
		logger:   logger,
	}
}

func enableCORS(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")

		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		}
		next(w, r)
	}
}

func (s *Server) Start() {
	mux := http.NewServeMux()

	gw := websocket.NewGateway(s.svc, s.mlClient, s.mlHub, s.logger)
	mux.HandleFunc("/ws/market-data", gw.HandleMarketData)
	mux.HandleFunc("/ws/trades", gw.HandleTrades)
	mux.HandleFunc("/ws/ml-predictions", gw.HandleMLPredictions)

	mux.HandleFunc("/health", enableCORS(s.handleHealth))
	mux.HandleFunc("/ready", enableCORS(s.handleReady))
	mux.HandleFunc("/api/benchmarks", enableCORS(s.handleBenchmarks))

	s.httpSrv = &http.Server{
		Addr:              fmt.Sprintf(":%d", s.port),
		Handler:           mux,
		ReadHeaderTimeout: 10 * time.Second,
	}

	s.logger.Info("HTTP gateway started",
		zap.Int("port", s.port),
		zap.Strings("routes", []string{"/ws/market-data", "/ws/trades", "/ws/ml-predictions", "/health", "/ready", "/api/benchmarks"}),
	)

	if err := s.httpSrv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		s.logger.Error("HTTP server error", zap.Error(err))
	}
}

func (s *Server) Stop(ctx context.Context) {
	if s.httpSrv == nil {
		return
	}
	if err := s.httpSrv.Shutdown(ctx); err != nil {
		s.logger.Error("HTTP server shutdown error", zap.Error(err))
	}
	s.logger.Info("HTTP gateway stopped")
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(s.svc.Health())
}

func (s *Server) handleReady(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("ok"))
}

func (s *Server) handleBenchmarks(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	searchPaths := []string{
		"core-cpp/results",
		"../core-cpp/results",
		"../../core-cpp/results",
	}

	var latestFile string
	var latestModTime time.Time

	for _, dir := range searchPaths {
		entries, err := os.ReadDir(dir)
		if err != nil {
			continue
		}
		for _, entry := range entries {
			if !entry.IsDir() && strings.HasPrefix(entry.Name(), "benchmark_") && strings.HasSuffix(entry.Name(), ".json") {
				info, err := entry.Info()
				if err != nil {
					continue
				}
				if info.ModTime().After(latestModTime) {
					latestModTime = info.ModTime()
					latestFile = filepath.Join(dir, entry.Name())
				}
			}
		}
	}

	if latestFile == "" {
		_ = json.NewEncoder(w).Encode(map[string]interface{}{
			"status":     "no_benchmarks_found",
			"benchmarks": map[string]interface{}{},
		})
		return
	}

	data, err := os.ReadFile(latestFile)
	if err != nil {
		s.logger.Error("Failed to read benchmark file", zap.String("file", latestFile), zap.Error(err))
		http.Error(w, `{"error":"failed to read benchmark file"}`, http.StatusInternalServerError)
		return
	}

	_, _ = w.Write(data)
}