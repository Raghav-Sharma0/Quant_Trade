#pragma once

#include <grpcpp/grpcpp.h>
#include "prediction.grpc.pb.h"

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <string>

namespace hft {

class PredictionClient {
public:
    PredictionClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(hft::prediction::v1::PredictionService::NewStub(channel)),
          circuit_open_(false),
          failure_count_(0) {
        
        cq_thread_ = std::thread(&PredictionClient::AsyncCompleteRpc, this);
    }

    ~PredictionClient() {
        cq_.Shutdown();
        if (cq_thread_.joinable()) {
            cq_thread_.join();
        }
    }

    // Non-blocking async gRPC call with Circuit Breaker
    void PredictAsync(const std::string& symbol, const std::vector<double>& features, int64_t timestamp_ns) {
        if (circuit_open_.load(std::memory_order_relaxed)) {
            // Circuit is open, skip sending request to avoid overwhelming a failing ML server
            return;
        }

        hft::prediction::v1::PredictionRequest req;
        req.set_symbol(symbol);
        for (double f : features) req.add_features(f);
        req.set_timestamp_ns(timestamp_ns);

        auto* call = new AsyncClientCall;
        call->response_reader = stub_->PrepareAsyncPredict(&call->context, req, &cq_);
        call->response_reader->StartCall();
        call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    }

    // Zero-overhead cached read for the strategy engine
    float get_latest_prediction() const {
        return cached_prediction_.load(std::memory_order_relaxed);
    }

private:
    struct AsyncClientCall {
        hft::prediction::v1::PredictionResponse reply;
        grpc::ClientContext context;
        grpc::Status status;
        std::unique_ptr<grpc::ClientAsyncResponseReader<hft::prediction::v1::PredictionResponse>> response_reader;
    };

    std::unique_ptr<hft::prediction::v1::PredictionService::Stub> stub_;
    grpc::CompletionQueue cq_;
    std::thread cq_thread_;

    std::atomic<float> cached_prediction_{0.0f};

    // Circuit Breaker state
    std::atomic<bool> circuit_open_;
    std::atomic<int> failure_count_;
    const int MAX_FAILURES = 5;

    void AsyncCompleteRpc() {
        void* got_tag;
        bool ok = false;

        while (cq_.Next(&got_tag, &ok)) {
            AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
            
            if (ok && call->status.ok()) {
                cached_prediction_.store(static_cast<float>(call->reply.predicted_value()), std::memory_order_relaxed);
                failure_count_.store(0, std::memory_order_relaxed);
                circuit_open_.store(false, std::memory_order_relaxed);
            } else {
                int fails = failure_count_.fetch_add(1, std::memory_order_relaxed) + 1;
                if (fails >= MAX_FAILURES) {
                    circuit_open_.store(true, std::memory_order_relaxed);
                }
            }
            delete call;
        }
    }
};

} // namespace hft
