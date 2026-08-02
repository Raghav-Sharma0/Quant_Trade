@echo off
setlocal enabledelayedexpansion

echo =============================================================
echo   QuantTrade HFT Platform - Local Minikube Deploy Script
echo =============================================================
echo.

:: ---------------------------------------------------------------
:: STEP 1: Check Minikube is running
:: ---------------------------------------------------------------
echo [1/7] Checking Minikube status...
minikube status | findstr "Running" >nul 2>&1
if errorlevel 1 (
    echo     Minikube is not running. Starting it now...
    minikube start
) else (
    echo     Minikube is already running. OK
)
echo.

:: ---------------------------------------------------------------
:: STEP 2: Point Docker CLI to Minikube's Docker daemon
:: ---------------------------------------------------------------
echo [2/7] Switching Docker context to Minikube daemon...
@FOR /f "tokens=*" %%i IN ('minikube -p minikube docker-env --shell cmd') DO @%%i
echo     Docker context set to Minikube. OK
echo.

:: ---------------------------------------------------------------
:: STEP 3: Build all Docker images inside Minikube
:: ---------------------------------------------------------------
echo [3/7] Building Docker images inside Minikube...

echo     Building Go Backend...
docker build -t quant_trade/hft-backend:latest -f backend-go/Dockerfile .
if errorlevel 1 ( echo ERROR: Failed to build backend. & exit /b 1 )

echo     Building ML Predictor...
docker build -t quant_trade/ml-predictor:latest -f ml/Dockerfile .
if errorlevel 1 ( echo ERROR: Failed to build ml-predictor. & exit /b 1 )

echo     Building Frontend...
docker build -t quant_trade/hft-frontend:latest -f frontend/Dockerfile .
if errorlevel 1 ( echo ERROR: Failed to build frontend. & exit /b 1 )

echo     Building C++ Exchange Simulator...
docker build -t quant_trade/exchange-sim:latest -f exchange-sim/Dockerfile .
if errorlevel 1 ( echo ERROR: Failed to build exchange-sim. & exit /b 1 )

echo     All images built successfully. OK
echo.

:: ---------------------------------------------------------------
:: STEP 4: Apply all Kubernetes manifests
:: ---------------------------------------------------------------
echo [4/7] Applying Kubernetes manifests...
kubectl apply -f deployment/k8s/platform-services.yaml
if errorlevel 1 ( echo ERROR: Failed to apply platform-services.yaml. & exit /b 1 )

kubectl apply -f deployment/k8s/simulation-cronjob.yaml
if errorlevel 1 ( echo ERROR: Failed to apply simulation-cronjob.yaml. & exit /b 1 )

echo     Manifests applied. OK
echo.

:: ---------------------------------------------------------------
:: STEP 5: Wait for core pods to be ready
:: ---------------------------------------------------------------
echo [5/7] Waiting for core pods to become Ready...
echo     Waiting for backend...
kubectl rollout status deployment/hft-backend-deploy -n hft --timeout=120s

echo     Waiting for ML predictor...
kubectl rollout status deployment/hft-ml-predictor-deploy -n hft --timeout=120s

echo     Waiting for frontend...
kubectl rollout status deployment/hft-frontend-deploy -n hft --timeout=120s

echo     All core pods are Running. OK
echo.

:: ---------------------------------------------------------------
:: STEP 6: Start the Exchange Simulator
:: ---------------------------------------------------------------
echo [6/7] Starting Exchange Simulator...

:: Delete any leftover test pods or old jobs
kubectl delete pod hft-simulator-test -n hft --ignore-not-found >nul 2>&1
kubectl delete job hft-sim-run -n hft --ignore-not-found >nul 2>&1

:: Run the simulator as a pod directly
kubectl run hft-simulator-test ^
  --image=quant_trade/exchange-sim:latest ^
  --image-pull-policy=Never ^
  --namespace=hft ^
  --restart=Never ^
  --command -- /app/exchange-sim/build/exchange_sim ^
  --synth --duration 82800 --symbol 0 --spread 20 --noise-interval-us 100000 --ws-port 8080

echo     Simulator pod created. OK
echo.

:: ---------------------------------------------------------------
:: STEP 7: Show pod status and port-forward instructions
:: ---------------------------------------------------------------
echo [7/7] Current pod status:
echo.
kubectl get pods -n hft
echo.

echo =============================================================
echo   DEPLOYMENT COMPLETE!
echo =============================================================
echo.
echo   Run these in SEPARATE CMD windows to access the dashboard:
echo.
echo   WINDOW 1 (Frontend):
echo     kubectl port-forward svc/hft-frontend-svc -n hft 3000:3000
echo.
echo   WINDOW 2 (Backend WebSocket):
echo     kubectl port-forward svc/hft-backend-svc -n hft 8081:8081
echo.
echo   Then open: http://localhost:3000/dashboard
echo.
echo =============================================================

endlocal
