# DDF as Project and Document Format

## Core Concept

**DDF is the universal format for saving, sharing, and opening whiteboard projects.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    DDF = PROJECT FORMAT                          │
└─────────────────────────────────────────────────────────────────┘

User creates diagram
    ↓
Save → DDF .json file
    ↓
Share → Send .json file to colleague
    ↓
Open → Load .json file
    ↓
Edit → Modify DDF document
    ↓
Save → Update .json file
```

## Yes, You're Correct! ✅

### DDF is Used For:

1. **💾 Saving Projects**
   - Save your work as `.json` files
   - Complete diagram state preserved
   - All shapes, connectors, data, styles saved

2. **🤝 Sharing Projects**
   - Send `.json` files to colleagues
   - Email, cloud storage, version control
   - Cross-platform compatible

3. **📂 Opening Projects**
   - Open `.json` files in the application
   - File → Import DDF Document
   - Restores complete diagram state

4. **📄 Document Format**
   - Human-readable JSON
   - Version control friendly (git)
   - Can be edited in text editor

## Workflow Example

### Scenario: Team Collaboration on Network Diagram

```
Day 1: Alice creates network diagram
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Alice opens whiteboard application
2. Creates network diagram with routers, switches, servers
3. Adds connectors between devices
4. Adds data (IP addresses, hostnames)
5. Saves: File → Export DDF → network_topology.json

File created: network_topology.json
{
  "version": "1.0",
  "metadata": {
    "title": "Office Network Topology",
    "author": "Alice",
    "created": "2025-10-20"
  },
  "shapes": [...],
  "connectors": [...],
  "data": {...}
}


Day 2: Bob reviews and updates
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Alice emails network_topology.json to Bob
2. Bob opens whiteboard application
3. File → Import DDF → Selects network_topology.json
4. Diagram loads with all shapes, connectors, data
5. Bob adds new server, updates IP addresses
6. Saves: File → Export DDF → network_topology_v2.json
7. Sends back to Alice


Day 3: Team meeting
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Alice opens network_topology_v2.json
2. Projects diagram on screen
3. Team discusses changes
4. Alice makes live edits during meeting
5. Saves final version: network_topology_final.json
6. Commits to git repository


Day 4: Version control
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

$ git add network_topology_final.json
$ git commit -m "Updated network topology with new servers"
$ git push

Team members can now:
- Clone repository
- Open .json file in whiteboard
- See the latest diagram
- Make their own changes
- Commit updates
```

## File Operations

### Save (Export)
```
Application → DDF Document → JSON file

User action: File → Export DDF
    ↓
DDFDocument::save_to_json()
    ↓
Serializes all layers:
- Shapes
- Connectors
- Data
- Events
- Styles
    ↓
Writes to file: my_diagram.json
```

### Open (Import)
```
JSON file → DDF Document → Application

User action: File → Import DDF
    ↓
DDFDocument::load_from_json()
    ↓
Parses JSON
    ↓
Loads all layers:
- Shapes (including SVGShape references)
- Connectors
- Data
- Events
- Styles
    ↓
Renders to canvas
```

### Share
```
my_diagram.json
    ↓
Email / Cloud / Git / USB
    ↓
Colleague receives file
    ↓
Opens in whiteboard application
    ↓
Same diagram appears
```

## What Gets Saved in DDF

### ✅ Included in DDF File

```json
{
  "version": "1.0",
  
  "metadata": {
    "title": "My Diagram",
    "author": "User Name",
    "created": "2025-10-20",
    "description": "Project documentation"
  },
  
  "shapes": [
    {
      "id": "shape1",
      "type": "svg",
      "svg_shape_id": "network.router",  // Reference to template
      "svg_parameters": {
        "label": "Main Router",
        "status_color": "#2ecc71"
      },
      "geometry": {"x": 100, "y": 100}
    }
  ],
  
  "connectors": [
    {
      "from": {"shape_id": "shape1"},
      "to": {"shape_id": "shape2"},
      "routing": {"algorithm": "orthogonal"}
    }
  ],
  
  "data": {
    "nodes": [
      {"id": "device1", "properties": {"ip": "192.168.1.1"}}
    ]
  },
  
  "events": [
    {
      "target": "shape1",
      "trigger": "click",
      "actions": [{"type": "show_tooltip"}]
    }
  ],
  
  "styles": {
    "rules": [
      {
        "selector": ".router",
        "properties": {"fill": "#3498db"}
      }
    ]
  }
}
```

### ❌ NOT Included in DDF File

- SVGShape template files (only references)
- Application settings
- User preferences
- Undo/redo history
- Temporary UI state

**Why?** SVGShape templates are in the shape library, shared across all projects.

## Comparison with Other Formats

### DDF vs Traditional Formats

| Format | Purpose | Editable | Shareable | Version Control |
|--------|---------|----------|-----------|-----------------|
| **DDF (.json)** | Project format | ✅ Yes | ✅ Yes | ✅ Yes (text) |
| PNG/JPG | Export image | ❌ No | ✅ Yes | ❌ No (binary) |
| SVG | Export vector | ⚠️ Limited | ✅ Yes | ⚠️ Partial |
| PDF | Export document | ❌ No | ✅ Yes | ❌ No (binary) |

### DDF Advantages

✅ **Fully editable** - Open, modify, save
✅ **Human-readable** - JSON text format
✅ **Version control friendly** - Git diffs work
✅ **Cross-platform** - Works on Windows, Mac, Linux
✅ **Complete state** - Everything preserved
✅ **Shareable** - Just send the .json file

## Use Cases

### 1. Project Documentation
```
Project: Office Network Redesign
Files:
- current_network.json      (As-is state)
- proposed_network.json     (To-be state)
- migration_plan.json       (Transition steps)

Share with team, track changes in git
```

### 2. Process Documentation
```
Project: Customer Onboarding Process
Files:
- onboarding_flowchart.json
- approval_workflow.json
- exception_handling.json

Update as process evolves, version in git
```

### 3. System Architecture
```
Project: Microservices Architecture
Files:
- service_architecture.json
- data_flow.json
- deployment_diagram.json

Collaborate with team, review in PRs
```

### 4. Organization Charts
```
Project: Company Structure
Files:
- org_chart_2025.json
- org_chart_2026_proposed.json

Update quarterly, share with HR
```

## Best Practices

### File Naming
```
✅ Good:
- network_topology_v1.json
- customer_journey_2025-10-20.json
- microservices_architecture_draft.json

❌ Bad:
- diagram.json
- untitled.json
- new_file_1.json
```

### Version Control
```bash
# Initialize git repository
git init

# Add DDF files
git add diagrams/*.json

# Commit with meaningful message
git commit -m "Add initial network topology diagram"

# Create branches for experiments
git checkout -b feature/add-new-datacenter

# Merge when ready
git checkout main
git merge feature/add-new-datacenter
```

### Collaboration
```
1. Share via git repository (recommended)
   - Everyone has latest version
   - Track changes over time
   - Review changes in PRs

2. Share via cloud storage
   - Dropbox, Google Drive, OneDrive
   - Automatic sync
   - Version history

3. Share via email
   - Quick sharing
   - Attach .json file
   - Recipient can open and edit
```

### Backup
```
✅ Keep backups of important diagrams
✅ Use version control (git)
✅ Export to multiple formats (DDF + PNG + PDF)
✅ Store in cloud storage
```

## Integration with Development Workflow

### Example: Architecture Documentation

```bash
# Project structure
my-project/
├── src/
├── docs/
│   └── architecture/
│       ├── system_overview.json      # DDF file
│       ├── data_flow.json            # DDF file
│       └── deployment.json           # DDF file
├── README.md
└── .git/

# Workflow
1. Update architecture diagram (system_overview.json)
2. Commit to git
3. Create PR
4. Team reviews diagram changes
5. Merge to main
6. CI/CD generates PNG from DDF for documentation
```

## Summary

### ✅ Yes, You're Correct!

**DDF is the format for:**
- 💾 **Saving** projects
- 🤝 **Sharing** with colleagues
- 📂 **Opening** in the application
- 📄 **Documenting** systems and processes

**DDF provides:**
- Complete project state
- Human-readable format
- Version control friendly
- Cross-platform compatibility
- Full editability

**Think of DDF like:**
- `.docx` for Word documents
- `.pptx` for PowerPoint presentations
- `.xlsx` for Excel spreadsheets
- `.json` for whiteboard diagrams ← **This is DDF!**

---

**Last Updated**: October 2025
