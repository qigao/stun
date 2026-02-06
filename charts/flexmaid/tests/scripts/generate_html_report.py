#!/usr/bin/env python3
"""
Generate an HTML visual comparison report between FlexMaid and official Mermaid output.

This creates an interactive HTML page showing side-by-side comparisons.
"""

import json
from pathlib import Path
from typing import Dict, List

SCRIPT_DIR = Path(__file__).parent
REPORT_FILE = SCRIPT_DIR.parent / "comparison_report.json"
REFERENCE_DIR = SCRIPT_DIR.parent / "reference_output"
FLEXMAID_DIR = SCRIPT_DIR.parent / "flexmaid_output"
OUTPUT_HTML = SCRIPT_DIR.parent / "comparison_report.html"

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FlexMaid vs Mermaid.js Comparison</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            min-height: 100vh;
        }}
        
        .container {{
            max-width: 1400px;
            margin: 0 auto;
        }}
        
        header {{
            background: white;
            border-radius: 12px;
            padding: 30px;
            margin-bottom: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }}
        
        h1 {{
            color: #333;
            margin-bottom: 10px;
            font-size: 2.5em;
        }}
        
        .stats {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }}
        
        .stat-card {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 8px;
            text-align: center;
        }}
        
        .stat-value {{
            font-size: 2em;
            font-weight: bold;
            margin-bottom: 5px;
        }}
        
        .stat-label {{
            font-size: 0.9em;
            opacity: 0.9;
        }}
        
        .filters {{
            background: white;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
        }}
        
        .filter-group {{
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
            align-items: center;
        }}
        
        .filter-group label {{
            font-weight: 600;
            color: #555;
        }}
        
        .filter-group select, .filter-group input {{
            padding: 8px 12px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
        }}
        
        .comparison-grid {{
            display: grid;
            gap: 30px;
        }}
        
        .comparison-item {{
            background: white;
            border-radius: 12px;
            padding: 25px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
            transition: transform 0.3s, box-shadow 0.3s;
        }}
        
        .comparison-item:hover {{
            transform: translateY(-5px);
            box-shadow: 0 15px 35px rgba(0,0,0,0.2);
        }}
        
        .item-header {{
            margin-bottom: 20px;
            padding-bottom: 15px;
            border-bottom: 2px solid #f0f0f0;
        }}
        
        .item-title {{
            font-size: 1.3em;
            color: #333;
            margin-bottom: 10px;
            font-weight: 600;
        }}
        
        .item-meta {{
            display: flex;
            gap: 20px;
            flex-wrap: wrap;
            font-size: 0.9em;
            color: #666;
        }}
        
        .meta-item {{
            display: flex;
            align-items: center;
            gap: 5px;
        }}
        
        .status-badge {{
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: 600;
        }}
        
        .status-success {{
            background: #d4edda;
            color: #155724;
        }}
        
        .status-error {{
            background: #f8d7da;
            color: #721c24;
        }}
        
        .svg-comparison {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-top: 20px;
        }}
        
        .svg-panel {{
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            padding: 15px;
            background: #fafafa;
        }}
        
        .svg-panel h3 {{
            margin-bottom: 10px;
            color: #555;
            font-size: 1.1em;
        }}
        
        .svg-container {{
            background: white;
            border-radius: 6px;
            padding: 20px;
            min-height: 200px;
            display: flex;
            align-items: center;
            justify-content: center;
            overflow: auto;
        }}
        
        .svg-container svg {{
            max-width: 100%;
            height: auto;
        }}
        
        .error-message {{
            color: #d32f2f;
            padding: 10px;
            background: #ffebee;
            border-radius: 4px;
            font-size: 0.9em;
        }}
        
        @media (max-width: 768px) {{
            .svg-comparison {{
                grid-template-columns: 1fr;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🎨 FlexMaid vs Mermaid.js</h1>
            <p style="color: #666; margin-top: 10px;">Visual Comparison Report</p>
            
            <div class="stats">
                <div class="stat-card">
                    <div class="stat-value">{total_files}</div>
                    <div class="stat-label">Total Diagrams</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value">{both_success}</div>
                    <div class="stat-label">Both Successful</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value">{speedup}x</div>
                    <div class="stat-label">Average Speedup</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value">{flexmaid_time:.1f}ms</div>
                    <div class="stat-label">Avg FlexMaid Time</div>
                </div>
            </div>
        </header>
        
        <div class="filters">
            <div class="filter-group">
                <label>Filter:</label>
                <select id="statusFilter">
                    <option value="all">All Diagrams</option>
                    <option value="success">Both Successful</option>
                    <option value="flexmaid-only">FlexMaid Only</option>
                    <option value="reference-only">Reference Only</option>
                    <option value="both-failed">Both Failed</option>
                </select>
                
                <input type="text" id="searchFilter" placeholder="Search by name...">
            </div>
        </div>
        
        <div class="comparison-grid" id="comparisonGrid">
            {comparison_items}
        </div>
    </div>
    
    <script>
        // Filter functionality
        const statusFilter = document.getElementById('statusFilter');
        const searchFilter = document.getElementById('searchFilter');
        const grid = document.getElementById('comparisonGrid');
        const items = grid.querySelectorAll('.comparison-item');
        
        function applyFilters() {{
            const status = statusFilter.value;
            const search = searchFilter.value.toLowerCase();
            
            items.forEach(item => {{
                const title = item.querySelector('.item-title').textContent.toLowerCase();
                const refSuccess = item.dataset.refSuccess === 'true';
                const flexSuccess = item.dataset.flexSuccess === 'true';
                
                let showStatus = true;
                if (status === 'success') showStatus = refSuccess && flexSuccess;
                else if (status === 'flexmaid-only') showStatus = flexSuccess && !refSuccess;
                else if (status === 'reference-only') showStatus = refSuccess && !flexSuccess;
                else if (status === 'both-failed') showStatus = !refSuccess && !flexSuccess;
                
                const showSearch = search === '' || title.includes(search);
                
                item.style.display = (showStatus && showSearch) ? 'block' : 'none';
            }});
        }}
        
        statusFilter.addEventListener('change', applyFilters);
        searchFilter.addEventListener('input', applyFilters);
    </script>
</body>
</html>
"""

def load_report() -> Dict:
    """Load the comparison report JSON."""
    if not REPORT_FILE.exists():
        print(f"❌ Report file not found: {REPORT_FILE}")
        print("   Run generate_reference_svgs.py first.")
        return None
    
    with open(REPORT_FILE, 'r') as f:
        return json.load(f)

def generate_comparison_item(file_data: Dict) -> str:
    """Generate HTML for a single comparison item."""
    path = file_data["path"]
    ref = file_data["reference"]
    flex = file_data["flexmaid"]
    
    ref_status = "✅ Success" if ref["success"] else "❌ Failed"
    flex_status = "✅ Success" if flex["success"] else "❌ Failed"
    
    ref_badge_class = "status-success" if ref["success"] else "status-error"
    flex_badge_class = "status-success" if flex["success"] else "status-error"
    
    # Generate SVG panels
    ref_svg_path = REFERENCE_DIR / Path(path).with_suffix(".svg")
    flex_svg_path = FLEXMAID_DIR / Path(path).with_suffix(".svg")
    
    ref_content = ""
    if ref["success"] and ref_svg_path.exists():
        with open(ref_svg_path, 'r', encoding='utf-8') as f:
            ref_content = f.read()
    elif not ref["success"]:
        ref_content = f'<div class="error-message">{ref["error"]}</div>'
    
    flex_content = ""
    if flex["success"] and flex_svg_path.exists():
        with open(flex_svg_path, 'r', encoding='utf-8') as f:
            flex_content = f.read()
    elif not flex["success"]:
        flex_content = f'<div class="error-message">{flex["error"]}</div>'
    
    speedup = ""
    if ref["success"] and flex["success"] and flex["time_ms"] > 0:
        speedup_val = ref["time_ms"] / flex["time_ms"]
        speedup = f'<div class="meta-item">⚡ {speedup_val:.1f}x faster</div>'
    
    return f"""
    <div class="comparison-item" data-ref-success="{str(ref['success']).lower()}" data-flex-success="{str(flex['success']).lower()}">
        <div class="item-header">
            <div class="item-title">{path}</div>
            <div class="item-meta">
                <div class="meta-item">
                    <span>Reference:</span>
                    <span class="status-badge {ref_badge_class}">{ref_status}</span>
                    <span>({ref['time_ms']:.2f}ms)</span>
                </div>
                <div class="meta-item">
                    <span>FlexMaid:</span>
                    <span class="status-badge {flex_badge_class}">{flex_status}</span>
                    <span>({flex['time_ms']:.2f}ms)</span>
                </div>
                {speedup}
            </div>
        </div>
        
        <div class="svg-comparison">
            <div class="svg-panel">
                <h3>📊 Official Mermaid.js</h3>
                <div class="svg-container">
                    {ref_content}
                </div>
            </div>
            <div class="svg-panel">
                <h3>🚀 FlexMaid</h3>
                <div class="svg-container">
                    {flex_content}
                </div>
            </div>
        </div>
    </div>
    """

def main():
    """Generate the HTML report."""
    print("Generating HTML comparison report...")
    
    # Load report data
    report = load_report()
    if not report:
        return
    
    # Generate comparison items
    comparison_items = []
    for file_data in report["files"]:
        comparison_items.append(generate_comparison_item(file_data))
    
    # Calculate speedup
    speedup = 0
    if report["avg_reference_time_ms"] > 0 and report["avg_flexmaid_time_ms"] > 0:
        speedup = report["avg_reference_time_ms"] / report["avg_flexmaid_time_ms"]
    
    # Generate HTML
    html = HTML_TEMPLATE.format(
        total_files=report["total"],
        both_success=report["both_success"],
        speedup=f"{speedup:.1f}",
        flexmaid_time=report["avg_flexmaid_time_ms"],
        comparison_items="\n".join(comparison_items)
    )
    
    # Save HTML
    with open(OUTPUT_HTML, 'w', encoding='utf-8') as f:
        f.write(html)
    
    print(f"✅ HTML report generated: {OUTPUT_HTML}")
    print(f"   Open in browser to view visual comparisons")

if __name__ == "__main__":
    main()
