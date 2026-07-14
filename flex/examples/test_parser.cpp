/*
 * Test Parser - Debug parser output
 * Tests the re2c lexer + recursive descent parser
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <flex.h>
#include "backends/thorvg/init.h" 

using namespace flex;

// Print node information
void print_node(const flex::Node* node, int depth = 0) {
    std::string indent(depth * 2, ' ');

    std::cout << indent << "- " << node->type_name()
              << " \"" << node->id() << "\""
              << " pos=(" << node->x() << ", " << node->y() << ")"
              << " scale=(" << node->scale_x() << ", " << node->scale_y() << ")"
              << " rot=" << node->rotation()
              << " opacity=" << node->opacity()
              << "\n";

    // If it's a shape, print geometry and paint info
    if (node->type() == flex::NodeType::Shape) {
        auto* shape = static_cast<const flex::Shape*>(node);
        std::cout << indent << "  Geometry: ";

        switch (shape->geometry_type()) {
            case flex::GeometryType::Rect: {
                auto rect = shape->rect();
                std::cout << "Rect(" << rect.width << " x " << rect.height << ")";
                break;
            }
            case flex::GeometryType::Circle: {
                auto circle = shape->circle();
                std::cout << "Circle(r=" << circle.radius << ")";
                break;
            }
            case flex::GeometryType::Ellipse: {
                auto ellipse = shape->ellipse();
                std::cout << "Ellipse(rx=" << ellipse.rx << ", ry=" << ellipse.ry << ")";
                break;
            }
            default:
                std::cout << "Other";
                break;
        }
        std::cout << "\n";

        // Print fill
        if (shape->has_fill()) {
            auto fill = shape->fill();
            std::cout << indent << "  Fill: rgba("
                      << fill.color.r << ", "
                      << fill.color.g << ", "
                      << fill.color.b << ", "
                      << fill.color.a << ")\n";
        } else {
            std::cout << indent << "  Fill: none\n";
        }

        // Print stroke
        if (shape->has_stroke()) {
            auto stroke = shape->stroke();
            std::cout << indent << "  Stroke: rgba("
                      << stroke.color.r << ", "
                      << stroke.color.g << ", "
                      << stroke.color.b << ", "
                      << stroke.color.a << ") width=" << stroke.width << "\n";
        }
    }
    // If it's text, print text info
    else if (node->type() == flex::NodeType::Text) {
        auto* text = static_cast<const flex::Text*>(node);
        std::cout << indent << "  Content: \"" << text->content() << "\"\n";
        std::cout << indent << "  Font: size=" << text->font_size() << "\n";
        std::cout << indent << "  Color: rgba("
                  << text->color().r << ", "
                  << text->color().g << ", "
                  << text->color().b << ", "
                  << text->color().a << ")\n";
    }

    // If it's a group, print children and pseudo-class styles
    if (node->is_group()) {
        auto* group = static_cast<const flex::Group*>(node);
        const auto& children = group->children();
        std::cout << indent << "  [" << children.size() << " children]\n";

        // Print pseudo-class styles
        auto* styles = group->pseudo_class_styles();
        if (styles && styles->size() > 0) {
            std::cout << indent << "  Pseudo-class styles:\n";
            for (auto it = styles->begin(); it != styles->end(); ++it) {
                std::cout << indent << "    " << it->first << "\n";
            }
        }

        for (const auto& child : children) {
            print_node(child, depth + 1);
        }
    }
}

// Test lexer
void test_lexer(const char* source) {
    std::cout << "\n=== LEXER TEST ===\n";
    std::cout << "Source: " << source << "\n\n";

    auto lexer = flex::parser::lexer_create(source);
    int token_count = 0;

    while (true) {
        flex::parser::Token tok = flex::parser::lex_next_token(lexer);
        std::cout << "Token " << std::setw(3) << token_count++ << ": "
                  << "type=" << std::setw(12) << tok.type
                  << " value=\"" << tok.value << "\""
                  << " line=" << std::setw(3) << tok.line
                  << " col=" << std::setw(3) << tok.column << "\n";

        if (tok.type == flex::parser::TOK_EOF || tok.type == flex::parser::TOK_ERROR) {
            break;
        }
    }

    flex::parser::lexer_destroy(lexer);
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    const char* flex_file = "hello.flex";
    bool test_lexer_only = false;

    // Parse command line
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--lexer" || arg == "-l") {
            test_lexer_only = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Flex Parser Debug Tool\n";
            std::cout << "Usage: test_parser [options] [file.flex]\n";
            std::cout << "Options:\n";
            std::cout << "  --lexer, -l  Test lexer only\n";
            std::cout << "  --help, -h   Show this help\n";
            return 0;
        } else {
            flex_file = argv[i];
        }
    }

    std::cout << "=================================================\n";
    std::cout << "Flex Parser Debug Tool (re2c + Recursive Descent)\n";
    std::cout << "=================================================\n";
    std::cout << "File: " << flex_file << "\n";

    if (test_lexer_only) {
        std::cout << "Mode: Lexer Test Only\n";
    } else {
        std::cout << "Mode: Full Parse + Render\n";
    }

    std::cout << "\n";

    // Initialize Flex
    flex::init();

    // Register test components for parsing demo
    using namespace flex;
    auto dummy_builder = [](const Props& props) -> ComponentNodePtr {
        auto g = std::make_shared<Group>();
        return g;
    };
    ComponentRegistry::instance().register_component("Slider", dummy_builder);
    ComponentRegistry::instance().register_component("ProgressBar", dummy_builder);
    ComponentRegistry::instance().register_component("LabeledSlider", dummy_builder);
    ComponentRegistry::instance().register_component("SettingsRow", dummy_builder);
    ComponentRegistry::instance().register_component("Toggle", dummy_builder);

    // Test lexer only
    if (test_lexer_only) {
        std::ifstream file(flex_file);
        if (!file.is_open()) {
            std::cerr << "❌ Failed to open file: " << flex_file << "\n";
            flex::shutdown();
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        test_lexer(content.c_str());
        flex::shutdown();
        return 0;
    }

    // Full parse and render
    auto definition = flex::Definition::load_file(flex_file);

    if (definition->has_error()) {
        std::cerr << "❌ Parse error at line " << definition->error_line()
                  << ", column " << definition->error_column() << ":\n";
        std::cerr << "   " << definition->error_message() << "\n";
        flex::shutdown();
        return 1;
    }

    std::cout << "✅ Parse succeeded!\n\n";

    // Print scene info
    auto scene = definition->scene();
    if (!scene) {
        std::cerr << "❌ No scene created!\n";
        flex::shutdown();
        return 1;
    }

    std::cout << "Scene:\n";
    std::cout << "  Size: " << scene->width() << " x " << scene->height() << "\n";
    std::cout << "  Background: rgba("
              << scene->background().r << ", "
              << scene->background().g << ", "
              << scene->background().b << ", "
              << scene->background().a << ")\n\n";

    // Print scene graph
    std::cout << "Scene Graph:\n";
    auto* root = scene->root();
    if (root) {
        const auto& children = root->children();
        std::cout << "Root has " << children.size() << " children:\n";
        for (const auto& child : children) {
            print_node(child, 1);
        }
    } else {
        std::cout << "  (empty root)\n";
    }

    // Print timelines
    std::cout << "\nTimelines:\n";
    const auto& timelines = definition->timelines();
    std::cout << "  Found " << timelines.size() << " timeline(s)\n";
    for (const auto& tl : timelines) {
        std::cout << "  - \"" << tl->name() << "\""
                  << " duration=" << tl->duration() << "s"
                  << " tracks=" << tl->tracks().size()
                  << "\n";
    }

    // Note: State machines are parsed to AST but not yet converted to runtime objects
    // This will be implemented in a future update
    std::cout << "\nState Machines: (see AST parsing test for details)\n";
    std::cout << "  Run test_statemachine_parser.cpp for AST-level testing\n";

    std::cout << "\n=================================================\n";
    std::cout << "Done!\n";

    flex::shutdown();
    return 0;
}
