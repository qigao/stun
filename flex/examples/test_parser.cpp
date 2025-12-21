/*
 * Test Parser - Debug parser output
 */

#include <iostream>
#include <flex/flex.h>

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

    // If it's a group, print children
    if (node->is_group()) {
        auto* group = static_cast<const flex::Group*>(node);
        const auto& children = group->children();
        std::cout << indent << "  [" << children.size() << " children]\n";
        for (const auto& child : children) {
            print_node(child.get(), depth + 1);
        }
    }
}

int main(int argc, char* argv[]) {
    const char* flex_file = "hello.flex";
    if (argc > 1) {
        flex_file = argv[1];
    }

    std::cout << "=================================================\n";
    std::cout << "Flex Parser Debug Tool\n";
    std::cout << "=================================================\n";
    std::cout << "Loading: " << flex_file << "\n\n";

    // Initialize Flex
    flex::init();

    // Load .flex file
    auto definition = flex::Definition::load_file(flex_file);

    if (definition->has_error()) {
        std::cerr << "❌ Parse error at line " << definition->error_line()
                  << ", column " << definition->error_column() << ":\n";
        std::cerr << "   " << definition->error_message() << "\n";
        flex::shutdown();
        return 1;
    }

    std::cout << "✅ Parse succeeded!\n\n";

    // Print artboard info
    auto artboard = definition->artboard();
    if (!artboard) {
        std::cerr << "❌ No artboard created!\n";
        flex::shutdown();
        return 1;
    }

    std::cout << "Artboard:\n";
    std::cout << "  Size: " << artboard->width() << " x " << artboard->height() << "\n";
    std::cout << "  Background: rgba("
              << artboard->background().r << ", "
              << artboard->background().g << ", "
              << artboard->background().b << ", "
              << artboard->background().a << ")\n\n";

    // Print scene graph
    std::cout << "Scene Graph:\n";
    auto* root = artboard->root();
    if (root) {
        const auto& children = root->children();
        std::cout << "Root has " << children.size() << " children:\n";
        for (const auto& child : children) {
            print_node(child.get(), 1);
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

    std::cout << "\n=================================================\n";
    std::cout << "Done!\n";

    flex::shutdown();
    return 0;
}
