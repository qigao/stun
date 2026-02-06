#!/usr/bin/env python3
"""
Generate reference SVGs using official Mermaid CLI for comparison testing.

This script:
1. Finds all .mmd fixture files
2. Generates SVGs using official mermaid-cli (mmdc)
3. Saves them to tests/reference_output/
4. Creates a comparison report

Requirements:
    pip install pyppeteer
    npm install -g @mermaid-js/mermaid-cli
"""

import os
import subprocess
import json
import time
from pathlib import Path
from typing import List, Dict, Tuple

# Paths
SCRIPT_DIR = Path(__file__).parent
FIXTURES_DIR = SCRIPT_DIR.parent / "fixtures"
REFERENCE_DIR = SCRIPT_DIR.parent / "reference_output"
FLEXMAID_DIR = SCRIPT_DIR.parent / "flexmaid_output"
REPORT_FILE = SCRIPT_DIR.parent / "comparison_report.json"

def find_fixture_files() -> List[Path]:
    """Find all .mmd files in the fixtures directory."""
    fixtures = []
    for mmd_file in FIXTURES_DIR.rglob("*.mmd"):
        fixtures.append(mmd_file)
    return sorted(fixtures)

def generate_reference_svg(mmd_file: Path, output_file: Path) -> Tuple[bool, str, float]:
    """
    Generate SVG using official mermaid-cli.
    
    Returns:
        (success, error_message, time_taken)
    """
    start_time = time.time()
    
    try:
        # Create output directory if needed
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Use mmdc.cmd on Windows, mmdc on others
        cmd = "mmdc.cmd" if os.name == 'nt' else "mmdc"
        
        # Run mermaid-cli with UTF-8 encoding for Windows
        result = subprocess.run(
            [cmd, "-i", str(mmd_file), "-o", str(output_file), "-b", "transparent"],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            timeout=30
        )
        
        elapsed = time.time() - start_time
        
        if result.returncode == 0:
            return True, "", elapsed
        else:
            return False, result.stderr, elapsed
            
    except subprocess.TimeoutExpired:
        return False, "Timeout (30s)", 30.0
    except FileNotFoundError:
        return False, "mermaid-cli (mmdc) not found. Install with: npm install -g @mermaid-js/mermaid-cli", 0.0
    except Exception as e:
        return False, str(e), time.time() - start_time

def generate_flexmaid_svg(mmd_file: Path, output_file: Path, exe_path: Path) -> Tuple[bool, str, float]:
    """
    Generate SVG using FlexMaid.
    
    Returns:
        (success, error_message, time_taken)
    """
    start_time = time.time()
    
    try:
        # Create output directory if needed
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Run FlexMaid with new CLI interface: flexmaid_example.exe <input.mmd> <output.svg>
        result = subprocess.run(
            [str(exe_path), str(mmd_file), str(output_file)],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',  # Replace invalid characters instead of failing
            timeout=10
        )
        
        elapsed = time.time() - start_time
        
        if result.returncode == 0:
            return True, "", elapsed
        else:
            return False, result.stderr, elapsed
            
    except subprocess.TimeoutExpired:
        return False, "Timeout (10s)", 10.0
    except FileNotFoundError:
        return False, f"FlexMaid executable not found: {exe_path}", 0.0
    except Exception as e:
        return False, str(e), time.time() - start_time

def compare_svg_sizes(ref_file: Path, flex_file: Path) -> Dict:
    """Compare file sizes and basic metrics."""
    comparison = {}
    
    if ref_file.exists():
        comparison["reference_size"] = ref_file.stat().st_size
    else:
        comparison["reference_size"] = 0
        
    if flex_file.exists():
        comparison["flexmaid_size"] = flex_file.stat().st_size
    else:
        comparison["flexmaid_size"] = 0
        
    if comparison["reference_size"] > 0 and comparison["flexmaid_size"] > 0:
        ratio = comparison["flexmaid_size"] / comparison["reference_size"]
        comparison["size_ratio"] = ratio
        comparison["size_difference_pct"] = (ratio - 1.0) * 100
    
    return comparison

def main():
    """Main execution."""
    print("=" * 80)
    print("FlexMaid vs Official Mermaid Comparison Test")
    print("=" * 80)
    print()
    
    # Find FlexMaid executable
    flexmaid_exe = SCRIPT_DIR.parent.parent.parent.parent / "build/Ninja/Msvc/bin/flexmaid_example.exe"
    if not flexmaid_exe.exists():
        print(f"❌ FlexMaid executable not found: {flexmaid_exe}")
        print("   Please build FlexMaid first.")
        return
    
    # Find all fixtures
    fixtures = find_fixture_files()
    print(f"Found {len(fixtures)} fixture files\n")
    
    # Results tracking
    results = {
        "total": len(fixtures),
        "reference_success": 0,
        "flexmaid_success": 0,
        "both_success": 0,
        "total_reference_time": 0.0,
        "total_flexmaid_time": 0.0,
        "files": []
    }
    
    # Process each fixture
    for i, mmd_file in enumerate(fixtures, 1):
        rel_path = mmd_file.relative_to(FIXTURES_DIR)
        print(f"[{i}/{len(fixtures)}] {rel_path}")
        
        # Generate reference SVG
        ref_output = REFERENCE_DIR / rel_path.with_suffix(".svg")
        ref_success, ref_error, ref_time = generate_reference_svg(mmd_file, ref_output)
        
        # Generate FlexMaid SVG
        flex_output = FLEXMAID_DIR / rel_path.with_suffix(".svg")
        flex_success, flex_error, flex_time = generate_flexmaid_svg(mmd_file, flex_output, flexmaid_exe)
        
        # Update stats
        if ref_success:
            results["reference_success"] += 1
            results["total_reference_time"] += ref_time
            
        if flex_success:
            results["flexmaid_success"] += 1
            results["total_flexmaid_time"] += flex_time
            
        if ref_success and flex_success:
            results["both_success"] += 1
        
        # Compare
        comparison = compare_svg_sizes(ref_output, flex_output)
        
        # Store result
        file_result = {
            "path": str(rel_path),
            "reference": {
                "success": ref_success,
                "error": ref_error,
                "time_ms": ref_time * 1000
            },
            "flexmaid": {
                "success": flex_success,
                "error": flex_error,
                "time_ms": flex_time * 1000
            },
            "comparison": comparison
        }
        results["files"].append(file_result)
        
        # Print status
        ref_status = "✅" if ref_success else "❌"
        flex_status = "✅" if flex_success else "❌"
        print(f"  Reference: {ref_status} ({ref_time*1000:.2f}ms)")
        print(f"  FlexMaid:  {flex_status} ({flex_time*1000:.2f}ms)")
        
        if ref_success and flex_success:
            speedup = ref_time / flex_time if flex_time > 0 else 0
            print(f"  Speedup:   {speedup:.1f}x faster")
        print()
    
    # Calculate averages
    if results["reference_success"] > 0:
        results["avg_reference_time_ms"] = (results["total_reference_time"] / results["reference_success"]) * 1000
    else:
        results["avg_reference_time_ms"] = 0
        
    if results["flexmaid_success"] > 0:
        results["avg_flexmaid_time_ms"] = (results["total_flexmaid_time"] / results["flexmaid_success"]) * 1000
    else:
        results["avg_flexmaid_time_ms"] = 0
    
    # Save report
    REPORT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(REPORT_FILE, 'w') as f:
        json.dump(results, f, indent=2)
    
    # Print summary
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Total Fixtures:        {results['total']}")
    print(f"Reference Success:     {results['reference_success']} ({results['reference_success']/results['total']*100:.1f}%)")
    print(f"FlexMaid Success:      {results['flexmaid_success']} ({results['flexmaid_success']/results['total']*100:.1f}%)")
    print(f"Both Success:          {results['both_success']} ({results['both_success']/results['total']*100:.1f}%)")
    print()
    print(f"Avg Reference Time:    {results['avg_reference_time_ms']:.2f}ms")
    print(f"Avg FlexMaid Time:     {results['avg_flexmaid_time_ms']:.2f}ms")
    
    if results['avg_reference_time_ms'] > 0 and results['avg_flexmaid_time_ms'] > 0:
        speedup = results['avg_reference_time_ms'] / results['avg_flexmaid_time_ms']
        print(f"Average Speedup:       {speedup:.1f}x faster")
    
    print()
    print(f"Report saved to: {REPORT_FILE}")
    print(f"Reference SVGs: {REFERENCE_DIR}")
    print(f"FlexMaid SVGs:  {FLEXMAID_DIR}")

if __name__ == "__main__":
    main()
