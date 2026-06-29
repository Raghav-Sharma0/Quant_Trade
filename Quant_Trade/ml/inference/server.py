import grpc
from concurrent import futures
import time
import logging

import prediction_pb2
import prediction_pb2_grpc
from predictor import Predictor

class PredictionService(prediction_pb2_grpc.PredictionServiceServicer):
    def __init__(self, model_dir="artifacts/"):
        self.predictor = Predictor(model_dir)

    def Predict(self, request, context):
        try:
            # Assuming C++ sends [bid, ask, bid_sz, ask_sz] in features array
            if len(request.features) < 4:
                context.abort(grpc.StatusCode.INVALID_ARGUMENT, "Expected at least 4 features: bid, ask, bid_sz, ask_sz")
                
            bid = request.features[0]
            ask = request.features[1]
            bid_sz = request.features[2]
            ask_sz = request.features[3]
            
            prob, direction = self.predictor.predict(bid, ask, bid_sz, ask_sz)
            
            return prediction_pb2.PredictionResponse(
                symbol=request.symbol,
                price_direction=direction,
                predicted_value=prob,
                timestamp_ns=request.timestamp_ns
            )
        except Exception as e:
            logging.error(f"Prediction failed: {e}")
            context.abort(grpc.StatusCode.INTERNAL, str(e))

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    prediction_pb2_grpc.add_PredictionServiceServicer_to_server(PredictionService(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("Inference Server started on port 50051.")
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    serve()
