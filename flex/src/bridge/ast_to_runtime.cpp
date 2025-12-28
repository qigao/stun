/*
 * Flex Engine - AST to Runtime Converter (Implementation)
 * Converts parsed AST objects to runtime objects
 */

#include "flex/bridge/ast_to_runtime.h"
#include "flex.h"
#include "flex/compiler/flex_ast.h"
#include "flex/compiler/flex_parser.h"
#include "flex/runtime/debug.h"
#include "flex/runtime/timeline.h"


namespace flex {

// ============================================================================
// Helper: Parse color from hex string - uses Color::from_hex from types.h
// ============================================================================

static inline Color parse_color_from_string(const std::string &color_str) {
  return Color::from_hex(color_str.c_str());
}

// ============================================================================
// AST to Runtime Converter - Implementation
// ============================================================================

// Helper macro to access Definition::Impl from void*
#define IMPL static_cast<Definition::Impl *>(impl_)

AstToRuntimeConverter::AstToRuntimeConverter(void *definition_impl) : impl_(definition_impl) {}

void AstToRuntimeConverter::convert(const parser::AstProgram &program) {
  FLEX_LOGD("Converting AST to runtime objects...");

  // Convert main scene
  if (program.scene) {
    IMPL->scene = convert_scene(program.scene);
  }

  // Convert machines
  if (!program.machines.empty()) {
    convert_machines(program.machines);
  }

  // Convert animations to Timelines
  if (!program.animations.empty()) {
    convert_animations(program.animations);
  }

  // Convert assets
  if (program.assets) {
    convert_assets(*program.assets);
  }

  FLEX_LOGD("Conversion complete!");
}

void AstToRuntimeConverter::convert_machines(
    const std::vector<std::shared_ptr<parser::AstMachine>> &machines) {
  FLEX_LOGD("Converting {} state machine(s)...", machines.size());

  for (const auto &ast_machine : machines) {
    auto runtime_machine = convert_machine(*ast_machine);
    IMPL->machines.push_back(runtime_machine);

    FLEX_LOGD("Converted machine: {}", runtime_machine->name());

    // Convert layers
    for (const auto &ast_layer : ast_machine->layers) {
      convert_layer(runtime_machine.get(), ast_layer);
    }
  }
}

void AstToRuntimeConverter::convert_animations(
    const std::vector<std::shared_ptr<parser::AstAnim>> &animations) {
  FLEX_LOGD("Converting {} animation(s) to Timeline...", animations.size());

  IMPL->timelines.clear();

  for (const auto &ast_anim : animations) {
    auto timeline = convert_animation(*ast_anim);
    IMPL->timelines.push_back(timeline);

    FLEX_LOGD("Converted animation to Timeline: {}", timeline->name());
  }
}

std::shared_ptr<RuntimeStateMachine>
AstToRuntimeConverter::convert_machine(const parser::AstMachine &machine) {
  auto runtime_machine = std::make_shared<RuntimeStateMachine>(machine.name);

  // Layers will be added in convert_layer

  return runtime_machine;
}

void AstToRuntimeConverter::convert_layer(RuntimeStateMachine *machine,
                                          const parser::AstLayer &layer) {
  FLEX_LOGD("  Converting layer: {}", layer.name);

  machine->add_layer(layer.name);
  auto runtime_layer = machine->get_layer(layer.name);

  // Convert states
  for (const auto &ast_state : layer.states) {
    runtime_layer->add_state(ast_state.name, ast_state.initial, ast_state.animation,
                             ast_state.play_audio, ast_state.stop_audio);
    FLEX_LOGD("    Added state: {}{}{}{}{}", ast_state.name, ast_state.initial ? " (initial)" : "",
              ast_state.animation.empty() ? "" : " animation=\"" + ast_state.animation + "\"",
              ast_state.play_audio.empty() ? "" : " play=\"" + ast_state.play_audio + "\"",
              ast_state.stop_audio.empty() ? "" : " stop=\"" + ast_state.stop_audio + "\"");
  }

  // Convert transitions
  for (const auto &ast_trans : layer.transitions) {
    runtime_layer->add_transition(ast_trans.from_state, ast_trans.to_state, ast_trans.condition_var,
                                  ast_trans.condition_op, ast_trans.condition_val);

    if (!ast_trans.condition_var.empty()) {
      FLEX_LOGD("    Added transition: {} -> {} when {} {} {}", ast_trans.from_state,
                ast_trans.to_state, ast_trans.condition_var, ast_trans.condition_op,
                ast_trans.condition_val);
    } else {
      FLEX_LOGD("    Added transition: {} -> {}", ast_trans.from_state, ast_trans.to_state);
    }
  }
}

std::shared_ptr<Timeline> AstToRuntimeConverter::convert_animation(const parser::AstAnim &anim) {
  // Create Timeline using Definition's arena allocator
  auto timeline = Timeline::create(anim.name.c_str(), IMPL->object_alloc);

  // Set properties
  timeline->set_duration(anim.duration);
  timeline->set_loop_mode(parse_loop_mode(anim.loop_mode));

  // Convert tracks
  for (const auto &ast_track : anim.tracks) {
    auto track = timeline->add_track(ast_track.property.c_str());

    // Add keyframes
    for (const auto &ast_kf : ast_track.keyframes) {
      if (auto *fval = std::get_if<float>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *fval);
      } else if (auto *sval = std::get_if<std::string>(&ast_kf.value)) {
        // Check if it's a color string
        if (!sval->empty() && (*sval)[0] == '#') {
          Color color = parse_color_from_string(*sval);
          track->add_keyframe(ast_kf.time, color);
        } else {
          track->add_keyframe(ast_kf.time, sval->c_str());
        }
      } else if (auto *bval = std::get_if<bool>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *bval ? 1.0f : 0.0f);
      }
    }
  }

  return timeline;
}

enum class LoopMode AstToRuntimeConverter::parse_loop_mode(const std::string &mode_str) {
  if (mode_str == "loop") {
    return LoopMode::Loop;
  } else if (mode_str == "pingpong") {
    return LoopMode::PingPong;
  }
  return LoopMode::Once;
}

void AstToRuntimeConverter::convert_assets(const parser::AstAssets &assets) {
  FLEX_LOGD("Converting {} asset(s)...", assets.assets.size());

  IMPL->parsed_assets.clear();

  for (const auto &ast_asset : assets.assets) {
    Definition::Impl::ParsedAsset parsed;
    parsed.type = ast_asset.type;
    parsed.id = ast_asset.id;
    parsed.path = ast_asset.path;

    // Extract options
    for (const auto &[key, value] : ast_asset.options) {
      if (key == "loop") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.loop = *bval;
        }
      } else if (key == "volume") {
        if (auto *fval = std::get_if<float>(&value)) {
          parsed.volume = *fval;
        }
      } else if (key == "preload") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.preload = *bval;
        }
      }
    }

    IMPL->parsed_assets.push_back(parsed);
    if (parsed.type == "audio") {
      FLEX_LOGD("  Asset: {} {} = \"{}\" (loop={}, volume={})", parsed.type, parsed.id, parsed.path,
                parsed.loop ? "true" : "false", parsed.volume);
    } else {
      FLEX_LOGD("  Asset: {} {} = \"{}\"", parsed.type, parsed.id, parsed.path);
    }
  }
}

uint32_t AstToRuntimeConverter::parse_color_rgba(const std::string &color_str) {
  if (color_str.empty() || color_str[0] != '#') {
    return 0x000000FF;
  }

  const char *hex = color_str.c_str() + 1;
  size_t len = color_str.length() - 1;

  uint32_t r = 0, g = 0, b = 0, a = 255;

  if (len == 6) {
    sscanf(hex, "%02x%02x%02x", &r, &g, &b);
  } else if (len == 8) {
    sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
  } else if (len == 3) {
    unsigned int r4, g4, b4;
    sscanf(hex, "%1x%1x%1x", &r4, &g4, &b4);
    r = r4 * 17;
    g = g4 * 17;
    b = b4 * 17;
  } else {
    return 0x000000FF;
  }

  return (r << 24) | (g << 16) | (b << 8) | a;
}

Scene*
AstToRuntimeConverter::convert_scene(const std::shared_ptr<parser::AstScene> &scene) {
  Scene* scene_obj = Scene::create(scene->width, scene->height, IMPL->object_alloc);

  for (const auto &child_ast : scene->children) {
    auto child = convert_node(child_ast);
    if (child) {
      scene_obj->add_child(child);
    }
  }

  return scene_obj;
}

Node*
AstToRuntimeConverter::convert_node(const std::shared_ptr<parser::AstNode> &ast_node) {
  Node* node = nullptr;
  auto& arena = IMPL->object_alloc;

  if (ast_node->type == "group") {
    node = Group::create(arena);
  } else if (ast_node->type == "rect") {
    auto* shape = Shape::create(arena);
    shape->set_rect(100, 100);
    node = shape;
  } else if (ast_node->type == "circle") {
    auto* shape = Shape::create(arena);
    shape->set_circle(50);
    node = shape;
  } else if (ast_node->type == "ellipse") {
    auto* shape = Shape::create(arena);
    shape->set_ellipse(50, 25);
    node = shape;
  } else if (ast_node->type == "polygon") {
    auto* shape = Shape::create(arena);
    shape->set_polygon(5, 50);
    node = shape;
  } else if (ast_node->type == "star") {
    auto* shape = Shape::create(arena);
    shape->set_star(5, 50, 25);
    node = shape;
  } else if (ast_node->type == "text") {
    auto* text = Text::create(arena);
    text->set_content("Text");
    node = text;
  } else if (ast_node->type == "image" || ast_node->type == "img") {
    node = Image::create(arena);
  } else if (ast_node->type == "svg") {
    node = Svg::create(arena);
  } else if (ast_node->type == "path") {
    auto* shape = Shape::create(arena);
    shape->set_path("");
    node = shape;
  } else if (ast_node->type == "line") {
    auto* shape = Shape::create(arena);
    shape->set_line(0, 0);
    node = shape;
  } else if (ast_node->type == "ring") {
    auto* shape = Shape::create(arena);
    shape->set_ring(50, 25);
    node = shape;
  } else if (ast_node->type == "triangle") {
    auto* shape = Shape::create(arena);
    shape->set_triangle(20, 20, Direction::Right);
    node = shape;
  } else {
    auto component = ComponentRegistry::instance().get(ast_node->type);
    if (component) {
      // Node-level properties should not be passed to component
      static const std::set<std::string> node_props = {"x",       "y",        "width",   "height",
                                                       "opacity", "rotation", "visible", "id"};

      Props props;
      for (const auto &[key, ast_value] : ast_node->properties) {
        // Skip Node-level properties - they're applied after instantiation
        if (node_props.count(key))
          continue;

        if (auto fval = std::get_if<float>(&ast_value)) {
          props[key] = *fval;
        } else if (auto sval = std::get_if<std::string>(&ast_value)) {
          if (!sval->empty() && (*sval)[0] == '#') {
            props[key] = parse_color_rgba(*sval);
          } else {
            props[key] = *sval;
          }
        } else if (auto bval = std::get_if<bool>(&ast_value)) {
          props[key] = *bval;
        }
      }
      // TODO: Component instantiation still uses shared_ptr - needs refactoring
      auto shared_node = component->instantiate(props);
      if (shared_node) {
        shared_node->set_id(ast_node->id);
        // WARNING: This is a temporary workaround - component nodes will leak
        // Components need to be refactored to use Arena allocation
        node = shared_node.get();
      }
    }
  }

  if (!node)
    return nullptr;

  node->set_id(ast_node->id);

  for (const auto &[key, value] : ast_node->properties) {
    if (key == "x") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_x(*fval);
      }
    } else if (key == "y") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_y(*fval);
      }
    } else if (key == "width") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_layout_width(*fval);
      }
    } else if (key == "height") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_layout_height(*fval);
      }
    } else if (key == "opacity") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_opacity(*fval);
      }
    } else if (key == "rotation") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_rotation(*fval);
      }
    } else if (key == "alignSelf") {
      if (auto sval = std::get_if<std::string>(&value)) {
        if (*sval == "auto")
          node->set_align_self(AlignSelf::Auto);
        else if (*sval == "start")
          node->set_align_self(AlignSelf::Start);
        else if (*sval == "end")
          node->set_align_self(AlignSelf::End);
        else if (*sval == "center")
          node->set_align_self(AlignSelf::Center);
        else if (*sval == "stretch")
          node->set_align_self(AlignSelf::Stretch);
      }
    } else if (key == "position") {
      if (auto sval = std::get_if<std::string>(&value)) {
        if (*sval == "absolute")
          node->set_position_absolute(true);
      }
    } else if (key == "flexGrow") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_flex_grow(*fval);
      }
    } else if (key == "flexShrink") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_flex_shrink(*fval);
      }
    } else if (key == "flexBasis") {
      if (auto fval = std::get_if<float>(&value)) {
        node->set_flex_basis(*fval);
      }
    } else if (key == "anchor") {
      if (auto sval = std::get_if<std::string>(&value)) {
        if (*sval == "topLeft")
          node->set_anchor(Anchor::TopLeft);
        else if (*sval == "top")
          node->set_anchor(Anchor::Top);
        else if (*sval == "topRight")
          node->set_anchor(Anchor::TopRight);
        else if (*sval == "left")
          node->set_anchor(Anchor::Left);
        else if (*sval == "center")
          node->set_anchor(Anchor::Center);
        else if (*sval == "right")
          node->set_anchor(Anchor::Right);
        else if (*sval == "bottomLeft")
          node->set_anchor(Anchor::BottomLeft);
        else if (*sval == "bottom")
          node->set_anchor(Anchor::Bottom);
        else if (*sval == "bottomRight")
          node->set_anchor(Anchor::BottomRight);
      }
    }

    if (auto* shape = dynamic_cast<Shape*>(node)) {
      if (key == "width") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "rect") {
            auto rect_geom = shape->rect();
            shape->set_rect(*fval, rect_geom.height, rect_geom.corner_radius);
          }
        }
      } else if (key == "height") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "rect") {
            auto rect_geom = shape->rect();
            shape->set_rect(rect_geom.width, *fval, rect_geom.corner_radius);
          }
        }
      } else if (key == "radius") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "circle") {
            shape->set_circle(*fval);
          } else if (ast_node->type == "rect") {
            auto rect_geom = shape->rect();
            shape->set_rect(rect_geom.width, rect_geom.height, *fval);
          } else if (ast_node->type == "polygon") {
            auto poly_geom = shape->polygon();
            shape->set_polygon(poly_geom.sides, *fval);
          } else if (ast_node->type == "star") {
            auto star_geom = shape->star();
            shape->set_star(star_geom.points, *fval, star_geom.inner_radius);
          }
        }
      } else if (key == "sides") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "polygon") {
            auto poly_geom = shape->polygon();
            shape->set_polygon(static_cast<int>(*fval), poly_geom.radius);
          }
        }
      } else if (key == "points") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "star") {
            auto star_geom = shape->star();
            shape->set_star(static_cast<int>(*fval), star_geom.outer_radius,
                            star_geom.inner_radius);
          }
        }
      } else if (key == "rx") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "ellipse") {
            auto ellipse_geom = shape->ellipse();
            shape->set_ellipse(*fval, ellipse_geom.ry);
          }
        }
      } else if (key == "ry") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "ellipse") {
            auto ellipse_geom = shape->ellipse();
            shape->set_ellipse(ellipse_geom.rx, *fval);
          }
        }
      } else if (key == "fill") {
        if (auto sval = std::get_if<std::string>(&value)) {
          uint32_t color = parse_color_rgba(*sval);
          shape->set_fill(color);
        }
      } else if (key == "stroke") {
        if (auto sval = std::get_if<std::string>(&value)) {
          uint32_t color = parse_color_rgba(*sval);
          shape->set_stroke(color);
        }
      } else if (key == "strokeWidth") {
        if (auto fval = std::get_if<float>(&value)) {
          if (shape->has_stroke()) {
            auto stroke_data = shape->stroke();
            shape->set_stroke(stroke_data.color, *fval);
          }
        }
      } else if (key == "d") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (ast_node->type == "path") {
            shape->set_path(*sval);
          }
        }
      } else if (key == "x2") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "line") {
            auto line_geom = shape->line();
            shape->set_line(*fval, line_geom.y2);
          }
        }
      } else if (key == "y2") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "line") {
            auto line_geom = shape->line();
            shape->set_line(line_geom.x2, *fval);
          }
        }
      } else if (key == "outerRadius") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "ring") {
            auto ring_geom = shape->ring();
            shape->set_ring(*fval, ring_geom.inner_radius);
          } else if (ast_node->type == "star") {
            auto star_geom = shape->star();
            shape->set_star(star_geom.points, *fval, star_geom.inner_radius);
          }
        }
      } else if (key == "innerRadius") {
        if (auto fval = std::get_if<float>(&value)) {
          if (ast_node->type == "ring") {
            auto ring_geom = shape->ring();
            shape->set_ring(ring_geom.outer_radius, *fval);
          } else if (ast_node->type == "star") {
            auto star_geom = shape->star();
            shape->set_star(star_geom.points, star_geom.outer_radius, *fval);
          }
        }
      } else if (key == "direction") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (ast_node->type == "triangle") {
            auto tri_geom = shape->triangle();
            Direction dir = Direction::Right;
            if (*sval == "right")
              dir = Direction::Right;
            else if (*sval == "left")
              dir = Direction::Left;
            else if (*sval == "up")
              dir = Direction::Up;
            else if (*sval == "down")
              dir = Direction::Down;
            shape->set_triangle(tri_geom.width, tri_geom.height, dir);
          }
        }
      }
    }

    // Triangle width/height handling (after general width/height)
    if (auto* shape = dynamic_cast<Shape*>(node)) {
      if (ast_node->type == "triangle") {
        auto tri_geom = shape->triangle();
        float w = tri_geom.width;
        float h = tri_geom.height;
        bool updated = false;

        if (key == "width") {
          if (auto fval = std::get_if<float>(&value)) {
            w = *fval;
            updated = true;
          }
        } else if (key == "height") {
          if (auto fval = std::get_if<float>(&value)) {
            h = *fval;
            updated = true;
          }
        }

        if (updated) {
          shape->set_triangle(w, h, tri_geom.direction);
        }
      }

      // Rough/hand-drawn style properties
      if (key == "roughness") {
        if (auto fval = std::get_if<float>(&value)) {
          auto opts = shape->rough();
          opts.roughness = *fval;
          shape->set_rough(opts);
        }
      } else if (key == "bowing") {
        if (auto fval = std::get_if<float>(&value)) {
          auto opts = shape->rough();
          opts.bowing = *fval;
          shape->set_rough(opts);
        }
      } else if (key == "roughSeed") {
        if (auto fval = std::get_if<float>(&value)) {
          auto opts = shape->rough();
          opts.seed = static_cast<unsigned int>(*fval);
          shape->set_rough(opts);
        }
      } else if (key == "fillStyle") {
        if (auto sval = std::get_if<std::string>(&value)) {
          auto opts = shape->rough();
          if (*sval == "solid")
            opts.fill_style = RoughFillStyle::Solid;
          else if (*sval == "hachure")
            opts.fill_style = RoughFillStyle::Hachure;
          else if (*sval == "zigzag")
            opts.fill_style = RoughFillStyle::ZigZag;
          else if (*sval == "crosshatch")
            opts.fill_style = RoughFillStyle::CrossHatch;
          shape->set_rough(opts);
        }
      } else if (key == "hachureGap") {
        if (auto fval = std::get_if<float>(&value)) {
          auto opts = shape->rough();
          opts.hachure_gap = *fval;
          shape->set_rough(opts);
        }
      } else if (key == "hachureAngle") {
        if (auto fval = std::get_if<float>(&value)) {
          auto opts = shape->rough();
          opts.hachure_angle = *fval;
          shape->set_rough(opts);
        }
      }
    }

    if (auto* svg = dynamic_cast<Svg*>(node)) {
      if (key == "src") {
        if (auto sval = std::get_if<std::string>(&value)) {
          svg->set_src(*sval);
        }
      } else if (key == "data") {
        if (auto sval = std::get_if<std::string>(&value)) {
          svg->set_data(*sval);
        }
      }
    }

    if (auto* img = dynamic_cast<Image*>(node)) {
      if (key == "src") {
        if (auto sval = std::get_if<std::string>(&value)) {
          img->set_src(*sval);
        }
      }
    }

    if (auto* text = dynamic_cast<Text*>(node)) {
      if (key == "content") {
        if (auto sval = std::get_if<std::string>(&value)) {
          text->set_content(*sval);
        }
      } else if (key == "fontSize") {
        if (auto fval = std::get_if<float>(&value)) {
          text->set_font_size(*fval);
        }
      } else if (key == "color") {
        if (auto sval = std::get_if<std::string>(&value)) {
          uint32_t color = parse_color_rgba(*sval);
          text->set_color(color);
        }
      }
    }

    // Group layout properties
    if (auto* group = dynamic_cast<Group*>(node)) {
      if (key == "layout") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (*sval == "flex") {
            group->set_layout(LayoutMode::Flex);
          }
        }
      } else if (key == "flexDirection") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (*sval == "row")
            group->set_flex_direction(FlexDirection::Row);
          else if (*sval == "column")
            group->set_flex_direction(FlexDirection::Column);
          else if (*sval == "rowReverse")
            group->set_flex_direction(FlexDirection::RowReverse);
          else if (*sval == "columnReverse")
            group->set_flex_direction(FlexDirection::ColumnReverse);
        }
      } else if (key == "justifyContent") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (*sval == "start")
            group->set_justify_content(JustifyContent::Start);
          else if (*sval == "end")
            group->set_justify_content(JustifyContent::End);
          else if (*sval == "center")
            group->set_justify_content(JustifyContent::Center);
          else if (*sval == "spaceBetween")
            group->set_justify_content(JustifyContent::SpaceBetween);
          else if (*sval == "spaceAround")
            group->set_justify_content(JustifyContent::SpaceAround);
          else if (*sval == "spaceEvenly")
            group->set_justify_content(JustifyContent::SpaceEvenly);
        }
      } else if (key == "alignItems") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (*sval == "start")
            group->set_align_items(AlignItems::Start);
          else if (*sval == "end")
            group->set_align_items(AlignItems::End);
          else if (*sval == "center")
            group->set_align_items(AlignItems::Center);
          else if (*sval == "stretch")
            group->set_align_items(AlignItems::Stretch);
        }
      } else if (key == "flexWrap") {
        if (auto sval = std::get_if<std::string>(&value)) {
          if (*sval == "nowrap" || *sval == "noWrap")
            group->set_flex_wrap(FlexWrap::NoWrap);
          else if (*sval == "wrap")
            group->set_flex_wrap(FlexWrap::Wrap);
        }
      } else if (key == "gap") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_gap(*fval);
        }
      } else if (key == "padding") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_padding(*fval);
        }
      } else if (key == "paddingTop") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_padding(*fval, group->padding_right(), group->padding_bottom(),
                             group->padding_left());
        }
      } else if (key == "paddingRight") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_padding(group->padding_top(), *fval, group->padding_bottom(),
                             group->padding_left());
        }
      } else if (key == "paddingBottom") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_padding(group->padding_top(), group->padding_right(), *fval,
                             group->padding_left());
        }
      } else if (key == "paddingLeft") {
        if (auto fval = std::get_if<float>(&value)) {
          group->set_padding(group->padding_top(), group->padding_right(), group->padding_bottom(),
                             *fval);
        }
      }
    }
  }

  for (const auto &child_ast : ast_node->children) {
    auto child = convert_node(child_ast);
    if (child) {
      if (auto* group = dynamic_cast<Group*>(node)) {
        group->add_child(child);
      }
    }
  }

  return node;
}

} // namespace flex
