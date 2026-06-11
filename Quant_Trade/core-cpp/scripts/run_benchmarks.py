import subprocess
import json
import os
import re
from datetime import datetime

BUILD_DIR = "build"
RESULTS_DIR = "results"
BENCHMARKS = [
    "./bench_latency",
    "./bench_order_book",
    "./bench_matching_engine"
]

def run_cmd(cmd, cwd=None):
    try:
        result = subprocess.run(
            cmd, 
            shell=True, 
            cwd=cwd, 
            check=True, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, 
            text=True
        )
        return result.stdout, None
    except subprocess.CalledProcessError as e:
        return e.stdout, e.stderr

def parse_benchmark_output(output):
    """
    Tries to parse Min/Max/Avg latency from the raw text output.
    Returns a structured dictionary, or just the raw output if parsing fails.
    """
    sections = {}
    current_section = "general"
    sections[current_section] = {"raw": ""}
    
    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue
            
        header_match = re.match(r"^===\s*(.+?)\s*===$", line)
        if header_match:
            current_section = header_match.group(1).lower().replace(" ", "_")
            sections[current_section] = {"raw": ""}
            continue
            
        sections[current_section]["raw"] += line + "\n"
        
        metric_match = re.match(r"^(Min|Max|Avg):\s*([0-9.]+)\s*ns\s*\(([0-9.]+)\s*cycles\)", line)
        if metric_match:
            metric_type = metric_match.group(1).lower()
            ns_val = float(metric_match.group(2))
            cycles_val = float(metric_match.group(3))
            
            sections[current_section][f"{metric_type}_ns"] = ns_val
            sections[current_section][f"{metric_type}_cycles"] = cycles_val
            
    return sections

def main():
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_path = os.path.join(project_root, BUILD_DIR)
    results_path = os.path.join(project_root, RESULTS_DIR)
    
    if not os.path.exists(build_path):
        print(f"Error: Build directory not found at {build_path}")
        print("Please compile the project first using: cd build && cmake .. && make")
        return
        
    os.makedirs(results_path, exist_ok=True)
    
    print("Running HFT Core Benchmarks...\n")
    
    results = {
        "timestamp_iso": datetime.now().isoformat(),
        "benchmarks": {}
    }
    
    for bench in BENCHMARKS:
        print(f"Executing: {bench}")
        stdout, stderr = run_cmd(bench, cwd=build_path)
        
        bench_data = {
            "status": "success" if stderr is None else "failed",
            "stdout_raw": stdout,
            "parsed_metrics": parse_benchmark_output(stdout) if stdout else {}
        }
        
        if stderr:
            bench_data["stderr"] = stderr
            print(f"  [!] Benchmark failed or produced stderr.")
        else:
            print(f"  [+] Completed successfully.")
            
        results["benchmarks"][bench.replace("./", "")] = bench_data

    timestamp_str = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = os.path.join(results_path, f"benchmark_{timestamp_str}.json")
    
    with open(filename, 'w') as f:
        json.dump(results, f, indent=4)
        
    print(f"\nAll benchmark results saved to:")
    print(f"-> {filename}")

if __name__ == "__main__":
    main()
