#!/usr/bin/env python3
"""
run_risk_bench.py — Build, run, and export risk engine latency benchmark results.

Usage (from repo root or core-cpp/):
    python3 core-cpp/scripts/run_risk_bench.py
    # or from core-cpp/:
    python3 scripts/run_risk_bench.py
"""

import subprocess
import json
import re
import sys
import os
from datetime import datetime
from pathlib import Path

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR  = Path(__file__).resolve().parent
CORE_DIR    = SCRIPT_DIR.parent
BUILD_DIR   = CORE_DIR / "build"
RESULTS_DIR = CORE_DIR / "results"
BINARY      = BUILD_DIR / "bench_risk_engine"

RESULTS_DIR.mkdir(exist_ok=True)


def run(cmd, **kwargs):
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        print(f"  [STDERR] {result.stderr.strip()}")
        sys.exit(f"Command failed with exit code {result.returncode}")
    return result.stdout


# ── 1. Build ──────────────────────────────────────────────────────────────────
print("=== Step 1: Building bench_risk_engine ===")
BUILD_DIR.mkdir(exist_ok=True)

run(["cmake", "-S", str(CORE_DIR), "-B", str(BUILD_DIR),
     "-DCMAKE_BUILD_TYPE=Release", "-Wno-dev"],
    cwd=str(CORE_DIR))

run(["cmake", "--build", str(BUILD_DIR), "--target", "bench_risk_engine"],
    cwd=str(CORE_DIR))

print(f"  Built: {BINARY}\n")


# ── 2. Run benchmark ──────────────────────────────────────────────────────────
print("=== Step 2: Running benchmark (this takes ~10 seconds) ===")
raw_output = run([str(BINARY)])
print(raw_output)


# ── 3. Parse JSON export block ────────────────────────────────────────────────
print("=== Step 3: Parsing results ===")
match = re.search(
    r"=== JSON_EXPORT_BEGIN ===(.*?)=== JSON_EXPORT_END ===",
    raw_output, re.DOTALL
)

if not match:
    sys.exit("Could not find JSON_EXPORT_BEGIN block in benchmark output.")

json_str  = match.group(1).strip()
bench_data = json.loads(json_str)


# ── 4. Build full result document ─────────────────────────────────────────────
timestamp     = datetime.utcnow().strftime("%Y%m%d_%H%M%S")
timestamp_iso = datetime.utcnow().isoformat()

result_doc = {
    "timestamp_iso": timestamp_iso,
    "generated_by": "core-cpp/scripts/run_risk_bench.py",
    "benchmarks": {
        "bench_risk_engine": {
            "status": "success",
            "raw_output": raw_output,
            "parsed": bench_data
        }
    }
}


# ── 5. Save to results/ ───────────────────────────────────────────────────────
out_file = RESULTS_DIR / f"risk_bench_{timestamp}.json"
with open(out_file, "w") as f:
    json.dump(result_doc, f, indent=4)

print(f"=== Step 4: Results saved ===")
print(f"  File: {out_file}\n")


# ── 6. Print summary ──────────────────────────────────────────────────────────
# New benchmark exports: latency{}, throughput{}, outliers{}, compliance{}, system{}, run{}
lat      = bench_data.get("latency", bench_data.get("latency_ns", {}))
comp     = bench_data.get("compliance", {})
sys_info = bench_data.get("system", {})
run_info = bench_data.get("run", {})
tput     = bench_data.get("throughput", {})
outliers = bench_data.get("outliers", {})

print("=== Risk Engine Latency Summary ===")
print(f"  CPU           : {sys_info.get('cpu', 'N/A')}")
print(f"  Compiler      : {sys_info.get('compiler', 'N/A')}")
print(f"  Flags         : {sys_info.get('flags', 'N/A')}")
print(f"  TSC Rate      : {sys_info.get('tsc_ghz', 'N/A')} GHz")
print(f"  CPU Freq      : {sys_info.get('cpu_mhz', 'N/A')} MHz")
pin_ok = sys_info.get('pin_success', False)
print(f"  Pinned Core   : {sys_info.get('pinned_core', 'N/A')} — {'SUCCESS' if pin_ok else 'FAILED'}")
print(f"  Iterations    : {run_info.get('iterations', 0):,}")
print(f"  Warm-up       : {run_info.get('warmup', 0):,}")
print(f"  Elapsed       : {run_info.get('elapsed_s', 'N/A'):.4f} s")
print()
print(f"  Min           : {lat.get('min',  'N/A')} ns")
print(f"  Mean          : {lat.get('mean', 'N/A')} ns")
print(f"  Std Dev       : {lat.get('stddev', 'N/A')} ns")
print(f"  Variance      : {lat.get('variance', 'N/A')} ns^2")
print(f"  Coeff of Var  : {lat.get('cv_pct', 'N/A')} %")
print(f"  P50 (median)  : {lat.get('p50',  'N/A')} ns")
print(f"  P90           : {lat.get('p90',  'N/A')} ns")
print(f"  P95           : {lat.get('p95',  'N/A')} ns")
print(f"  P99           : {lat.get('p99',  'N/A')} ns")
print(f"  P99.9 (tail)  : {lat.get('p99_9', lat.get('p99.9', 'N/A'))} ns")
print(f"  Max           : {lat.get('max',  'N/A')} ns")
print()
cps = tput.get('checks_per_second', 0)
print(f"  Throughput    : {cps/1e6:.2f} M checks/sec")
print(f"  Outliers      : {outliers.get('count', 'N/A')}  (fence: {outliers.get('fence_ns', 'N/A')} ns)")
spike = outliers.get('spike_warned', False)
if spike:
    print(f"  SPIKE WARNING : Ratio {outliers.get('spike_ratio', '?')}x above P99.9")
print()
print(f"  P99  < 500 ns : {'PASS' if comp.get('p99_under_500ns')   else 'FAIL'}")
print(f"  P99.9 <1000 ns: {'PASS' if comp.get('p999_under_1000ns') else 'FAIL'}")
print()
print(f"Result file: {out_file}")

