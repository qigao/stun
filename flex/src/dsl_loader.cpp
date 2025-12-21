/*
 * Flex Engine - DSL Loader Implementation
 */

#include "flex/dsl_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace flex {

bool DSLLoader::read_file(const std::string& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) {
        last_error_ = "Failed to open file: " + path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    return true;
}

ast::Document DSLLoader::parse_ast(const std::string& content) {
    // TODO: This is a placeholder - need to integrate with flex_parser.y
    // For now, we'll create a simple hardcoded AST for demonstration

    ast::Document doc;

    // Create a sample timeline
    ast::Timeline timeline;
    timeline.name = "PlayerMove";
    timeline.duration = 3.0;

    // Track 1: x position
    ast::Track track_x;
    track_x.property = "x";

    auto kf1 = std::make_shared<ast::Value>();
    kf1->data = ast::NumberValue{100.0};
    track_x.keyframes.push_back(ast::Keyframe{0.0, "s", kf1, "linear"});

    auto kf2 = std::make_shared<ast::Value>();
    kf2->data = ast::NumberValue{500.0};
    track_x.keyframes.push_back(ast::Keyframe{1.0, "s", kf2, "linear"});

    auto kf3 = std::make_shared<ast::Value>();
    kf3->data = ast::NumberValue{100.0};
    track_x.keyframes.push_back(ast::Keyframe{2.0, "s", kf3, "linear"});

    timeline.tracks.push_back(track_x);

    // Track 2: y position
    ast::Track track_y;
    track_y.property = "y";

    auto kf4 = std::make_shared<ast::Value>();
    kf4->data = ast::NumberValue{400.0};
    track_y.keyframes.push_back(ast::Keyframe{0.0, "s", kf4, "linear"});

    auto kf5 = std::make_shared<ast::Value>();
    kf5->data = ast::NumberValue{300.0};
    track_y.keyframes.push_back(ast::Keyframe{0.5, "s", kf5, "linear"});

    auto kf6 = std::make_shared<ast::Value>();
    kf6->data = ast::NumberValue{400.0};
    track_y.keyframes.push_back(ast::Keyframe{1.0, "s", kf6, "linear"});

    timeline.tracks.push_back(track_y);

    doc.timelines.push_back(timeline);

    // Create a simple artboard
    ast::Artboard artboard;
    artboard.width = 800;
    artboard.height = 600;

    // Player group
    auto player_node = std::make_shared<ast::Node>();
    player_node->type = "Group";
    player_node->name = "player";

    auto x_prop = std::make_shared<ast::Value>();
    x_prop->data = ast::NumberValue{100.0};
    player_node->properties.push_back(ast::Property{"x", x_prop});

    auto y_prop = std::make_shared<ast::Value>();
    y_prop->data = ast::NumberValue{400.0};
    player_node->properties.push_back(ast::Property{"y", y_prop});

    // Player body
    auto body_node = std::make_shared<ast::Node>();
    body_node->type = "Shape";
    body_node->name = "body";

    auto width_prop = std::make_shared<ast::Value>();
    width_prop->data = ast::NumberValue{80.0};
    body_node->properties.push_back(ast::Property{"width", width_prop});

    auto height_prop = std::make_shared<ast::Value>();
    height_prop->data = ast::NumberValue{80.0};
    body_node->properties.push_back(ast::Property{"height", height_prop});

    auto geom_prop = std::make_shared<ast::Value>();
    geom_prop->data = ast::StringValue{"rect"};
    body_node->properties.push_back(ast::Property{"geometry", geom_prop});

    auto color_prop = std::make_shared<ast::Value>();
    color_prop->data = ast::ColorValue{"#3498db"};
    body_node->properties.push_back(ast::Property{"fill.color", color_prop});

    player_node->children.push_back(body_node);
    artboard.children.push_back(player_node);

    doc.artboards.push_back(artboard);

    return doc;
}

} // namespace flex
