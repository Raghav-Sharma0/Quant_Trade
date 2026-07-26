#!/bin/bash
# retrain_cron.sh — retrains the HFT ML model on fresh parquet data.
#
# Usage (crontab): 0 2 * * * /bin/bash /path/to/scripts/retrain_cron.sh >> /var/log/hft_retrain.log 2>&1
#
# Exit codes (from retrain.py):
#   0 = challenger promoted   1 = hard error   2 = not enough data   3 = challenger lost

set -euo pipefail

# paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# overridable defaults
DATA_DIR="${ML_DATA_DIR:-ml/data/raw}"
MODEL_DIR="${ML_MODEL_DIR:-artifacts}"
HORIZON="${ML_HORIZON:-10}"
MIN_ROWS="${ML_MIN_ROWS:-5000}"
LOG_DIR="${LOG_DIR:-logs}"

# use system python, not conda
export PATH="$(echo "$PATH" | tr ':' '\n' | grep -v "anaconda" | tr '\n' ':')"

cd "$PROJECT_ROOT"
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +"%Y-%m-%dT%H:%M:%S")
LOG_FILE="$LOG_DIR/retrain.log"

echo "[$TIMESTAMP] retrain_cron.sh starting | data=$DATA_DIR model=$MODEL_DIR" | tee -a "$LOG_FILE"

# detect python
if command -v python3 >/dev/null 2>&1; then
    PYTHON_CMD="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
else
    echo "[$TIMESTAMP] ERROR: python not found" | tee -a "$LOG_FILE"
    exit 1
fi

echo "[$TIMESTAMP] python: $($PYTHON_CMD --version 2>&1)" | tee -a "$LOG_FILE"

# run retrain, capture exit code without crashing the script
set +e
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
    0) echo "[$TIMESTAMP] retrain OK — new model promoted to $MODEL_DIR" | tee -a "$LOG_FILE" ;;
    2) echo "[$TIMESTAMP] skipped — not enough data (< $MIN_ROWS rows)" | tee -a "$LOG_FILE" ;;
    3) echo "[$TIMESTAMP] skipped — challenger did not beat champion" | tee -a "$LOG_FILE" ;;
    *) echo "[$TIMESTAMP] FAILED with exit=$RETRAIN_EXIT" | tee -a "$LOG_FILE"; exit "$RETRAIN_EXIT" ;;
esac

echo "[$TIMESTAMP] retrain_cron.sh done (exit=$RETRAIN_EXIT)" | tee -a "$LOG_FILE"
exit 0
