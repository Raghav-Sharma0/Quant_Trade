import pandas as pd
import numpy as np
from sklearn.base import BaseEstimator, TransformerMixin

FEATURE_NAMES = [
    "spread",
    "microprice",
    "imbalance",
    "log_return_5",
    "log_return_10",
    "log_return_50",
    "rolling_vol_20",
    "ewma_spread_01",
    "ewma_spread_05",
    "ewma_spread_10",
    "ewma_imbalance_01",
    "ewma_imbalance_05",
    "ewma_imbalance_10",
    "ewma_log_ret_01",
    "ewma_log_ret_05",
    "ewma_log_ret_10"
]

class BatchFeaturePipeline(BaseEstimator, TransformerMixin):
    def fit(self, X, y=None):
        return self

    def transform(self, X):
        df = X.copy()
        
        # Instantaneous
        df["spread"] = df["ask"] - df["bid"]
        df["mid"] = (df["bid"] + df["ask"]) / 2.0
        
        total_sz = df["bid_sz"] + df["ask_sz"]
        # avoid division by zero
        df["microprice"] = np.where(total_sz > 0, 
                                    (df["bid"] * df["ask_sz"] + df["ask"] * df["bid_sz"]) / total_sz, 
                                    df["mid"])
        
        df["imbalance"] = np.where(total_sz > 0,
                                   (df["bid_sz"] - df["ask_sz"]) / total_sz,
                                   0.0)
        
        # Log returns
        for k in [5, 10, 50]:
            df[f"log_return_{k}"] = np.log(df["mid"] / df["mid"].shift(k))
            
        # Base log return for rolling vol and ewma
        df["log_return_1"] = np.log(df["mid"] / df["mid"].shift(1)).fillna(0.0)
        
        # Rolling Volatility
        df["rolling_vol_20"] = df["log_return_1"].rolling(window=20).std()
        
        # EWMAs
        alphas = [0.01, 0.05, 0.1]
        for a in alphas:
            suffix = "10" if a == 0.1 else str(a)[2:]
            df[f"ewma_spread_{suffix}"] = df["spread"].ewm(alpha=a, adjust=False).mean()
            df[f"ewma_imbalance_{suffix}"] = df["imbalance"].ewm(alpha=a, adjust=False).mean()
            df[f"ewma_log_ret_{suffix}"] = df["log_return_1"].ewm(alpha=a, adjust=False).mean()
            
        # Gap safety: mask lookback features near seq_gap=True
        # If there's a gap at index i, then for row j > i, if j - i <= 50 (max lookback), it's contaminated.
        if "seq_gap" in df.columns:
            gap_indices = df.index[df["seq_gap"]].tolist()
            # Simple approach: if rolling sum of seq_gap over last 50 > 0, mask lookbacks
            gap_in_50 = df["seq_gap"].rolling(window=50, min_periods=1).max() > 0
            
            lookback_cols = [c for c in FEATURE_NAMES if "log_return" in c or "rolling_vol" in c or "ewma" in c]
            df.loc[gap_in_50, lookback_cols] = np.nan
            
        return df[FEATURE_NAMES]

class StreamingFeaturePipeline:
    """Stateful feature pipeline for single-tick inference."""
    def __init__(self):
        self.history = []  # store last 50 mids
        self.log_ret_history = [] # store last 20 log returns for vol
        self.feature_names = FEATURE_NAMES
        
        self.ewma_state = {}
        self.alphas = [0.01, 0.05, 0.1]
        for a in self.alphas:
            suffix = str(a)[2:]
            self.ewma_state[f"spread_{suffix}"] = None
            self.ewma_state[f"imbalance_{suffix}"] = None
            self.ewma_state[f"log_ret_{suffix}"] = None
            
    def _update_ewma(self, key, value, alpha):
        if self.ewma_state[key] is None:
            self.ewma_state[key] = value
        else:
            self.ewma_state[key] = alpha * value + (1 - alpha) * self.ewma_state[key]
        return self.ewma_state[key]

    def transform_single(self, bid: float, ask: float, bid_sz: float, ask_sz: float):
        spread = ask - bid
        mid = (bid + ask) / 2.0
        total_sz = bid_sz + ask_sz
        microprice = (bid * ask_sz + ask * bid_sz) / total_sz if total_sz > 0 else mid
        imbalance = (bid_sz - ask_sz) / total_sz if total_sz > 0 else 0.0
        
        self.history.append(mid)
        if len(self.history) > 51:
            self.history.pop(0)
            
        # Log returns
        log_ret_5 = np.log(mid / self.history[-6]) if len(self.history) >= 6 else 0.0
        log_ret_10 = np.log(mid / self.history[-11]) if len(self.history) >= 11 else 0.0
        log_ret_50 = np.log(mid / self.history[-51]) if len(self.history) >= 51 else 0.0
        log_ret_1 = np.log(mid / self.history[-2]) if len(self.history) >= 2 else 0.0
        
        self.log_ret_history.append(log_ret_1)
        if len(self.log_ret_history) > 20:
            self.log_ret_history.pop(0)
            
        rolling_vol = np.std(self.log_ret_history, ddof=1) if len(self.log_ret_history) >= 2 else 0.0
        
        feats = {
            "spread": spread,
            "microprice": microprice,
            "imbalance": imbalance,
            "log_return_5": log_ret_5,
            "log_return_10": log_ret_10,
            "log_return_50": log_ret_50,
            "rolling_vol_20": rolling_vol
        }
        
        for a in self.alphas:
            suffix = "10" if a == 0.1 else str(a)[2:]
            feats[f"ewma_spread_{suffix}"] = self._update_ewma(f"spread_{suffix}", spread, a)
            feats[f"ewma_imbalance_{suffix}"] = self._update_ewma(f"imbalance_{suffix}", imbalance, a)
            feats[f"ewma_log_ret_{suffix}"] = self._update_ewma(f"log_ret_{suffix}", log_ret_1, a)
            
        # Return in exactly FEATURE_NAMES order
        return [feats[k] for k in FEATURE_NAMES]
