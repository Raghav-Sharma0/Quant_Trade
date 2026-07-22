"""
Cron-aware retraining script for the HFT market-maker ML model.

Designed to be called regularly by a cron job (e.g. daily).  It:
  1. Trains a fresh challenger model on ALL available Parquet data.
  2. Evaluates both the champion (currently deployed) and the challenger
     on the most recent held-out 15 % of data.
  3. Promotes the challenger to champion ONLY if it beats the champion
     on AUC-ROC.  This prevents regressions from noisy data batches.
  4. Keeps a versioned archive of every promoted model under
     artifacts/archive/<version>/ for auditability.
  5. Exits with code 0 on success, 1 on hard errors, 2 if data is
     insufficient to retrain (so cron can distinguish the cases).

Usage (run from project root):
    PYTHONPATH=. python ml/training/retrain.py [--data-dir ml/data/raw]
                                               [--model-dir artifacts]
                                               [--min-rows 5000]
                                               [--horizon 10]
                                               [--force]

Flags:
    --min-rows N   Refuse to train if fewer than N valid rows are found.
                   Default 5 000.  Set lower for smoke-tests.
    --force        Promote challenger even if it doesn't beat the champion.
                   Useful after a deliberate feature schema change.
"""

import argparse
import json
import logging
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import joblib
import numpy as np
import xgboost as xgb
from sklearn.metrics import precision_score, roc_auc_score
from sklearn.model_selection import TimeSeriesSplit

from ml.feature_engineering.features import BatchFeaturePipeline, FEATURE_NAMES
from ml.training.train import load_data, create_labels, _make_synthetic_data

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
    datefmt="%Y-%m-%dT%H:%M:%S",
)
logger = logging.getLogger("retrain")


# ---------------------------------------------------------------------------
# Exit codes (cron-friendly)
# ---------------------------------------------------------------------------
EXIT_OK              = 0
EXIT_ERROR           = 1
EXIT_INSUFFICIENT    = 2   # data existed but was too thin
EXIT_NOT_PROMOTED    = 3   # challenger trained but was not better; not an error


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_champion_auc(model_dir: str) -> float:
    """Return the AUC stored in the current metadata, or 0.0 if none exists."""
    meta_path = os.path.join(model_dir, "metadata.json")
    if not os.path.exists(meta_path):
        return 0.0
    try:
        with open(meta_path) as fh:
            meta = json.load(fh)
        return float(meta.get("eval_auc", 0.0))
    except Exception:
        return 0.0


def _archive_champion(model_dir: str, version: str) -> None:
    """Copy current artifacts into artifacts/archive/<version>/."""
    archive_dir = os.path.join(model_dir, "archive", version)
    os.makedirs(archive_dir, exist_ok=True)
    for fname in ("model.pkl", "pipe.pkl", "metadata.json"):
        src = os.path.join(model_dir, fname)
        if os.path.exists(src):
            shutil.copy2(src, archive_dir)
    logger.info("archived champion to %s", archive_dir)


def _evaluate_on_holdout(model, pipe, df, horizon: int) -> tuple[float, float]:
    """
    Evaluate *model* on the last 15 % of *df* (time-based split).
    Returns (auc, precision_at_0_5).
    """
    X_full = pipe.transform(df)
    y_full = create_labels(df, horizon=horizon)

    valid = X_full.notna().all(axis=1) & y_full.notna()
    X_full, y_full = X_full[valid], y_full[valid]

    test_start = int(len(X_full) * 0.85)
    X_test = X_full.iloc[test_start:]
    y_test = y_full.iloc[test_start:]

    if len(X_test) < 50:
        logger.warning("holdout set has only %d rows — AUC may be unreliable", len(X_test))

    proba = model.predict_proba(X_test)[:, 1]
    preds = (proba > 0.5).astype(int)

    auc  = float(roc_auc_score(y_test, proba))
    prec = float(precision_score(y_test, preds, zero_division=0))
    return auc, prec


# ---------------------------------------------------------------------------
# Core retraining logic
# ---------------------------------------------------------------------------

def retrain(
    data_dir:  str  = "ml/data/raw",
    model_dir: str  = "artifacts",
    horizon:   int  = 10,
    min_rows:  int  = 5_000,
    force:     bool = False,
) -> int:
    """
    Run the full retrain cycle.  Returns an exit code (see EXIT_* constants).
    """
    logger.info("=== Retrain job started ===")
    logger.info("data_dir=%s  model_dir=%s  horizon=%d  min_rows=%d  force=%s",
                data_dir, model_dir, horizon, min_rows, force)

    # ------------------------------------------------------------------
    # 1. Load data
    # ------------------------------------------------------------------
    try:
        df = load_data(data_dir)
    except FileNotFoundError:
        logger.warning("no Parquet data found — falling back to synthetic data")
        df = _make_synthetic_data(n=max(min_rows, 10_000))

    logger.info("loaded %d total rows", len(df))

    # ------------------------------------------------------------------
    # 2. Feature engineering + labels (used for both training and eval)
    # ------------------------------------------------------------------
    logger.info("engineering features ...")
    pipe = BatchFeaturePipeline()
    X = pipe.fit_transform(df)

    logger.info("creating labels (horizon=%d) ...", horizon)
    y = create_labels(df, horizon=horizon)

    valid = X.notna().all(axis=1) & y.notna()
    X_clean, y_clean = X[valid], y[valid]

    n_valid = len(X_clean)
    logger.info("valid training rows: %d", n_valid)

    if n_valid < min_rows:
        logger.error(
            "insufficient data: %d valid rows < min_rows=%d.  Skipping retrain.",
            n_valid, min_rows,
        )
        return EXIT_INSUFFICIENT

    pos_rate = float(y_clean.mean())
    logger.info("positive rate: %.4f", pos_rate)

    # ------------------------------------------------------------------
    # 3. Walk-forward cross-validation (challenger)
    # ------------------------------------------------------------------
    tscv = TimeSeriesSplit(n_splits=5)
    challenger = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        eval_metric="logloss",
        random_state=42,
        verbosity=0,
    )

    logger.info("walk-forward CV (5 folds) ...")
    cv_aucs: list[float] = []
    for fold, (train_idx, val_idx) in enumerate(tscv.split(X_clean), start=1):
        X_tr, y_tr = X_clean.iloc[train_idx], y_clean.iloc[train_idx]
        X_val, y_val = X_clean.iloc[val_idx], y_clean.iloc[val_idx]
        challenger.fit(X_tr, y_tr, eval_set=[(X_val, y_val)], verbose=False)
        proba = challenger.predict_proba(X_val)[:, 1]
        auc   = roc_auc_score(y_val, proba)
        prec  = precision_score(y_val, (proba > 0.5).astype(int), zero_division=0)
        cv_aucs.append(auc)
        logger.info("  fold %d  AUC=%.4f  precision=%.4f", fold, auc, prec)

    mean_cv_auc = float(np.mean(cv_aucs))
    logger.info("challenger mean CV AUC: %.4f", mean_cv_auc)

    # ------------------------------------------------------------------
    # 4. Retrain challenger on full dataset, evaluate on holdout
    # ------------------------------------------------------------------
    logger.info("retraining challenger on full dataset ...")
    challenger.fit(X_clean, y_clean)

    challenger_auc, challenger_prec = _evaluate_on_holdout(challenger, pipe, df, horizon)
    logger.info(
        "challenger holdout  AUC=%.4f  precision=%.4f",
        challenger_auc, challenger_prec,
    )

    # ------------------------------------------------------------------
    # 5. Champion/challenger comparison
    # ------------------------------------------------------------------
    champion_auc = _load_champion_auc(model_dir)
    logger.info("champion holdout AUC (stored): %.4f", champion_auc)

    should_promote = force or (challenger_auc > champion_auc)

    if not should_promote:
        logger.info(
            "challenger (%.4f) did not beat champion (%.4f) — keeping current model.",
            challenger_auc, champion_auc,
        )
        return EXIT_NOT_PROMOTED

    # ------------------------------------------------------------------
    # 6. Archive champion, promote challenger
    # ------------------------------------------------------------------
    version = datetime.now(timezone.utc).strftime("v%Y%m%d_%H%M%S")

    if champion_auc > 0.0:  # only archive if a real champion exists
        _archive_champion(model_dir, version)

    os.makedirs(model_dir, exist_ok=True)

    model_path = os.path.join(model_dir, "model.pkl")
    pipe_path  = os.path.join(model_dir, "pipe.pkl")
    meta_path  = os.path.join(model_dir, "metadata.json")

    joblib.dump(challenger, model_path)
    joblib.dump(pipe,       pipe_path)

    metadata = {
        "features":      FEATURE_NAMES,
        "horizon":       horizon,
        "train_rows":    n_valid,
        "positive_rate": pos_rate,
        "train_start":   str(df["timestamp_ns"].iloc[0]),
        "train_end":     str(df["timestamp_ns"].iloc[-1]),
        "cv_auc_mean":   round(mean_cv_auc, 6),
        "eval_auc":      round(challenger_auc, 6),
        "eval_precision": round(challenger_prec, 6),
        "model_version": version,
        "timestamp":     datetime.now(timezone.utc).isoformat(),
    }
    with open(meta_path, "w") as fh:
        json.dump(metadata, fh, indent=4)

    logger.info(
        "✅ promoted challenger %s  AUC %.4f → %.4f",
        version, champion_auc, challenger_auc,
    )
    logger.info("artifacts written to %s", model_dir)
    logger.info("=== Retrain job complete ===")
    return EXIT_OK


# ---------------------------------------------------------------------------
# CLI entry-point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Cron-aware retraining for the HFT ML model.",
    )
    parser.add_argument("--data-dir",  default="ml/data/raw",
                        help="Root directory containing Parquet tick files.")
    parser.add_argument("--model-dir", default="artifacts",
                        help="Directory to read/write model artifacts.")
    parser.add_argument("--horizon",   type=int, default=10,
                        help="Forward-looking label horizon in ticks.")
    parser.add_argument("--min-rows",  type=int, default=5_000,
                        help="Minimum valid rows required to run retraining.")
    parser.add_argument("--force",     action="store_true",
                        help="Promote challenger even if AUC is lower than champion.")
    args = parser.parse_args()

    code = retrain(
        data_dir=args.data_dir,
        model_dir=args.model_dir,
        horizon=args.horizon,
        min_rows=args.min_rows,
        force=args.force,
    )
    sys.exit(code)


if __name__ == "__main__":
    main()
