"""
Feature engineering pipeline for the HFT market-maker ML system.

Two classes are provided:

  BatchFeaturePipeline    — sklearn-compatible transformer for training.
                            Input:  DataFrame with raw tick columns.
                            Output: DataFrame with exactly FEATURE_NAMES columns.

  StreamingFeaturePipeline — stateful single-tick transformer for inference.
                             Maintains rolling history internally.
                             Input:  (bid, ask, bid_sz, ask_sz) per call.
                             Output: list of floats in FEATURE_NAMES order.

FEATURE_NAMES is the proto contract between the C++ feature producer and the
Python model.  Never reorder without bumping metadata["model_version"].

Alpha labels for EWMA suffix:
    α = 0.01  →  suffix "01"
    α = 0.05  →  suffix "05"
    α = 0.10  →  suffix "10"
"""

import numpy as np
import pandas as pd
from sklearn.base import BaseEstimator, TransformerMixin

# ---------------------------------------------------------------------------
# FEATURE_NAMES — the canonical feature order.
# This list is the contract: index 0 == spread, index 1 == microprice, etc.
# The C++ prediction_client.hpp must fill features[] in exactly this order.
# ---------------------------------------------------------------------------
FEATURE_NAMES: list[str] = [
    "spread",               # 0  ask − bid
    "microprice",           # 1  size-weighted mid
    "imbalance",            # 2  (bid_sz − ask_sz) / (bid_sz + ask_sz)
    "log_return_5",         # 3  ln(mid_t / mid_{t-5})
    "log_return_10",        # 4  ln(mid_t / mid_{t-10})
    "log_return_50",        # 5  ln(mid_t / mid_{t-50})
    "rolling_vol_20",       # 6  std(log_return_1, window=20)
    "ewma_spread_01",       # 7  EWMA(spread, α=0.01)
    "ewma_spread_05",       # 8  EWMA(spread, α=0.05)
    "ewma_spread_10",       # 9  EWMA(spread, α=0.10)
    "ewma_imbalance_01",    # 10 EWMA(imbalance, α=0.01)
    "ewma_imbalance_05",    # 11 EWMA(imbalance, α=0.05)
    "ewma_imbalance_10",    # 12 EWMA(imbalance, α=0.10)
    "ewma_log_ret_01",      # 13 EWMA(log_return_1, α=0.01)
    "ewma_log_ret_05",      # 14 EWMA(log_return_1, α=0.05)
    "ewma_log_ret_10",      # 15 EWMA(log_return_1, α=0.10)
]

# Alpha → suffix mapping (used in both classes to stay consistent)
_ALPHAS: list[tuple[float, str]] = [
    (0.01, "01"),
    (0.05, "05"),
    (0.10, "10"),
]


# ---------------------------------------------------------------------------
# BatchFeaturePipeline
# ---------------------------------------------------------------------------

class BatchFeaturePipeline(BaseEstimator, TransformerMixin):
    """
    Batch feature pipeline used during training.

    fit() is a no-op (all features are deterministic).
    transform() accepts a DataFrame with raw tick columns and returns a
    DataFrame with exactly the FEATURE_NAMES columns.

    Gap safety:
        Rows within the lookback window of a seq_gap=True tick have all
        lookback-dependent features set to NaN.  Instantaneous features
        (spread, microprice, imbalance) are left valid.
    """

    MAX_LOOKBACK: int = 50  # max window used in any lookback feature

    def fit(self, X: pd.DataFrame, y=None) -> "BatchFeaturePipeline":
        return self

    def transform(self, X: pd.DataFrame) -> pd.DataFrame:
        df = X.copy()

        # --- instantaneous features ---
        df["spread"] = df["ask"] - df["bid"]
        df["mid"] = (df["bid"] + df["ask"]) / 2.0

        total_sz = df["bid_sz"] + df["ask_sz"]
        df["microprice"] = np.where(
            total_sz > 0,
            (df["bid"] * df["ask_sz"] + df["ask"] * df["bid_sz"]) / total_sz,
            df["mid"],
        )
        df["imbalance"] = np.where(
            total_sz > 0,
            (df["bid_sz"] - df["ask_sz"]) / total_sz,
            0.0,
        )

        # --- log returns ---
        for k in [5, 10, 50]:
            df[f"log_return_{k}"] = np.log(df["mid"] / df["mid"].shift(k))

        # tick-to-tick log return (used for vol and EWMA)
        df["_log_ret_1"] = np.log(df["mid"] / df["mid"].shift(1)).fillna(0.0)

        # --- rolling volatility ---
        df["rolling_vol_20"] = df["_log_ret_1"].rolling(window=20).std()

        # --- EWMAs ---
        for alpha, suffix in _ALPHAS:
            df[f"ewma_spread_{suffix}"]    = df["spread"].ewm(alpha=alpha, adjust=False).mean()
            df[f"ewma_imbalance_{suffix}"] = df["imbalance"].ewm(alpha=alpha, adjust=False).mean()
            df[f"ewma_log_ret_{suffix}"]   = df["_log_ret_1"].ewm(alpha=alpha, adjust=False).mean()

        # --- gap safety: mask all lookback features near a seq_gap ---
        if "seq_gap" in df.columns and df["seq_gap"].any():
            # Any row within MAX_LOOKBACK ticks after a gap is contaminated.
            gap_nearby = df["seq_gap"].rolling(window=self.MAX_LOOKBACK, min_periods=1).max() > 0
            lookback_cols = [
                c for c in FEATURE_NAMES
                if "log_return" in c or "rolling_vol" in c or "ewma" in c
            ]
            df.loc[gap_nearby, lookback_cols] = np.nan

        return df[FEATURE_NAMES]


# ---------------------------------------------------------------------------
# StreamingFeaturePipeline
# ---------------------------------------------------------------------------

class StreamingFeaturePipeline:
    """
    Stateful single-tick feature pipeline used at inference time.

    transform_single(bid, ask, bid_sz, ask_sz) returns a list of floats
    in exactly FEATURE_NAMES order — matching what the batch pipeline produces.

    State is per-instance.  Create one instance per symbol stream and keep it
    alive for the duration of the session.
    """

    def __init__(self) -> None:
        self.feature_names: list[str] = FEATURE_NAMES

        # Rolling history of mid prices (max 51 to cover log_return_50)
        self._mid_history: list[float] = []
        # Rolling history of tick-to-tick log returns (max 20 for rolling vol)
        self._log_ret_history: list[float] = []

        # EWMA state: key → current value (None until first tick)
        self._ewma: dict[str, float | None] = {}
        for _, suffix in _ALPHAS:
            self._ewma[f"spread_{suffix}"]    = None
            self._ewma[f"imbalance_{suffix}"] = None
            self._ewma[f"log_ret_{suffix}"]   = None

    def transform_single(
        self,
        bid: float,
        ask: float,
        bid_sz: float,
        ask_sz: float,
    ) -> list[float]:
        """
        Compute features for one tick and return them in FEATURE_NAMES order.
        NaN is returned for lookback features when history is insufficient.
        """
        spread = ask - bid
        mid = (bid + ask) / 2.0
        total_sz = bid_sz + ask_sz
        microprice = (
            (bid * ask_sz + ask * bid_sz) / total_sz if total_sz > 0 else mid
        )
        imbalance = (bid_sz - ask_sz) / total_sz if total_sz > 0 else 0.0

        # Update mid history (keep last 51 to compute log_return_50)
        self._mid_history.append(mid)
        if len(self._mid_history) > 51:
            self._mid_history.pop(0)

        # Log returns
        n = len(self._mid_history)
        log_ret_1  = float(np.log(mid / self._mid_history[-2]))  if n >= 2  else 0.0
        log_ret_5  = float(np.log(mid / self._mid_history[-6]))  if n >= 6  else float("nan")
        log_ret_10 = float(np.log(mid / self._mid_history[-11])) if n >= 11 else float("nan")
        log_ret_50 = float(np.log(mid / self._mid_history[-51])) if n >= 51 else float("nan")

        # Rolling volatility (std of last 20 tick-to-tick log returns)
        self._log_ret_history.append(log_ret_1)
        if len(self._log_ret_history) > 20:
            self._log_ret_history.pop(0)
        rolling_vol = (
            float(np.std(self._log_ret_history, ddof=1))
            if len(self._log_ret_history) >= 2
            else float("nan")
        )

        # EWMAs
        def update_ewma(key: str, value: float, alpha: float) -> float:
            prev = self._ewma[key]
            new_val = value if prev is None else alpha * value + (1.0 - alpha) * prev
            self._ewma[key] = new_val
            return new_val

        ewma_vals: dict[str, float] = {}
        for alpha, suffix in _ALPHAS:
            ewma_vals[f"ewma_spread_{suffix}"]    = update_ewma(f"spread_{suffix}",    spread,    alpha)
            ewma_vals[f"ewma_imbalance_{suffix}"] = update_ewma(f"imbalance_{suffix}", imbalance, alpha)
            ewma_vals[f"ewma_log_ret_{suffix}"]   = update_ewma(f"log_ret_{suffix}",   log_ret_1, alpha)

        feature_map: dict[str, float] = {
            "spread":         spread,
            "microprice":     microprice,
            "imbalance":      imbalance,
            "log_return_5":   log_ret_5,
            "log_return_10":  log_ret_10,
            "log_return_50":  log_ret_50,
            "rolling_vol_20": rolling_vol,
            **ewma_vals,
        }

        return [feature_map[k] for k in FEATURE_NAMES]
