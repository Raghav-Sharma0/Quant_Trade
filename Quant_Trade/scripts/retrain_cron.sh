#!/bin/bash
# =============================================================================
# retrain_cron.sh — called by cron to retrain the HFT ML model on fresh data.
#
# Cron example (daily at 02:00, project root assumed to be in $PROJECT_ROOT):
#   0 2 * * * /bin/bash /path/to/Quant_Trade/scripts/retrain_cron.sh >> /var/log/hft_retrain.log 2>&1
#
# Exit codes (forwarded from retrain.py):
#   0 — challenger promoted successfully
#   1 — hard error (check logs)
#   2 — insufficient data (< min_rows); skipped without error
#   3 — challenger trained but did not beat champion; skipped
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration — override via environment variables if needed
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DATA_DIR="${ML_DATA_DIR:-ml/data/raw}"
MODEL_DIR="${ML_MODEL_DIR:-artifacts}"
HORIZON="${ML_HORIZON:-10}"
MIN_ROWS="${ML_MIN_ROWS:-5000}"
LOG_DIR="${LOG_DIR:-logs}"

# Strip Anaconda from PATH so system Python/pip is used, not conda's.
export PATH="$(echo "$PATH" | tr ':' '\n' | grep -v "anaconda" | tr '\n' ':')"

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
cd "$PROJECT_ROOT"
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +"%Y-%m-%dT%H:%M:%S")
LOG_FILE="$LOG_DIR/retrain.log"

echo "[$TIMESTAMP] === retrain_cron.sh starting ===" | tee -a "$LOG_FILE"
echo "[$TIMESTAMP] project_root=$PROJECT_ROOT  data_dir=$DATA_DIR  model_dir=$MODEL_DIR" | tee -a "$LOG_FILE"

# ---------------------------------------------------------------------------
# Detect Python command
# ---------------------------------------------------------------------------
if command -v python3 >/dev/null 2>&1; then
    PYTHON_CMD="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
else
    echo "[$TIMESTAMP] ERROR: no python3 or python found in PATH" | tee -a "$LOG_FILE"
    exit 1
fi

echo "[$TIMESTAMP] using Python: $($PYTHON_CMD --version 2>&1)" | tee -a "$LOG_FILE"

# ---------------------------------------------------------------------------
# Run retrain
# ---------------------------------------------------------------------------
set +e   # don't exit on non-zero so we can handle exit codes explicitly
PYTHONPATH=. $PYTHON_CMD -m ml.training.retrain \
    --data-dir  "$DATA_DIR"  \
    --model-dir "$MODEL_DIR" \
    --horizon   "$HORIZON"   \
    --min-rows  "$MIN_ROWS"  \
    2>&1 | tee -a "$LOG_FILE"
RETRAIN_EXIT=${PIPESTATUS[0]}
set -e

TIMESTAMP=$(date +"%Y-%m-%dT%H:%M:%S")

case "$RETRAIN_EXIT" in
    0)
        echo "[$TIMESTAMP] ✅ retrain complete — new model promoted to $MODEL_DIR" | tee -a "$LOG_FILE"
        ;;
    2)
        echo "[$TIMESTAMP] ⚠️  insufficient data — retrain skipped (not an error)" | tee -a "$LOG_FILE"
        ;;
    3)
        echo "[$TIMESTAMP] ℹ️  challenger did not beat champion — model unchanged" | tee -a "$LOG_FILE"
        ;;
    *)
        echo "[$TIMESTAMP] ❌ retrain failed with exit code $RETRAIN_EXIT" | tee -a "$LOG_FILE"
        exit "$RETRAIN_EXIT"
        ;;
esac

echo "[$TIMESTAMP] === retrain_cron.sh done (exit=$RETRAIN_EXIT) ===" | tee -a "$LOG_FILE"
exit 0
