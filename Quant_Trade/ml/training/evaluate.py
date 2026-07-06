"""
Model evaluation utilities.

Run from project root:
    PYTHONPATH=. python ml/training/evaluate.py --model-dir artifacts --data-dir ml/data/raw
"""

import argparse
import logging
import os

import joblib
import numpy as np
import pandas as pd
from sklearn.metrics import (
    classification_report,
    precision_score,
    recall_score,
    roc_auc_score,
)

from ml.feature_engineering.features import BatchFeaturePipeline, FEATURE_NAMES
from ml.training.train import load_data, create_labels, _make_synthetic_data

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    datefmt="%Y-%m-%dT%H:%M:%S",
)
logger = logging.getLogger(__name__)


def evaluate(model_dir: str = "artifacts", data_dir: str = "ml/data/raw") -> None:
    """
    Load the saved model and pipe, run on held-out test data (last 15%),
    and print a full evaluation report.
    """
    model_path = os.path.join(model_dir, "model.pkl")
    pipe_path  = os.path.join(model_dir, "pipe.pkl")

    if not os.path.exists(model_path) or not os.path.exists(pipe_path):
        raise FileNotFoundError(
            f"Artifacts not found in {model_dir}. Run ml/training/train.py first."
        )

    model = joblib.load(model_path)
    pipe  = joblib.load(pipe_path)
    logger.info("loaded model and pipe from %s", model_dir)

    try:
        df = load_data(data_dir)
    except FileNotFoundError:
        df = _make_synthetic_data()

    X_full = pipe.transform(df)
    y_full = create_labels(df)

    valid = X_full.notna().all(axis=1) & y_full.notna()
    X_full, y_full = X_full[valid], y_full[valid]

    # Use the last 15% as the test set (time-based, never shuffle)
    test_start = int(len(X_full) * 0.85)
    X_test = X_full.iloc[test_start:]
    y_test = y_full.iloc[test_start:]

    logger.info("evaluating on %d test samples (last 15%%)...", len(X_test))

    proba  = model.predict_proba(X_test)[:, 1]
    preds  = (proba > 0.5).astype(int)

    auc  = roc_auc_score(y_test, proba)
    prec = precision_score(y_test, preds, zero_division=0)
    rec  = recall_score(y_test, preds, zero_division=0)

    print(f"\n{'=' * 50}")
    print(f"Test set evaluation  ({len(X_test):,} samples)")
    print(f"{'=' * 50}")
    print(f"AUC-ROC:    {auc:.4f}")
    print(f"Precision:  {prec:.4f}")
    print(f"Recall:     {rec:.4f}")
    print(f"\nClassification report:")
    print(classification_report(y_test, preds, zero_division=0))

    # Feature importances
    importances = dict(zip(FEATURE_NAMES, model.feature_importances_))
    top = sorted(importances.items(), key=lambda x: x[1], reverse=True)[:10]
    print("Top-10 feature importances:")
    for fname, score in top:
        print(f"  {fname:<25} {score:.4f}")
    print(f"{'=' * 50}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Evaluate trained model on test split.")
    parser.add_argument("--model-dir", default="artifacts")
    parser.add_argument("--data-dir",  default="ml/data/raw")
    args = parser.parse_args()
    evaluate(model_dir=args.model_dir, data_dir=args.data_dir)
