"""
Predictor — loads trained artifacts and serves predictions.

Feature ordering contract (matches FEATURE_NAMES in features.py):
  The C++ prediction_client sends a float array via gRPC where:
    features[0]  = spread
    features[1]  = microprice
    ...
    features[15] = ewma_log_ret_10

  The server passes bid/ask/bid_sz/ask_sz and the Predictor computes
  the full feature vector via StreamingFeaturePipeline before inference.
"""

import logging
import os

import joblib
import numpy as np
import pandas as pd

from ml.feature_engineering.features import StreamingFeaturePipeline, FEATURE_NAMES

logger = logging.getLogger(__name__)


class Predictor:
    """
    Wraps the trained model and streaming feature pipeline.

    Load once at server startup; call predict() per tick.
    Thread-safe for concurrent gRPC calls (each call has no shared mutable state
    other than the streaming pipeline — keep one Predictor per symbol stream).
    """

    def __init__(self, model_dir: str = "artifacts") -> None:
        model_path = os.path.join(model_dir, "model.pkl")
        pipe_path  = os.path.join(model_dir, "pipe.pkl")

        if not os.path.exists(model_path):
            raise FileNotFoundError(
                f"model.pkl not found at {model_path}. "
                "Run ml/training/train.py first."
            )

        logger.info("loading model from %s", model_path)
        self.model = joblib.load(model_path)

        if not os.path.exists(pipe_path):
            logger.warning("pipe.pkl not found at %s — using fresh streaming pipeline", pipe_path)
        else:
            logger.info("pipe.pkl found at %s", pipe_path)

        # The streaming pipeline owns the rolling state for this session.
        self.streaming_pipe = StreamingFeaturePipeline()
        self.feature_names = FEATURE_NAMES

    def predict(
        self,
        bid: float,
        ask: float,
        bid_sz: float,
        ask_sz: float,
    ) -> tuple[float, int]:
        """
        Compute features for one tick and return (buy_prob, direction).

        buy_prob  — probability of an upward price move (0.0 – 1.0)
        direction — 1 (up) if buy_prob > 0.5, else 0 (down / flat)
        """
        feature_vec = self.streaming_pipe.transform_single(bid, ask, bid_sz, ask_sz)

        # Replace NaN with 0.0 for early ticks where history is insufficient.
        feature_vec = [0.0 if (v != v) else v for v in feature_vec]

        X = pd.DataFrame([feature_vec], columns=self.feature_names)
        buy_prob = float(self.model.predict_proba(X)[0][1])
        direction = 1 if buy_prob > 0.5 else 0
        return buy_prob, direction
