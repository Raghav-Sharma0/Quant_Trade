import os
import joblib
import numpy as np
import pandas as pd
from ml.feature_engineering.features import StreamingFeaturePipeline

class Predictor:
    def __init__(self, model_dir="artifacts/"):
        model_path = os.path.join(model_dir, "model.pkl")
        pipe_path = os.path.join(model_dir, "pipe.pkl")
        
        print(f"Loading model from {model_path}")
        self.model = joblib.load(model_path)
        
        # We don't strictly need the batch pipe for inference, but we ensure it exists
        if not os.path.exists(pipe_path):
            print("Warning: pipe.pkl not found.")
            
        self.streaming_pipe = StreamingFeaturePipeline()
        
    def predict(self, bid: float, ask: float, bid_sz: float, ask_sz: float) -> (float, int):
        # 1. Transform raw inputs into full feature vector
        features = self.streaming_pipe.transform_single(bid, ask, bid_sz, ask_sz)
        
        # 2. Predict (requires 2D array)
        X = pd.DataFrame([features], columns=self.streaming_pipe.feature_names) if hasattr(self.streaming_pipe, "feature_names") else np.array([features])
        
        # 3. Get probability of class 1
        prob = self.model.predict_proba(X)[0][1]
        
        # 4. Determine direction (1 for up, 0 for down/flat)
        direction = 1 if prob > 0.5 else 0
        
        return prob, direction
