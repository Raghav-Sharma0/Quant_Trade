import os
import json
import joblib
import pandas as pd
import numpy as np
import xgboost as xgb
from datetime import datetime
from sklearn.model_selection import TimeSeriesSplit
from sklearn.metrics import roc_auc_score, precision_score
from ml.feature_engineering.features import BatchFeaturePipeline, FEATURE_NAMES

def load_data(data_dir):
    """Load and concatenate all parquet files in the given directory."""
    if not os.path.exists(data_dir):
        raise ValueError(f"Directory not found: {data_dir}")
        
    files = [os.path.join(data_dir, f) for f in os.listdir(data_dir) if f.endswith('.parquet')]
    if not files:
        raise ValueError(f"No parquet files found in {data_dir}")
    dfs = [pd.read_parquet(f) for f in sorted(files)]
    return pd.concat(dfs, ignore_index=True)

def create_labels(df, horizon=10, alpha=1.0, fees=0.0):
    """
    Create cost-adjusted binary labels.
    label = 1 if (mid_{t+N} - mid_t) > alpha * spread + fees else 0
    """
    if "mid" not in df.columns:
        df["mid"] = (df["bid"] + df["ask"]) / 2.0
    if "spread" not in df.columns:
        df["spread"] = df["ask"] - df["bid"]
        
    future_mid = df["mid"].shift(-horizon)
    price_change = future_mid - df["mid"]
    threshold = alpha * df["spread"] + fees
    
    label = (price_change > threshold).astype(int)
    
    # Mask rows near seq_gap
    if "seq_gap" in df.columns:
        # If any seq_gap is True in the forward window, the label is untrustworthy
        gap_in_forward = df["seq_gap"].shift(-horizon).rolling(window=horizon, min_periods=1).max() > 0
        label.loc[gap_in_forward] = np.nan
        
    return label

def train_model(data_dir="data/", model_dir="artifacts/"):
    print("Loading data...")
    try:
        df = load_data(data_dir)
    except ValueError:
        print("Mocking data for training due to missing parquet files...")
        # Create dummy data for training pipeline testing
        np.random.seed(42)
        n = 10000
        df = pd.DataFrame({
            "timestamp_ns": np.arange(n) * 1_000_000,
            "bid": 100 + np.cumsum(np.random.randn(n) * 0.1),
            "ask": 0,
            "bid_sz": np.random.uniform(1, 10, n),
            "ask_sz": np.random.uniform(1, 10, n),
            "seq_gap": False
        })
        df["ask"] = df["bid"] + np.random.uniform(0.01, 0.05, n)
    
    print("Engineering features...")
    pipe = BatchFeaturePipeline()
    X = pipe.fit_transform(df)
    
    print("Creating labels...")
    y = create_labels(df, horizon=10, alpha=1.0)
    
    # Drop rows with NaN (due to lookback/forward windows or gaps)
    valid_idx = X.notna().all(axis=1) & y.notna()
    X = X[valid_idx]
    y = y[valid_idx]
    
    if len(X) == 0:
        raise ValueError("No valid rows remaining after feature/label creation.")
    
    print(f"Training on {len(X)} samples. Positive class ratio: {y.mean():.4f}")
    
    tscv = TimeSeriesSplit(n_splits=5)
    
    print("Evaluating baseline (Always 0)...")
    base_preds = np.zeros(len(y))
    print(f"Baseline Precision: {precision_score(y, base_preds, zero_division=0):.4f}")
    
    print("Training XGBoost...")
    model = xgb.XGBClassifier(
        n_estimators=100, 
        max_depth=4, 
        learning_rate=0.05, 
        eval_metric='logloss',
        random_state=42
    )
    
    # Walk-forward CV
    for i, (train_idx, val_idx) in enumerate(tscv.split(X)):
        X_tr, y_tr = X.iloc[train_idx], y.iloc[train_idx]
        X_val, y_val = X.iloc[val_idx], y.iloc[val_idx]
        
        model.fit(X_tr, y_tr, eval_set=[(X_val, y_val)], verbose=False)
        preds = model.predict_proba(X_val)[:, 1]
        auc = roc_auc_score(y_val, preds)
        pred_classes = (preds > 0.5).astype(int)
        prec = precision_score(y_val, pred_classes, zero_division=0)
        print(f"Fold {i+1} | AUC-ROC: {auc:.4f} | Precision: {prec:.4f}")

    # Retrain on full data
    print("Retraining on full dataset...")
    model.fit(X, y)
    
    # Ensure artifacts dir exists
    os.makedirs(model_dir, exist_ok=True)
    
    # Save artifacts
    model_path = os.path.join(model_dir, "model.pkl")
    pipe_path = os.path.join(model_dir, "pipe.pkl")
    meta_path = os.path.join(model_dir, "metadata.json")
    
    joblib.dump(model, model_path)
    joblib.dump(pipe, pipe_path)
    
    metadata = {
        "features": FEATURE_NAMES,
        "horizon": 10,
        "train_start": str(df["timestamp_ns"].min()),
        "train_end": str(df["timestamp_ns"].max()),
        "model_version": "v1",
        "timestamp": datetime.now().isoformat()
    }
    with open(meta_path, 'w') as f:
        json.dump(metadata, f, indent=4)
        
    print(f"Saved artifacts to {model_dir}")

if __name__ == "__main__":
    train_model()
