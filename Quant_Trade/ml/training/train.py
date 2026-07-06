"""
Training script for the HFT market-maker ML model.

Run from project root:
    PYTHONPATH=. python ml/training/train.py

Artifacts written to artifacts/:
    model.pkl       — trained XGBoost classifier
    pipe.pkl        — fitted BatchFeaturePipeline
    metadata.json   — feature names, horizon, data range, model version

If no Parquet data is found in data_dir, synthetic data is generated
automatically so the entire pipeline can be smoke-tested without real data.
"""

import json
import logging
import os
from datetime import datetime

import joblib
import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.metrics import precision_score, roc_auc_score
from sklearn.model_selection import TimeSeriesSplit

from ml.feature_engineering.features import BatchFeaturePipeline, FEATURE_NAMES

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    datefmt="%Y-%m-%dT%H:%M:%S",
)
logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_data(data_dir: str) -> pd.DataFrame:
    """
    Concatenate all Parquet files found recursively under data_dir.

    Raises FileNotFoundError if data_dir does not exist or contains no files.
    """
    if not os.path.isdir(data_dir):
        raise FileNotFoundError(f"Data directory not found: {data_dir}")

    files: list[str] = []
    for root, _, fnames in os.walk(data_dir):
        for fname in sorted(fnames):
            if fname.endswith(".parquet"):
                files.append(os.path.join(root, fname))

    if not files:
        raise FileNotFoundError(f"No Parquet files found under {data_dir}")

    logger.info("loading %d Parquet file(s) from %s", len(files), data_dir)
    dfs = [pd.read_parquet(f) for f in files]
    df = pd.concat(dfs, ignore_index=True)
    df = df.sort_values("timestamp_ns").reset_index(drop=True)
    logger.info("loaded %d rows", len(df))
    return df


def _make_synthetic_data(n: int = 10_000) -> pd.DataFrame:
    """Generate synthetic tick data for pipeline smoke-testing."""
    logger.warning("no real data found — generating %d synthetic ticks", n)
    rng = np.random.default_rng(42)
    bid = 100.0 + np.cumsum(rng.standard_normal(n) * 0.1)
    spread = rng.uniform(0.01, 0.05, n)
    return pd.DataFrame({
        "timestamp_ns": np.arange(n, dtype=np.int64) * 1_000_000,
        "bid":          bid,
        "ask":          bid + spread,
        "bid_sz":       rng.uniform(1.0, 10.0, n),
        "ask_sz":       rng.uniform(1.0, 10.0, n),
        "last_price":   bid + spread / 2.0,
        "volume":       np.cumsum(rng.uniform(0.1, 1.0, n)),
        "sequence":     np.arange(n, dtype=np.int64),
        "seq_gap":      np.zeros(n, dtype=bool),
    })


# ---------------------------------------------------------------------------
# Label creation
# ---------------------------------------------------------------------------

def create_labels(
    df: pd.DataFrame,
    horizon: int = 10,
    alpha: float = 1.0,
    fees: float = 0.0,
) -> pd.Series:
    """
    Cost-adjusted binary label.

    label_t = 1  if  (mid_{t+horizon} − mid_t) > alpha × spread_t + fees
              0  otherwise

    Rows where seq_gap=True falls anywhere in the forward window are masked
    to NaN — the label spans a data hole and cannot be trusted.
    """
    if "mid" not in df.columns:
        df = df.copy()
        df["mid"] = (df["bid"] + df["ask"]) / 2.0
    if "spread" not in df.columns:
        df["spread"] = df["ask"] - df["bid"]

    future_mid = df["mid"].shift(-horizon)
    threshold = alpha * df["spread"] + fees
    label = (future_mid - df["mid"] > threshold).astype(float)

    # Mask labels where a seq_gap appears in the forward window
    if "seq_gap" in df.columns and df["seq_gap"].any():
        gap_in_forward = (
            df["seq_gap"]
            .shift(-horizon, fill_value=False)
            .rolling(window=horizon, min_periods=1)
            .max()
            .astype(bool)
        )
        label[gap_in_forward] = float("nan")

    # Last `horizon` rows have no valid future mid — mask them too
    label.iloc[-horizon:] = float("nan")

    return label


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train_model(
    data_dir: str = "ml/data/raw",
    model_dir: str = "artifacts",
    horizon: int = 10,
) -> None:
    # 1. Load data
    try:
        df = load_data(data_dir)
    except FileNotFoundError:
        df = _make_synthetic_data()

    # 2. Feature engineering
    logger.info("engineering features ...")
    pipe = BatchFeaturePipeline()
    X = pipe.fit_transform(df)

    # 3. Label creation
    logger.info("creating labels (horizon=%d) ...", horizon)
    y = create_labels(df, horizon=horizon)

    # 4. Drop rows with NaN in features OR labels
    valid = X.notna().all(axis=1) & y.notna()
    X, y = X[valid], y[valid]

    if len(X) == 0:
        raise RuntimeError("No valid rows remain after feature/label creation.")

    pos_rate = float(y.mean())
    logger.info("training set: %d samples, positive rate=%.4f", len(X), pos_rate)

    # 5. Naive baselines (establish the floor before touching the real model)
    always_zero = np.zeros(len(y))
    always_one  = np.ones(len(y))
    logger.info(
        "baseline always-zero  precision=%.4f",
        precision_score(y, always_zero, zero_division=0),
    )
    logger.info(
        "baseline always-one   precision=%.4f",
        precision_score(y, always_one, zero_division=0),
    )

    # 6. Walk-forward cross-validation
    tscv = TimeSeriesSplit(n_splits=5)
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        eval_metric="logloss",
        random_state=42,
        verbosity=0,
    )

    logger.info("walk-forward cross-validation ...")
    for fold, (train_idx, val_idx) in enumerate(tscv.split(X), start=1):
        X_tr, y_tr = X.iloc[train_idx], y.iloc[train_idx]
        X_val, y_val = X.iloc[val_idx], y.iloc[val_idx]

        model.fit(X_tr, y_tr, eval_set=[(X_val, y_val)], verbose=False)
        proba = model.predict_proba(X_val)[:, 1]
        auc = roc_auc_score(y_val, proba)
        prec = precision_score(y_val, (proba > 0.5).astype(int), zero_division=0)
        logger.info("fold %d  AUC-ROC=%.4f  precision@0.5=%.4f", fold, auc, prec)

    # 7. Retrain on full dataset
    logger.info("retraining on full dataset ...")
    model.fit(X, y)

    # 8. Save artifacts (model + pipe + metadata must always be saved together)
    os.makedirs(model_dir, exist_ok=True)

    model_path = os.path.join(model_dir, "model.pkl")
    pipe_path  = os.path.join(model_dir, "pipe.pkl")
    meta_path  = os.path.join(model_dir, "metadata.json")

    joblib.dump(model, model_path)
    joblib.dump(pipe, pipe_path)

    metadata = {
        "features":      FEATURE_NAMES,
        "horizon":       horizon,
        "train_start":   str(df["timestamp_ns"].iloc[0]),
        "train_end":     str(df["timestamp_ns"].iloc[-1]),
        "model_version": "v1",
        "timestamp":     datetime.now().isoformat(),
    }
    with open(meta_path, "w") as fh:
        json.dump(metadata, fh, indent=4)

    logger.info("artifacts saved to %s  (model, pipe, metadata)", model_dir)


if __name__ == "__main__":
    train_model()
