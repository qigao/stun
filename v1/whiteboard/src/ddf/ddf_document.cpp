#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <whiteboard/ddf/component_layer.h>
#include <whiteboard/ddf/connector_layer.h>
#include <whiteboard/ddf/data_layer.h>
#include <whiteboard/ddf/ddf_document.h>
#include <whiteboard/ddf/event_layer.h>
#include <whiteboard/ddf/layout_algorithms.h>
#include <whiteboard/ddf/rendering_pipeline.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/style_layer.h>
#include <whiteboard/ddf/stylesheet.h>
#include <whiteboard/ddf/svg_importer.h>
#include <whiteboard/ddf/svg_renderer.h>

namespace whiteboard {
namespace ddf {

using json = nlohmann::json;

// Helper functions for JSON serialization

// Convert RoutingAlgorithm to string
static std::string routing_algorithm_to_string(RoutingAlgorithm algo) {
  switch (algo) {
  case RoutingAlgorithm::Straight:
    return "straight";
  case RoutingAlgorithm::Orthogonal:
    return "orthogonal";
  case RoutingAlgorithm::Bezier:
    return "bezier";
  case RoutingAlgorithm::CurvedOrthogonal:
    return "curved_orthogonal";
  default:
    return "straight";
  }
}

// Convert string to RoutingAlgorithm
static RoutingAlgorithm string_to_routing_algorithm(const std::string &str) {
  if (str == "orthogonal")
    return RoutingAlgorithm::Orthogonal;
  if (str == "bezier")
    return RoutingAlgorithm::Bezier;
  if (str == "curved_orthogonal")
    return RoutingAlgorithm::CurvedOrthogonal;
  return RoutingAlgorithm::Straight;
}

// Convert ArrowType to string
static std::string arrow_type_to_string(ArrowType type) {
  switch (type) {
  case ArrowType::None:
    return "none";
  case ArrowType::Arrow:
    return "arrow";
  case ArrowType::Diamond:
    return "diamond";
  case ArrowType::Circle:
    return "circle";
  case ArrowType::Square:
    return "square";
  default:
    return "none";
  }
}

// Convert string to ArrowType
static ArrowType string_to_arrow_type(const std::string &str) {
  if (str == "arrow")
    return ArrowType::Arrow;
  if (str == "diamond")
    return ArrowType::Diamond;
  if (str == "circle")
    return ArrowType::Circle;
  if (str == "square")
    return ArrowType::Square;
  return ArrowType::None;
}

// Convert EventTrigger to string
static std::string event_trigger_to_string(EventTrigger trigger) {
  switch (trigger) {
  case EventTrigger::Click:
    return "click";
  case EventTrigger::DoubleClick:
    return "double_click";
  case EventTrigger::RightClick:
    return "right_click";
  case EventTrigger::Hover:
    return "hover";
  case EventTrigger::HoverEnd:
    return "hover_end";
  case EventTrigger::DragStart:
    return "drag_start";
  case EventTrigger::Drag:
    return "drag";
  case EventTrigger::DragEnd:
    return "drag_end";
  case EventTrigger::Select:
    return "select";
  case EventTrigger::Deselect:
    return "deselect";
  default:
    return "click";
  }
}

// Convert string to EventTrigger
static EventTrigger string_to_event_trigger(const std::string &str) {
  if (str == "double_click")
    return EventTrigger::DoubleClick;
  if (str == "right_click")
    return EventTrigger::RightClick;
  if (str == "hover")
    return EventTrigger::Hover;
  if (str == "hover_end")
    return EventTrigger::HoverEnd;
  if (str == "drag_start")
    return EventTrigger::DragStart;
  if (str == "drag")
    return EventTrigger::Drag;
  if (str == "drag_end")
    return EventTrigger::DragEnd;
  if (str == "select")
    return EventTrigger::Select;
  if (str == "deselect")
    return EventTrigger::Deselect;
  return EventTrigger::Click;
}

// Convert ActionType to string
static std::string action_type_to_string(ActionType type) {
  switch (type) {
  case ActionType::UpdateData:
    return "update_data";
  case ActionType::UpdateStyle:
    return "update_style";
  case ActionType::UpdateGeometry:
    return "update_geometry";
  case ActionType::ShowTooltip:
    return "show_tooltip";
  case ActionType::Navigate:
    return "navigate";
  case ActionType::ExecuteScript:
    return "execute_script";
  case ActionType::EmitCustomEvent:
    return "emit_custom_event";
  default:
    return "update_data";
  }
}

// Convert string to ActionType
static ActionType string_to_action_type(const std::string &str) {
  if (str == "update_style")
    return ActionType::UpdateStyle;
  if (str == "update_geometry")
    return ActionType::UpdateGeometry;
  if (str == "show_tooltip")
    return ActionType::ShowTooltip;
  if (str == "navigate")
    return ActionType::Navigate;
  if (str == "execute_script")
    return ActionType::ExecuteScript;
  if (str == "emit_custom_event")
    return ActionType::EmitCustomEvent;
  return ActionType::UpdateData;
}

DDFDocument::DDFDocument()
    : data_layer_(std::make_unique<DataLayer>()), shape_layer_(std::make_unique<ShapeLayer>()),
      component_layer_(std::make_unique<ComponentLayer>()),
      connector_layer_(std::make_unique<ConnectorLayer>()),
      event_layer_(std::make_unique<EventLayer>()), style_layer_(std::make_unique<StyleLayer>()),
      rendering_pipeline_(nullptr), version_("1.0") {}

DDFDocument::~DDFDocument() = default;

DataLayer &DDFDocument::data_layer() { return *data_layer_; }

const DataLayer &DDFDocument::data_layer() const { return *data_layer_; }

ShapeLayer &DDFDocument::shape_layer() { return *shape_layer_; }

const ShapeLayer &DDFDocument::shape_layer() const { return *shape_layer_; }

ComponentLayer &DDFDocument::component_layer() { return *component_layer_; }

const ComponentLayer &DDFDocument::component_layer() const { return *component_layer_; }

ConnectorLayer &DDFDocument::connector_layer() { return *connector_layer_; }

const ConnectorLayer &DDFDocument::connector_layer() const { return *connector_layer_; }

EventLayer &DDFDocument::event_layer() { return *event_layer_; }

const EventLayer &DDFDocument::event_layer() const { return *event_layer_; }

StyleLayer &DDFDocument::style_layer() { return *style_layer_; }

const StyleLayer &DDFDocument::style_layer() const { return *style_layer_; }

bool DDFDocument::load_from_json(const std::string &json_string) {
  try {
    std::cout << "DDFDocument::load_from_json: Parsing JSON..." << std::endl;
    json doc = json::parse(json_string);
    std::cout << "DDFDocument::load_from_json: JSON parsed successfully" << std::endl;

    // Clear existing data
    data_layer_->clear();
    shape_layer_->clear();
    component_layer_->clear();
    connector_layer_->clear();
    event_layer_->clear();
    style_layer_->clear_all_pseudo_states();

    std::cout << "DDFDocument::load_from_json: Cleared existing data" << std::endl;

    // Load version and metadata
    if (doc.contains("version")) {
      version_ = doc["version"].get<std::string>();
    }

    if (doc.contains("metadata")) {
      metadata_ = doc["metadata"].get<std::map<std::string, std::string>>();
    }

    // Load data layer
    if (doc.contains("data")) {
      const auto &data_json = doc["data"];

      // Load nodes
      if (data_json.contains("nodes")) {
        for (const auto &node_json : data_json["nodes"]) {
          DataNode node;
          node.id = node_json["id"].get<std::string>();
          node.type = node_json["type"].get<std::string>();
          if (node_json.contains("properties")) {
            node.properties = node_json["properties"].get<std::map<std::string, std::string>>();
          }
          data_layer_->add_node(node);
        }
      }

      // Load relationships
      if (data_json.contains("relationships")) {
        for (const auto &rel_json : data_json["relationships"]) {
          DataRelationship rel;
          rel.id = rel_json["id"].get<std::string>();
          rel.type = rel_json["type"].get<std::string>();
          rel.from_node_id = rel_json["from"].get<std::string>();
          rel.to_node_id = rel_json["to"].get<std::string>();
          if (rel_json.contains("properties")) {
            rel.properties = rel_json["properties"].get<std::map<std::string, std::string>>();
          }
          data_layer_->add_relationship(rel);
        }
      }
    }

    // Load shape layer
    if (doc.contains("shapes")) {
      for (const auto &shape_json : doc["shapes"]) {
        Shape shape;
        shape.id = shape_json["id"].get<std::string>();
        shape.type = shape_json["type"].get<std::string>();

        if (shape_json.contains("data_binding")) {
          const auto &binding_json = shape_json["data_binding"];
          shape.has_data_binding = true;
          shape.data_binding.node_id = binding_json["node_id"].get<std::string>();
          if (binding_json.contains("property_mappings")) {
            shape.data_binding.property_mappings =
                binding_json["property_mappings"].get<std::map<std::string, std::string>>();
          }
        }

        // Handle both "class" (string) and "classes" (array)
        if (shape_json.contains("class")) {
          shape.classes.push_back(shape_json["class"].get<std::string>());
        }
        if (shape_json.contains("classes")) {
          auto classes = shape_json["classes"].get<std::vector<std::string>>();
          shape.classes.insert(shape.classes.end(), classes.begin(), classes.end());
        }

        if (shape_json.contains("style")) {
          shape.inline_style = shape_json["style"].get<std::map<std::string, std::string>>();
        }

        if (shape_json.contains("geometry")) {
          shape.geometry = shape_json["geometry"].get<std::map<std::string, float>>();
        }

        if (shape_json.contains("connection_points")) {
          for (const auto &cp_json : shape_json["connection_points"]) {
            ConnectionPoint cp;
            cp.id = cp_json["id"].get<std::string>();
            // Support both "x"/"y" and "x_ratio"/"y_ratio"
            if (cp_json.contains("x_ratio")) {
              cp.x_ratio = cp_json["x_ratio"].get<float>();
            } else if (cp_json.contains("x")) {
              cp.x_ratio = cp_json["x"].get<float>();
            }
            if (cp_json.contains("y_ratio")) {
              cp.y_ratio = cp_json["y_ratio"].get<float>();
            } else if (cp_json.contains("y")) {
              cp.y_ratio = cp_json["y"].get<float>();
            }
            shape.connection_points.push_back(cp);
          }
        }

        if (shape_json.contains("transform")) {
          shape.transform = shape_json["transform"].get<std::vector<float>>();
        }

        if (shape_json.contains("children")) {
          shape.child_shape_ids = shape_json["children"].get<std::vector<std::string>>();
        }

        if (shape_json.contains("parent")) {
          shape.parent_shape_id = shape_json["parent"].get<std::string>();
        }

        if (shape_json.contains("pseudo_states")) {
          shape.pseudo_states = shape_json["pseudo_states"].get<std::set<std::string>>();
        }

        if (shape_json.contains("text")) {
          shape.text = shape_json["text"].get<std::string>();
        }
        
        // SVG shape support
        if (shape_json.contains("svg_shape_id")) {
          shape.svg_shape_id = shape_json["svg_shape_id"].get<std::string>();
        }
        if (shape_json.contains("svg_data")) {
          shape.svg_data = shape_json["svg_data"].get<std::string>();
        }
        if (shape_json.contains("svg_parameters")) {
          shape.svg_parameters = shape_json["svg_parameters"].get<std::map<std::string, std::string>>();
        }

        shape_layer_->add_shape(shape);
      }
    }

    // Load component layer
    if (doc.contains("components")) {
      const auto &components_json = doc["components"];

      // Load definitions
      if (components_json.contains("definitions")) {
        for (const auto &comp_json : components_json["definitions"]) {
          ComponentDefinition comp;
          comp.id = comp_json["id"].get<std::string>();
          comp.name = comp_json["name"].get<std::string>();

          if (comp_json.contains("parameters")) {
            for (const auto &param_json : comp_json["parameters"]) {
              ComponentParameter param;
              param.name = param_json["name"].get<std::string>();
              param.type = param_json["type"].get<std::string>();
              param.default_value = param_json["default"].get<std::string>();
              comp.parameters.push_back(param);
            }
          }

          if (comp_json.contains("shapes")) {
            int shape_index = 0;
            for (const auto &shape_json : comp_json["shapes"]) {
              Shape shape;
              // ID is optional for component shapes - generate if missing
              if (shape_json.contains("id")) {
                shape.id = shape_json["id"].get<std::string>();
              } else {
                shape.id = comp.id + "_shape_" + std::to_string(shape_index++);
              }
              shape.type = shape_json["type"].get<std::string>();

              // Handle both "class" (string) and "classes" (array)
              if (shape_json.contains("class")) {
                shape.classes.push_back(shape_json["class"].get<std::string>());
              }
              if (shape_json.contains("classes")) {
                auto classes = shape_json["classes"].get<std::vector<std::string>>();
                shape.classes.insert(shape.classes.end(), classes.begin(), classes.end());
              }

              if (shape_json.contains("style")) {
                shape.inline_style = shape_json["style"].get<std::map<std::string, std::string>>();
              }
              if (shape_json.contains("geometry")) {
                shape.geometry = shape_json["geometry"].get<std::map<std::string, float>>();
              }
              if (shape_json.contains("text")) {
                shape.text = shape_json["text"].get<std::string>();
              }
              // SVG shape support
              if (shape_json.contains("svg_shape_id")) {
                shape.svg_shape_id = shape_json["svg_shape_id"].get<std::string>();
              }
              if (shape_json.contains("svg_data")) {
                shape.svg_data = shape_json["svg_data"].get<std::string>();
              }
              if (shape_json.contains("svg_parameters")) {
                shape.svg_parameters = shape_json["svg_parameters"].get<std::map<std::string, std::string>>();
              }
              comp.shapes.push_back(shape);
            }
          }

          if (comp_json.contains("connection_points")) {
            for (const auto &cp_json : comp_json["connection_points"]) {
              ConnectionPoint cp;
              cp.id = cp_json["id"].get<std::string>();
              // Support both "x"/"y" and "x_ratio"/"y_ratio"
              if (cp_json.contains("x_ratio")) {
                cp.x_ratio = cp_json["x_ratio"].get<float>();
              } else if (cp_json.contains("x")) {
                cp.x_ratio = cp_json["x"].get<float>();
              }
              if (cp_json.contains("y_ratio")) {
                cp.y_ratio = cp_json["y_ratio"].get<float>();
              } else if (cp_json.contains("y")) {
                cp.y_ratio = cp_json["y"].get<float>();
              }
              comp.connection_points.push_back(cp);
            }
          }

          if (comp_json.contains("parent")) {
            comp.parent_component_id = comp_json["parent"].get<std::string>();
          }

          component_layer_->register_component(comp);
        }
      }

      // Load instances
      if (components_json.contains("instances")) {
        for (const auto &inst_json : components_json["instances"]) {
          ComponentInstance inst;
          inst.id = inst_json["id"].get<std::string>();
          inst.component_id = inst_json["component_id"].get<std::string>();
          inst.parameters = inst_json["parameters"].get<std::map<std::string, std::string>>();

          if (inst_json.contains("position") && inst_json["position"].is_array() &&
              inst_json["position"].size() >= 2) {
            inst.position_x = inst_json["position"][0].get<float>();
            inst.position_y = inst_json["position"][1].get<float>();
          }

          if (inst_json.contains("overrides")) {
            inst.overrides = inst_json["overrides"].get<std::map<std::string, std::string>>();
          }

          if (inst_json.contains("detached")) {
            inst.detached = inst_json["detached"].get<bool>();
          }

          if (inst_json.contains("generated_shapes")) {
            inst.generated_shape_ids =
                inst_json["generated_shapes"].get<std::vector<std::string>>();
          }

          // Manually add instance (bypassing create_instance which would generate shapes)
          // This is a bit of a hack - ideally ComponentLayer would have an add_instance method
          component_layer_->create_instance(inst.component_id, inst.parameters, inst.position_x,
                                            inst.position_y);
        }
      }
    }

    // Load connector layer
    if (doc.contains("connectors")) {
      int connector_index = 0;
      for (const auto &conn_json : doc["connectors"]) {
        // Skip connectors without from/to (they're templates/defaults)
        if (!conn_json.contains("from") || !conn_json.contains("to")) {
          continue;
        }

        Connector conn;
        // ID is optional - generate if missing
        if (conn_json.contains("id")) {
          conn.id = conn_json["id"].get<std::string>();
        } else {
          conn.id = "connector_" + std::to_string(connector_index++);
        }

        if (conn_json.contains("from")) {
          conn.from.shape_id = conn_json["from"]["shape_id"].get<std::string>();
          conn.from.connection_point_id = conn_json["from"]["connection_point"].get<std::string>();
        }

        if (conn_json.contains("to")) {
          conn.to.shape_id = conn_json["to"]["shape_id"].get<std::string>();
          conn.to.connection_point_id = conn_json["to"]["connection_point"].get<std::string>();
        }

        if (conn_json.contains("routing")) {
          const auto &routing_json = conn_json["routing"];
          if (routing_json.contains("algorithm")) {
            conn.routing.algorithm =
                string_to_routing_algorithm(routing_json["algorithm"].get<std::string>());
          }
          if (routing_json.contains("avoid_shapes")) {
            conn.routing.avoid_shapes = routing_json["avoid_shapes"].get<bool>();
          }
          if (routing_json.contains("padding")) {
            conn.routing.padding = routing_json["padding"].get<float>();
          }
          if (routing_json.contains("corner_radius")) {
            conn.routing.corner_radius = routing_json["corner_radius"].get<float>();
          }
        }

        if (conn_json.contains("style")) {
          conn.style = conn_json["style"].get<std::map<std::string, std::string>>();
        }

        if (conn_json.contains("arrow_start")) {
          conn.arrow_start = string_to_arrow_type(conn_json["arrow_start"].get<std::string>());
        }

        if (conn_json.contains("arrow_end")) {
          conn.arrow_end = string_to_arrow_type(conn_json["arrow_end"].get<std::string>());
        }

        if (conn_json.contains("label")) {
          const auto &label_json = conn_json["label"];
          ConnectorLabel label;
          label.text = label_json["text"].get<std::string>();
          if (label_json.contains("position")) {
            label.position = label_json["position"].get<float>();
          }
          if (label_json.contains("offset") && label_json["offset"].is_array() &&
              label_json["offset"].size() >= 2) {
            label.offset_x = label_json["offset"][0].get<float>();
            label.offset_y = label_json["offset"][1].get<float>();
          }
          conn.label = label;
        }

        connector_layer_->add_connector(conn);
      }
    }

    // Load event layer
    if (doc.contains("events")) {
      for (const auto &event_json : doc["events"]) {
        Event event;
        event.id = event_json["id"].get<std::string>();
        event.target_id = event_json["target"].get<std::string>();
        event.trigger = string_to_event_trigger(event_json["trigger"].get<std::string>());

        if (event_json.contains("actions")) {
          for (const auto &action_json : event_json["actions"]) {
            EventAction action;
            action.type = string_to_action_type(action_json["type"].get<std::string>());
            if (action_json.contains("parameters")) {
              action.parameters =
                  action_json["parameters"].get<std::map<std::string, std::string>>();
            }
            event.actions.push_back(action);
          }
        }

        if (event_json.contains("condition")) {
          event.condition = event_json["condition"].get<std::string>();
        }

        event_layer_->register_event(event);
      }
    }

    // Load style layer
    if (doc.contains("styles")) {
      auto stylesheet = std::make_unique<StyleSheet>();
      
      // Support CSS text (preferred - uses Lexbor for full CSS3 support)
      if (doc["styles"].contains("css")) {
        std::string css_text = doc["styles"]["css"].get<std::string>();
        if (!stylesheet->parse_css(css_text)) {
          std::cerr << "DDFDocument::load_from_json: Warning - CSS parsing failed" << std::endl;
        }
      }
      
      // Support individual rules (legacy format)
      if (doc["styles"].contains("rules")) {
        for (const auto &rule_json : doc["styles"]["rules"]) {
          std::string selector = rule_json["selector"].get<std::string>();
          auto properties = rule_json["properties"].get<std::map<std::string, std::string>>();
          stylesheet->add_rule(selector, properties);
        }
      }
      
      style_layer_->add_stylesheet("default", std::move(stylesheet));
      
      // Compute styles for all shapes using the loaded stylesheet
      for (auto *shape : shape_layer_->get_all_shapes()) {
        auto computed_style = style_layer_->compute_style_for_shape(*shape);
        // Store computed style in shape (if Shape has a computed_style field)
        // For now, the style will be computed on-demand during rendering
      }
    }

    std::cout << "DDFDocument::load_from_json: Successfully loaded all sections" << std::endl;
    return true;
  } catch (const json::exception &e) {
    // JSON parsing error
    std::cerr << "DDFDocument::load_from_json: JSON exception: " << e.what() << std::endl;
    std::cerr << "  Exception ID: " << e.id << std::endl;
    return false;
  } catch (const std::exception &e) {
    // Other error
    std::cerr << "DDFDocument::load_from_json: Exception: " << e.what() << std::endl;
    return false;
  }
}

std::string DDFDocument::save_to_json() const {
  json doc;

  // Version and metadata
  doc["version"] = version_;
  doc["metadata"] = metadata_;

  // Serialize data layer
  json data_json;
  json nodes_array = json::array();
  for (const auto *node : data_layer_->get_all_nodes()) {
    json node_json;
    node_json["id"] = node->id;
    node_json["type"] = node->type;
    node_json["properties"] = node->properties;
    nodes_array.push_back(node_json);
  }
  data_json["nodes"] = nodes_array;

  json relationships_array = json::array();
  for (const auto *node : data_layer_->get_all_nodes()) {
    for (const auto *rel : data_layer_->get_relationships_for_node(node->id)) {
      // Only add each relationship once (when we encounter the from_node)
      if (rel->from_node_id == node->id) {
        json rel_json;
        rel_json["id"] = rel->id;
        rel_json["type"] = rel->type;
        rel_json["from"] = rel->from_node_id;
        rel_json["to"] = rel->to_node_id;
        rel_json["properties"] = rel->properties;
        relationships_array.push_back(rel_json);
      }
    }
  }
  data_json["relationships"] = relationships_array;
  doc["data"] = data_json;

  // Serialize shape layer
  json shapes_array = json::array();
  for (const auto *shape : shape_layer_->get_all_shapes()) {
    json shape_json;
    shape_json["id"] = shape->id;
    shape_json["type"] = shape->type;

    if (shape->has_data_binding) {
      json binding_json;
      binding_json["node_id"] = shape->data_binding.node_id;
      binding_json["property_mappings"] = shape->data_binding.property_mappings;
      shape_json["data_binding"] = binding_json;
    }

    if (!shape->classes.empty()) {
      shape_json["classes"] = shape->classes;
    }

    if (!shape->inline_style.empty()) {
      shape_json["style"] = shape->inline_style;
    }

    if (!shape->geometry.empty()) {
      shape_json["geometry"] = shape->geometry;
    }

    if (!shape->connection_points.empty()) {
      json conn_points_array = json::array();
      for (const auto &cp : shape->connection_points) {
        json cp_json;
        cp_json["id"] = cp.id;
        cp_json["x_ratio"] = cp.x_ratio;
        cp_json["y_ratio"] = cp.y_ratio;
        conn_points_array.push_back(cp_json);
      }
      shape_json["connection_points"] = conn_points_array;
    }

    if (!shape->transform.empty()) {
      shape_json["transform"] = shape->transform;
    }

    if (!shape->child_shape_ids.empty()) {
      shape_json["children"] = shape->child_shape_ids;
    }

    if (!shape->parent_shape_id.empty()) {
      shape_json["parent"] = shape->parent_shape_id;
    }

    if (!shape->pseudo_states.empty()) {
      shape_json["pseudo_states"] = shape->pseudo_states;
    }

    if (!shape->text.empty()) {
      shape_json["text"] = shape->text;
    }

    shapes_array.push_back(shape_json);
  }
  doc["shapes"] = shapes_array;

  // Serialize component layer
  json components_json;
  json definitions_array = json::array();
  for (const auto *comp : component_layer_->get_all_components()) {
    json comp_json;
    comp_json["id"] = comp->id;
    comp_json["name"] = comp->name;

    json params_array = json::array();
    for (const auto &param : comp->parameters) {
      json param_json;
      param_json["name"] = param.name;
      param_json["type"] = param.type;
      param_json["default"] = param.default_value;
      params_array.push_back(param_json);
    }
    comp_json["parameters"] = params_array;

    // Serialize component shapes (similar to shape layer)
    json comp_shapes_array = json::array();
    for (const auto &shape : comp->shapes) {
      json shape_json;
      shape_json["id"] = shape.id;
      shape_json["type"] = shape.type;
      if (!shape.inline_style.empty())
        shape_json["style"] = shape.inline_style;
      if (!shape.geometry.empty())
        shape_json["geometry"] = shape.geometry;
      if (!shape.text.empty())
        shape_json["text"] = shape.text;
      comp_shapes_array.push_back(shape_json);
    }
    comp_json["shapes"] = comp_shapes_array;

    if (!comp->connection_points.empty()) {
      json conn_points_array = json::array();
      for (const auto &cp : comp->connection_points) {
        json cp_json;
        cp_json["id"] = cp.id;
        cp_json["x_ratio"] = cp.x_ratio;
        cp_json["y_ratio"] = cp.y_ratio;
        conn_points_array.push_back(cp_json);
      }
      comp_json["connection_points"] = conn_points_array;
    }

    if (!comp->parent_component_id.empty()) {
      comp_json["parent"] = comp->parent_component_id;
    }

    definitions_array.push_back(comp_json);
  }
  components_json["definitions"] = definitions_array;

  json instances_array = json::array();
  for (const auto *inst : component_layer_->get_all_instances()) {
    json inst_json;
    inst_json["id"] = inst->id;
    inst_json["component_id"] = inst->component_id;
    inst_json["parameters"] = inst->parameters;
    inst_json["position"] = {inst->position_x, inst->position_y};
    if (!inst->overrides.empty()) {
      inst_json["overrides"] = inst->overrides;
    }
    if (inst->detached) {
      inst_json["detached"] = true;
    }
    if (!inst->generated_shape_ids.empty()) {
      inst_json["generated_shapes"] = inst->generated_shape_ids;
    }
    instances_array.push_back(inst_json);
  }
  components_json["instances"] = instances_array;
  doc["components"] = components_json;

  // Serialize connector layer
  json connectors_array = json::array();
  for (const auto *conn : connector_layer_->get_all_connectors()) {
    json conn_json;
    conn_json["id"] = conn->id;

    json from_json;
    from_json["shape_id"] = conn->from.shape_id;
    from_json["connection_point"] = conn->from.connection_point_id;
    conn_json["from"] = from_json;

    json to_json;
    to_json["shape_id"] = conn->to.shape_id;
    to_json["connection_point"] = conn->to.connection_point_id;
    conn_json["to"] = to_json;

    json routing_json;
    routing_json["algorithm"] = routing_algorithm_to_string(conn->routing.algorithm);
    routing_json["avoid_shapes"] = conn->routing.avoid_shapes;
    routing_json["padding"] = conn->routing.padding;
    routing_json["corner_radius"] = conn->routing.corner_radius;
    conn_json["routing"] = routing_json;

    if (!conn->style.empty()) {
      conn_json["style"] = conn->style;
    }

    conn_json["arrow_start"] = arrow_type_to_string(conn->arrow_start);
    conn_json["arrow_end"] = arrow_type_to_string(conn->arrow_end);

    if (conn->label.has_value()) {
      json label_json;
      label_json["text"] = conn->label->text;
      label_json["position"] = conn->label->position;
      label_json["offset"] = {conn->label->offset_x, conn->label->offset_y};
      conn_json["label"] = label_json;
    }

    connectors_array.push_back(conn_json);
  }
  doc["connectors"] = connectors_array;

  // Serialize event layer
  json events_array = json::array();
  for (const auto *event : event_layer_->get_all_events()) {
    json event_json;
    event_json["id"] = event->id;
    event_json["target"] = event->target_id;
    event_json["trigger"] = event_trigger_to_string(event->trigger);

    json actions_array = json::array();
    for (const auto &action : event->actions) {
      json action_json;
      action_json["type"] = action_type_to_string(action.type);
      action_json["parameters"] = action.parameters;
      actions_array.push_back(action_json);
    }
    event_json["actions"] = actions_array;

    if (event->condition.has_value()) {
      event_json["condition"] = event->condition.value();
    }

    events_array.push_back(event_json);
  }
  doc["events"] = events_array;

  // Serialize style layer (stylesheets)
  json styles_json;
  json rules_array = json::array();
  for (const auto &[id, stylesheet] : style_layer_->get_stylesheets()) {
    for (const auto &rule : stylesheet->get_rules()) {
      json rule_json;
      rule_json["selector"] = rule.selector;
      rule_json["properties"] = rule.properties;
      rules_array.push_back(rule_json);
    }
  }
  styles_json["rules"] = rules_array;
  doc["styles"] = styles_json;

  return doc.dump(2); // Pretty print with 2-space indentation
}

bool DDFDocument::load_from_file(const std::string &filepath) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "DDFDocument::load_from_file: Failed to open file: " << filepath << std::endl;
      return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string json_str = buffer.str();
    std::cout << "DDFDocument::load_from_file: Read " << json_str.size() << " bytes from file"
              << std::endl;

    bool result = load_from_json(json_str);
    if (!result) {
      std::cerr << "DDFDocument::load_from_file: load_from_json failed" << std::endl;
    }
    return result;
  } catch (const std::exception &e) {
    std::cerr << "DDFDocument::load_from_file: Exception: " << e.what() << std::endl;
    return false;
  }
}

bool DDFDocument::save_to_file(const std::string &filepath) const {
  try {
    std::ofstream file(filepath);
    if (!file.is_open()) {
      return false;
    }

    std::string json_str = save_to_json();
    file << json_str;
    file.close();

    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

std::string DDFDocument::export_to_svg() const {
  SVGRenderer renderer;
  return renderer.render(*this);
}

bool DDFDocument::export_to_svg_file(const std::string &filepath) const {
  try {
    std::string svg_content = export_to_svg();

    std::ofstream file(filepath);
    if (!file.is_open()) {
      return false;
    }

    file << svg_content;
    file.close();

    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

bool DDFDocument::import_from_svg(const std::string &svg_string) {
  try {
    SVGImporter importer;
    bool success = importer.import(svg_string, *this);

    if (!success) {
      // Log error if needed: importer.get_last_error()
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    // Handle any exceptions during import
    return false;
  }
}

void DDFDocument::generate_from_data(const std::string &layout_algorithm) {
  // Clear existing shapes and connectors
  shape_layer_->clear();
  connector_layer_->clear();

  // Get all data nodes
  auto nodes = data_layer_->get_all_nodes();
  if (nodes.empty()) {
    return;
  }

  // Compute layout based on algorithm
  LayoutResult layout_result;

  if (layout_algorithm == "tree") {
    TreeLayout tree_layout;
    layout_result = tree_layout.compute(*data_layer_);
  } else if (layout_algorithm == "force-directed" || layout_algorithm == "force_directed") {
    ForceDirectedLayout force_layout;
    layout_result = force_layout.compute(*data_layer_);
  } else if (layout_algorithm == "grid") {
    GridLayout grid_layout;
    layout_result = grid_layout.compute(*data_layer_);
  } else {
    // Default to grid layout
    GridLayout grid_layout;
    layout_result = grid_layout.compute(*data_layer_);
  }

  // Create shapes for each data node
  for (const auto *node : nodes) {
    auto pos_it = layout_result.positions.find(node->id);
    if (pos_it == layout_result.positions.end()) {
      continue;
    }

    const auto &pos = pos_it->second;

    // Create a shape for this node
    Shape shape;
    shape.id = "shape_" + node->id;
    shape.type = "rect";

    // Set geometry
    shape.geometry["x"] = pos.x;
    shape.geometry["y"] = pos.y;
    shape.geometry["width"] = 120.0f;
    shape.geometry["height"] = 60.0f;

    // Set default style
    shape.inline_style["fill"] = "#3498db";
    shape.inline_style["stroke"] = "#2c3e50";
    shape.inline_style["stroke-width"] = "2";

    // Bind to data
    shape.has_data_binding = true;
    shape.data_binding.node_id = node->id;

    // Add connection points
    shape.connection_points = {
        {"top", 0.5f, 0.0f}, {"bottom", 0.5f, 1.0f}, {"left", 0.0f, 0.5f}, {"right", 1.0f, 0.5f}};

    shape_layer_->add_shape(shape);

    // Create text label if node has a name property
    if (node->properties.count("name") > 0) {
      Shape text_shape;
      text_shape.id = "text_" + node->id;
      text_shape.type = "text";
      text_shape.text = node->properties.at("name");

      // Center text in shape
      text_shape.geometry["x"] = pos.x + 60.0f;
      text_shape.geometry["y"] = pos.y + 30.0f;

      text_shape.inline_style["font-size"] = "14";
      text_shape.inline_style["text-anchor"] = "middle";
      text_shape.inline_style["fill"] = "#ffffff";

      shape_layer_->add_shape(text_shape);
    }
  }

  // Create connectors for relationships
  auto all_relationships = data_layer_->get_all_nodes();
  for (const auto *node : all_relationships) {
    auto relationships = data_layer_->get_relationships_for_node(node->id);

    for (const auto *rel : relationships) {
      // Only create connector if this node is the source
      if (rel->from_node_id != node->id) {
        continue;
      }

      // Check if both shapes exist
      std::string from_shape_id = "shape_" + rel->from_node_id;
      std::string to_shape_id = "shape_" + rel->to_node_id;

      if (!shape_layer_->get_shape(from_shape_id) || !shape_layer_->get_shape(to_shape_id)) {
        continue;
      }

      // Create connector
      Connector connector;
      connector.id = "connector_" + rel->id;
      connector.from.shape_id = from_shape_id;
      connector.from.connection_point_id = "bottom";
      connector.to.shape_id = to_shape_id;
      connector.to.connection_point_id = "top";

      // Set routing based on layout algorithm
      if (layout_algorithm == "tree") {
        connector.routing.algorithm = RoutingAlgorithm::Straight;
      } else if (layout_algorithm == "force-directed" || layout_algorithm == "force_directed") {
        connector.routing.algorithm = RoutingAlgorithm::Bezier;
      } else {
        connector.routing.algorithm = RoutingAlgorithm::Orthogonal;
      }

      connector.arrow_end = ArrowType::Arrow;
      connector.style["stroke"] = "#2c3e50";
      connector.style["stroke-width"] = "2";

      connector_layer_->add_connector(connector);
    }
  }

  // Compute all connector paths
  connector_layer_->recompute_all_paths();
}

void DDFDocument::render(NVGcontext *ctx, float viewport_x, float viewport_y, float viewport_width,
                         float viewport_height) {
  if (!rendering_pipeline_) {
    rendering_pipeline_ = std::make_unique<RenderingPipeline>(ctx);
    // Set shape library if it was set before pipeline was created
    if (svg_shape_library_) {
      rendering_pipeline_->set_shape_library(svg_shape_library_);
    }
  }
  rendering_pipeline_->set_viewport(viewport_x, viewport_y, viewport_width, viewport_height);
  rendering_pipeline_->render(*this);
}

void DDFDocument::set_svg_shape_library(SVGShapeLibrary* library) {
  svg_shape_library_ = library;
  if (rendering_pipeline_) {
    rendering_pipeline_->set_shape_library(library);
  }
}

bool DDFDocument::validate() const { return get_validation_errors().empty(); }

std::vector<std::string> DDFDocument::get_validation_errors() const {
  std::vector<std::string> errors;

  // 1. Validate shape references and hierarchy
  for (const auto *shape : shape_layer_->get_all_shapes()) {
    // Check parent reference
    if (!shape->parent_shape_id.empty()) {
      const auto *parent = shape_layer_->get_shape(shape->parent_shape_id);
      if (!parent) {
        errors.push_back("Shape '" + shape->id + "' references non-existent parent '" +
                         shape->parent_shape_id + "'");
      }
    }

    // Check child references
    for (const auto &child_id : shape->child_shape_ids) {
      const auto *child = shape_layer_->get_shape(child_id);
      if (!child) {
        errors.push_back("Shape '" + shape->id + "' references non-existent child '" + child_id +
                         "'");
      } else if (child->parent_shape_id != shape->id) {
        errors.push_back("Shape '" + shape->id + "' lists '" + child_id +
                         "' as child, but child's parent is '" + child->parent_shape_id + "'");
      }
    }

    // Check data binding references
    if (shape->has_data_binding) {
      const auto *node = data_layer_->get_node(shape->data_binding.node_id);
      if (!node) {
        errors.push_back("Shape '" + shape->id + "' binds to non-existent data node '" +
                         shape->data_binding.node_id + "'");
      }
    }
  }

  // 2. Check for circular dependencies in shape hierarchy
  for (const auto *shape : shape_layer_->get_all_shapes()) {
    std::set<std::string> visited;
    const Shape *current = shape;
    visited.insert(current->id);

    while (!current->parent_shape_id.empty()) {
      current = shape_layer_->get_shape(current->parent_shape_id);
      if (!current)
        break; // Already reported as error above

      if (visited.count(current->id)) {
        errors.push_back("Circular dependency detected in shape hierarchy involving '" + shape->id +
                         "'");
        break;
      }
      visited.insert(current->id);
    }
  }

  // 3. Validate data relationships
  for (const auto *node : data_layer_->get_all_nodes()) {
    for (const auto *rel : data_layer_->get_relationships_for_node(node->id)) {
      // Check from_node reference
      if (!data_layer_->get_node(rel->from_node_id)) {
        errors.push_back("Relationship '" + rel->id + "' references non-existent from_node '" +
                         rel->from_node_id + "'");
      }

      // Check to_node reference
      if (!data_layer_->get_node(rel->to_node_id)) {
        errors.push_back("Relationship '" + rel->id + "' references non-existent to_node '" +
                         rel->to_node_id + "'");
      }
    }
  }

  // 4. Validate connectors
  for (const auto *conn : connector_layer_->get_all_connectors()) {
    // Check from shape reference
    const auto *from_shape = shape_layer_->get_shape(conn->from.shape_id);
    if (!from_shape) {
      errors.push_back("Connector '" + conn->id + "' references non-existent from_shape '" +
                       conn->from.shape_id + "'");
    } else {
      // Check from connection point
      bool found = false;
      for (const auto &cp : from_shape->connection_points) {
        if (cp.id == conn->from.connection_point_id) {
          found = true;
          break;
        }
      }
      if (!found && !conn->from.connection_point_id.empty()) {
        errors.push_back("Connector '" + conn->id + "' references non-existent connection point '" +
                         conn->from.connection_point_id + "' on shape '" + conn->from.shape_id +
                         "'");
      }
    }

    // Check to shape reference
    const auto *to_shape = shape_layer_->get_shape(conn->to.shape_id);
    if (!to_shape) {
      errors.push_back("Connector '" + conn->id + "' references non-existent to_shape '" +
                       conn->to.shape_id + "'");
    } else {
      // Check to connection point
      bool found = false;
      for (const auto &cp : to_shape->connection_points) {
        if (cp.id == conn->to.connection_point_id) {
          found = true;
          break;
        }
      }
      if (!found && !conn->to.connection_point_id.empty()) {
        errors.push_back("Connector '" + conn->id + "' references non-existent connection point '" +
                         conn->to.connection_point_id + "' on shape '" + conn->to.shape_id + "'");
      }
    }
  }

  // 5. Validate component definitions
  for (const auto *comp : component_layer_->get_all_components()) {
    // Check parent component reference
    if (!comp->parent_component_id.empty()) {
      const auto *parent = component_layer_->get_component(comp->parent_component_id);
      if (!parent) {
        errors.push_back("Component '" + comp->id + "' references non-existent parent component '" +
                         comp->parent_component_id + "'");
      }
    }
  }

  // 6. Check for circular dependencies in component inheritance
  for (const auto *comp : component_layer_->get_all_components()) {
    std::set<std::string> visited;
    const ComponentDefinition *current = comp;
    visited.insert(current->id);

    while (!current->parent_component_id.empty()) {
      current = component_layer_->get_component(current->parent_component_id);
      if (!current)
        break; // Already reported as error above

      if (visited.count(current->id)) {
        errors.push_back("Circular dependency detected in component inheritance involving '" +
                         comp->id + "'");
        break;
      }
      visited.insert(current->id);
    }
  }

  // 7. Validate component instances
  for (const auto *inst : component_layer_->get_all_instances()) {
    // Check component reference
    const auto *comp = component_layer_->get_component(inst->component_id);
    if (!comp) {
      errors.push_back("Component instance '" + inst->id + "' references non-existent component '" +
                       inst->component_id + "'");
    } else {
      // Check that all required parameters are provided
      for (const auto &param : comp->parameters) {
        if (inst->parameters.find(param.name) == inst->parameters.end() &&
            param.default_value.empty()) {
          errors.push_back("Component instance '" + inst->id + "' missing required parameter '" +
                           param.name + "'");
        }
      }
    }

    // Check generated shape references
    for (const auto &shape_id : inst->generated_shape_ids) {
      if (!shape_layer_->get_shape(shape_id)) {
        errors.push_back("Component instance '" + inst->id +
                         "' references non-existent generated shape '" + shape_id + "'");
      }
    }
  }

  // 8. Validate events
  for (const auto *event : event_layer_->get_all_events()) {
    // Check target reference (could be shape, data node, or component instance)
    bool target_exists = false;
    if (shape_layer_->get_shape(event->target_id)) {
      target_exists = true;
    } else if (data_layer_->get_node(event->target_id)) {
      target_exists = true;
    } else if (component_layer_->get_instance(event->target_id)) {
      target_exists = true;
    }

    if (!target_exists) {
      errors.push_back("Event '" + event->id + "' references non-existent target '" +
                       event->target_id + "'");
    }
  }

  return errors;
}

} // namespace ddf
} // namespace whiteboard
