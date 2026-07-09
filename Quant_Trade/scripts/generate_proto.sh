#!/bin/bash
# =============================================================================
# Generate Go and Python protobuf/gRPC bindings.
# =============================================================================

set -e

# Root workspace directory
WORKSPACE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${WORKSPACE_DIR}"

echo "Generating Go protobuf/gRPC stubs..."
mkdir -p backend-go/generated/proto

# Ensure local go install path is in PATH
export PATH=$PATH:$HOME/go/bin

# Generate Go stubs
protoc -Iproto \
  --go_out=backend-go \
  --go_opt=module=github.com/anshul/hft/backend \
  --go-grpc_out=backend-go \
  --go-grpc_opt=module=github.com/anshul/hft/backend \
  proto/*.proto

echo "Generating Python protobuf/gRPC stubs..."
# Generate Python stubs
python -m grpc_tools.protoc -Iproto \
  --python_out=ml/inference \
  --grpc_python_out=ml/inference \
  proto/prediction.proto

echo "SUCCESS: Protobuf generation complete!"
