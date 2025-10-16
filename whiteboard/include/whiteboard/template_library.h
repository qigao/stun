/**
 * \file template_library.h
 * \brief Lightweight repository of reusable whiteboard templates.
 */

#pragma once

#include "whiteboard/common.h"

namespace whiteboard {

/**
 * \struct Template
 * \brief Describes a reusable whiteboard template with metadata and stroke data.
 */
struct Template {
  std::string name;
  std::string category;
  std::string description;
  int preview_image; // NanoVG image handle for preview
  std::vector<Stroke> strokes;

  Template() : preview_image(-1) {}

  Template(const std::string &n, const std::string &cat, const std::string &desc)
      : name(n), category(cat), description(desc), preview_image(-1) {}
};

/**
 * \class TemplateLibrary
 * \brief Singleton that loads, categorizes, and serves whiteboard templates.
 *
 * The library bootstraps with a set of built-in templates and allows callers to
 * register custom templates at runtime. It also exposes helpers for category
 * filtering and preview generation.
 */
class TemplateLibrary {
public:
  static TemplateLibrary &instance() {
    static TemplateLibrary lib;
    return lib;
  }

  void initialize(NVGcontext *vg) {
    if (m_initialized)
      return;
    m_vg = vg;
    create_builtin_templates();
    m_initialized = true;
  }

  const std::vector<Template> &get_templates() const { return m_templates; }

  std::vector<std::string> get_categories() const {
    return {"Flowchart", "Wireframe", "Brainstorm", "Diagram", "Kanban"};
  }

  std::vector<Template> get_templates_by_category(const std::string &category) const {
    std::vector<Template> result;
    for (const auto &tmpl : m_templates) {
      if (tmpl.category == category) {
        result.push_back(tmpl);
      }
    }
    return result;
  }

  void add_custom_template(const Template &tmpl) { m_templates.push_back(tmpl); }

  void save_as_template(const std::string &name, const std::string &category,
                        const std::string &description, const std::vector<Stroke> &strokes) {
    Template tmpl(name, category, description);
    tmpl.strokes = strokes;
    // TODO: Generate preview image
    m_templates.push_back(tmpl);
  }

private:
  TemplateLibrary() : m_initialized(false), m_vg(nullptr) {}

  void create_builtin_templates() {
    // Flowchart templates
    create_basic_flowchart();
    create_decision_tree();

    // Wireframe templates
    create_mobile_wireframe();
    create_web_wireframe();

    // Brainstorm templates
    create_mind_map();
    create_sticky_grid();

    // Diagram templates
    create_network_diagram();
    create_org_chart();

    // Kanban templates
    create_kanban_board();
    create_sprint_board();
  }

  void create_basic_flowchart() {
    Template tmpl("Basic Flowchart", "Flowchart", "Simple process flow with start, steps, and end");

    // Start oval
    Stroke start;
    start.tool = Tool::Circle;
    start.color = Color(100, 200, 100, 255);
    start.fill_style = FillStyle::Solid;
    start.fill_color = Color(200, 255, 200, 255);
    start.width = 2.0f;
    start.points.push_back(Point(200, 100));
    start.points.push_back(Point(300, 150));
    start.text = "Start";
    tmpl.strokes.push_back(start);

    // Arrow down
    Stroke arrow1;
    arrow1.tool = Tool::Arrow;
    arrow1.color = Color(100, 100, 100, 255);
    arrow1.width = 2.0f;
    arrow1.points.push_back(Point(250, 150));
    arrow1.points.push_back(Point(250, 200));
    tmpl.strokes.push_back(arrow1);

    // Process rectangle
    Stroke process;
    process.tool = Tool::Rectangle;
    process.color = Color(100, 150, 255, 255);
    process.fill_style = FillStyle::Solid;
    process.fill_color = Color(200, 220, 255, 255);
    process.width = 2.0f;
    process.points.push_back(Point(150, 200));
    process.points.push_back(Point(350, 260));
    process.text = "Process";
    tmpl.strokes.push_back(process);

    // Arrow down
    Stroke arrow2;
    arrow2.tool = Tool::Arrow;
    arrow2.color = Color(100, 100, 100, 255);
    arrow2.width = 2.0f;
    arrow2.points.push_back(Point(250, 260));
    arrow2.points.push_back(Point(250, 310));
    tmpl.strokes.push_back(arrow2);

    // End oval
    Stroke end;
    end.tool = Tool::Circle;
    end.color = Color(255, 100, 100, 255);
    end.fill_style = FillStyle::Solid;
    end.fill_color = Color(255, 200, 200, 255);
    end.width = 2.0f;
    end.points.push_back(Point(200, 310));
    end.points.push_back(Point(300, 360));
    end.text = "End";
    tmpl.strokes.push_back(end);

    m_templates.push_back(tmpl);
  }

  void create_decision_tree() {
    Template tmpl("Decision Tree", "Flowchart", "Flowchart with decision diamond");

    // Start
    Stroke start;
    start.tool = Tool::Rectangle;
    start.color = Color(100, 200, 100, 255);
    start.fill_style = FillStyle::Solid;
    start.fill_color = Color(200, 255, 200, 255);
    start.width = 2.0f;
    start.points.push_back(Point(200, 50));
    start.points.push_back(Point(300, 100));
    tmpl.strokes.push_back(start);

    // Arrow to decision
    Stroke arrow1;
    arrow1.tool = Tool::Arrow;
    arrow1.color = Color(100, 100, 100, 255);
    arrow1.width = 2.0f;
    arrow1.points.push_back(Point(250, 100));
    arrow1.points.push_back(Point(250, 150));
    tmpl.strokes.push_back(arrow1);

    // Decision diamond (using rotated rectangle)
    Stroke decision;
    decision.tool = Tool::Rectangle;
    decision.color = Color(255, 200, 100, 255);
    decision.fill_style = FillStyle::Solid;
    decision.fill_color = Color(255, 240, 200, 255);
    decision.width = 2.0f;
    decision.rotation = M_PI / 4.0f; // 45 degrees
    decision.points.push_back(Point(200, 150));
    decision.points.push_back(Point(300, 220));
    tmpl.strokes.push_back(decision);

    // Yes branch (left)
    Stroke yes_arrow;
    yes_arrow.tool = Tool::Arrow;
    yes_arrow.color = Color(100, 100, 100, 255);
    yes_arrow.width = 2.0f;
    yes_arrow.points.push_back(Point(200, 185));
    yes_arrow.points.push_back(Point(120, 250));
    tmpl.strokes.push_back(yes_arrow);

    Stroke yes_box;
    yes_box.tool = Tool::Rectangle;
    yes_box.color = Color(100, 200, 255, 255);
    yes_box.fill_style = FillStyle::Solid;
    yes_box.fill_color = Color(200, 230, 255, 255);
    yes_box.width = 2.0f;
    yes_box.points.push_back(Point(50, 250));
    yes_box.points.push_back(Point(150, 300));
    tmpl.strokes.push_back(yes_box);

    // No branch (right)
    Stroke no_arrow;
    no_arrow.tool = Tool::Arrow;
    no_arrow.color = Color(100, 100, 100, 255);
    no_arrow.width = 2.0f;
    no_arrow.points.push_back(Point(300, 185));
    no_arrow.points.push_back(Point(380, 250));
    tmpl.strokes.push_back(no_arrow);

    Stroke no_box;
    no_box.tool = Tool::Rectangle;
    no_box.color = Color(255, 150, 150, 255);
    no_box.fill_style = FillStyle::Solid;
    no_box.fill_color = Color(255, 220, 220, 255);
    no_box.width = 2.0f;
    no_box.points.push_back(Point(350, 250));
    no_box.points.push_back(Point(450, 300));
    tmpl.strokes.push_back(no_box);

    m_templates.push_back(tmpl);
  }

  void create_mobile_wireframe() {
    Template tmpl("Mobile App", "Wireframe", "Basic mobile app layout");

    // Phone frame
    Stroke frame;
    frame.tool = Tool::Rectangle;
    frame.color = Color(100, 100, 100, 255);
    frame.width = 3.0f;
    frame.points.push_back(Point(150, 50));
    frame.points.push_back(Point(350, 550));
    tmpl.strokes.push_back(frame);

    // Header
    Stroke header;
    header.tool = Tool::Rectangle;
    header.color = Color(0, 120, 215, 255);
    header.fill_style = FillStyle::Solid;
    header.fill_color = Color(0, 120, 215, 255);
    header.width = 1.0f;
    header.points.push_back(Point(150, 50));
    header.points.push_back(Point(350, 110));
    tmpl.strokes.push_back(header);

    // Content boxes
    for (int i = 0; i < 3; i++) {
      Stroke box;
      box.tool = Tool::Rectangle;
      box.color = Color(200, 200, 200, 255);
      box.fill_style = FillStyle::Solid;
      box.fill_color = Color(240, 240, 240, 255);
      box.width = 1.0f;
      box.points.push_back(Point(170, 130 + i * 120));
      box.points.push_back(Point(330, 220 + i * 120));
      tmpl.strokes.push_back(box);
    }

    m_templates.push_back(tmpl);
  }

  void create_web_wireframe() {
    Template tmpl("Web Page", "Wireframe", "Basic web page layout");

    // Browser window
    Stroke window;
    window.tool = Tool::Rectangle;
    window.color = Color(100, 100, 100, 255);
    window.width = 2.0f;
    window.points.push_back(Point(50, 50));
    window.points.push_back(Point(550, 450));
    tmpl.strokes.push_back(window);

    // Navigation bar
    Stroke nav;
    nav.tool = Tool::Rectangle;
    nav.color = Color(0, 120, 215, 255);
    nav.fill_style = FillStyle::Solid;
    nav.fill_color = Color(0, 120, 215, 255);
    nav.width = 1.0f;
    nav.points.push_back(Point(50, 50));
    nav.points.push_back(Point(550, 100));
    tmpl.strokes.push_back(nav);

    // Sidebar
    Stroke sidebar;
    sidebar.tool = Tool::Rectangle;
    sidebar.color = Color(150, 150, 150, 255);
    sidebar.fill_style = FillStyle::Solid;
    sidebar.fill_color = Color(220, 220, 220, 255);
    sidebar.width = 1.0f;
    sidebar.points.push_back(Point(50, 100));
    sidebar.points.push_back(Point(150, 450));
    tmpl.strokes.push_back(sidebar);

    // Main content area
    Stroke content;
    content.tool = Tool::Rectangle;
    content.color = Color(200, 200, 200, 255);
    content.fill_style = FillStyle::Solid;
    content.fill_color = Color(245, 245, 245, 255);
    content.width = 1.0f;
    content.points.push_back(Point(150, 100));
    content.points.push_back(Point(550, 450));
    tmpl.strokes.push_back(content);

    m_templates.push_back(tmpl);
  }

  void create_mind_map() {
    Template tmpl("Mind Map", "Brainstorm", "Central idea with branches");

    // Central circle
    Stroke center;
    center.tool = Tool::Circle;
    center.color = Color(255, 150, 0, 255);
    center.fill_style = FillStyle::Solid;
    center.fill_color = Color(255, 200, 100, 255);
    center.width = 3.0f;
    center.points.push_back(Point(220, 220));
    center.points.push_back(Point(280, 280));
    tmpl.strokes.push_back(center);

    // Branch circles and lines
    float angles[] = {0, M_PI / 2, M_PI, 3 * M_PI / 2};
    Color colors[] = {Color(100, 200, 255, 255), Color(255, 100, 150, 255),
                      Color(150, 255, 100, 255), Color(255, 255, 100, 255)};

    for (int i = 0; i < 4; i++) {
      float angle = angles[i];
      float cx = 250 + 120 * std::cos(angle);
      float cy = 250 + 120 * std::sin(angle);

      // Line from center
      Stroke line;
      line.tool = Tool::Line;
      line.color = Color(150, 150, 150, 255);
      line.width = 2.0f;
      line.points.push_back(Point(250, 250));
      line.points.push_back(Point(cx, cy));
      tmpl.strokes.push_back(line);

      // Branch circle
      Stroke branch;
      branch.tool = Tool::Circle;
      branch.color = colors[i];
      branch.fill_style = FillStyle::Solid;
      branch.fill_color =
          Color(colors[i].r() * 0.8f, colors[i].g() * 0.8f, colors[i].b() * 0.8f, 1.0f);
      branch.width = 2.0f;
      branch.points.push_back(Point(cx - 25, cy - 25));
      branch.points.push_back(Point(cx + 25, cy + 25));
      tmpl.strokes.push_back(branch);
    }

    m_templates.push_back(tmpl);
  }

  void create_sticky_grid() {
    Template tmpl("Sticky Grid", "Brainstorm", "Grid of sticky notes");

    Color colors[] = {
        Color(255, 255, 150, 255), // Yellow
        Color(255, 200, 200, 255), // Pink
        Color(200, 255, 200, 255), // Green
        Color(200, 220, 255, 255)  // Blue
    };

    for (int row = 0; row < 3; row++) {
      for (int col = 0; col < 4; col++) {
        Stroke sticky;
        sticky.tool = Tool::Sticky;
        sticky.color = Color(200, 200, 100, 255);
        sticky.fill_style = FillStyle::Solid;
        sticky.fill_color = colors[(row * 4 + col) % 4];
        sticky.width = 1.0f;
        sticky.points.push_back(Point(50 + col * 130, 50 + row * 130));
        sticky.points.push_back(Point(160 + col * 130, 160 + row * 130));
        tmpl.strokes.push_back(sticky);
      }
    }

    m_templates.push_back(tmpl);
  }

  void create_network_diagram() {
    Template tmpl("Network Diagram", "Diagram", "Simple network topology");

    // Server (center)
    Stroke server;
    server.tool = Tool::Rectangle;
    server.color = Color(100, 100, 255, 255);
    server.fill_style = FillStyle::Solid;
    server.fill_color = Color(200, 200, 255, 255);
    server.width = 2.0f;
    server.points.push_back(Point(220, 220));
    server.points.push_back(Point(280, 280));
    tmpl.strokes.push_back(server);

    // Client nodes
    for (int i = 0; i < 4; i++) {
      float angle = i * M_PI / 2;
      float cx = 250 + 150 * std::cos(angle);
      float cy = 250 + 150 * std::sin(angle);

      // Connection line
      Stroke line;
      line.tool = Tool::Line;
      line.color = Color(150, 150, 150, 255);
      line.width = 2.0f;
      line.points.push_back(Point(250, 250));
      line.points.push_back(Point(cx, cy));
      tmpl.strokes.push_back(line);

      // Client circle
      Stroke client;
      client.tool = Tool::Circle;
      client.color = Color(100, 200, 100, 255);
      client.fill_style = FillStyle::Solid;
      client.fill_color = Color(200, 255, 200, 255);
      client.width = 2.0f;
      client.points.push_back(Point(cx - 20, cy - 20));
      client.points.push_back(Point(cx + 20, cy + 20));
      tmpl.strokes.push_back(client);
    }

    m_templates.push_back(tmpl);
  }

  void create_org_chart() {
    Template tmpl("Org Chart", "Diagram", "Organizational hierarchy");

    // CEO (top)
    Stroke ceo;
    ceo.tool = Tool::Rectangle;
    ceo.color = Color(255, 150, 0, 255);
    ceo.fill_style = FillStyle::Solid;
    ceo.fill_color = Color(255, 200, 150, 255);
    ceo.width = 2.0f;
    ceo.points.push_back(Point(200, 50));
    ceo.points.push_back(Point(300, 100));
    tmpl.strokes.push_back(ceo);

    // Lines to managers
    for (int i = 0; i < 3; i++) {
      Stroke line;
      line.tool = Tool::Line;
      line.color = Color(150, 150, 150, 255);
      line.width = 2.0f;
      line.points.push_back(Point(250, 100));
      line.points.push_back(Point(100 + i * 150, 150));
      tmpl.strokes.push_back(line);

      // Manager box
      Stroke manager;
      manager.tool = Tool::Rectangle;
      manager.color = Color(100, 150, 255, 255);
      manager.fill_style = FillStyle::Solid;
      manager.fill_color = Color(200, 220, 255, 255);
      manager.width = 2.0f;
      manager.points.push_back(Point(50 + i * 150, 150));
      manager.points.push_back(Point(150 + i * 150, 200));
      tmpl.strokes.push_back(manager);
    }

    m_templates.push_back(tmpl);
  }

  void create_kanban_board() {
    Template tmpl("Kanban Board", "Kanban", "To Do, In Progress, Done columns");

    const char *columns[] = {"To Do", "In Progress", "Done"};
    Color column_colors[] = {Color(255, 200, 200, 255), Color(255, 255, 200, 255),
                             Color(200, 255, 200, 255)};

    for (int col = 0; col < 3; col++) {
      // Column header
      Stroke header;
      header.tool = Tool::Rectangle;
      header.color = Color(100, 100, 100, 255);
      header.fill_style = FillStyle::Solid;
      header.fill_color = column_colors[col];
      header.width = 2.0f;
      header.points.push_back(Point(50 + col * 180, 50));
      header.points.push_back(Point(210 + col * 180, 100));
      tmpl.strokes.push_back(header);

      // Task cards
      for (int row = 0; row < 3; row++) {
        Stroke card;
        card.tool = Tool::Rectangle;
        card.color = Color(200, 200, 200, 255);
        card.fill_style = FillStyle::Solid;
        card.fill_color = Color(255, 255, 255, 255);
        card.width = 1.0f;
        card.points.push_back(Point(60 + col * 180, 120 + row * 90));
        card.points.push_back(Point(200 + col * 180, 190 + row * 90));
        tmpl.strokes.push_back(card);
      }
    }

    m_templates.push_back(tmpl);
  }

  void create_sprint_board() {
    Template tmpl("Sprint Board", "Kanban", "Sprint planning board with swimlanes");

    // Sprint header
    Stroke header;
    header.tool = Tool::Rectangle;
    header.color = Color(0, 120, 215, 255);
    header.fill_style = FillStyle::Solid;
    header.fill_color = Color(0, 120, 215, 255);
    header.width = 2.0f;
    header.points.push_back(Point(50, 50));
    header.points.push_back(Point(550, 100));
    tmpl.strokes.push_back(header);

    // Swimlanes
    for (int i = 0; i < 3; i++) {
      Stroke lane;
      lane.tool = Tool::Rectangle;
      lane.color = Color(150, 150, 150, 255);
      lane.width = 1.0f;
      lane.points.push_back(Point(50.f, 120.f + i * 100.f));
      lane.points.push_back(Point(550.f, 200.f + i * 100.f));
      tmpl.strokes.push_back(lane);

      // Story cards in lane
      for (int j = 0; j < 3; j++) {
        Stroke card;
        card.tool = Tool::Rectangle;
        card.color = Color(100, 200, 255, 255);
        card.fill_style = FillStyle::Solid;
        card.fill_color = Color(220, 240, 255, 255);
        card.width = 1.0f;
        card.points.push_back(Point(70 + j * 160, 130 + i * 100));
        card.points.push_back(Point(210 + j * 160, 190 + i * 100));
        tmpl.strokes.push_back(card);
      }
    }

    m_templates.push_back(tmpl);
  }

  bool m_initialized;
  NVGcontext *m_vg;
  std::vector<Template> m_templates;
};

} // namespace whiteboard
