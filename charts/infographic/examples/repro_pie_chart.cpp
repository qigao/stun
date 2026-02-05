
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <memory>

#include <flex.h>
#include <flexinfographic.h>
#include <infographic_component.h>
#include <flex/runtime/instance.h>
#include <thorvg.h>

using namespace flex;
using namespace flex::modules::infographic;

// Simple PPM writer
void write_ppm(const std::string& filename, const uint32_t* buffer, int width, int height) {
    std::ofstream ofs(filename, std::ios::binary);
    ofs << "P3\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        uint32_t pixel = buffer[i];
        // ARGB8888 -> R G B (ignore alpha for PPM)
        int r = (pixel >> 16) & 0xFF;
        int g = (pixel >> 8) & 0xFF;
        int b = (pixel) & 0xFF;
        ofs << r << " " << g << " " << b << "\n";
    }
    ofs.close();
    std::cout << "Wrote " << filename << std::endl;
}

int main() {
    flex::init();
    
    constexpr int WIDTH = 800;
    constexpr int HEIGHT = 600;
    
    // Setup ThorVG Canvas
    auto canvas = tvg::SwCanvas::gen();
    std::vector<uint32_t> buffer(WIDTH * HEIGHT);
    canvas->target(buffer.data(), WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);
    
    // Create Pie Chart Infographic
    // Note: The template enum name might be inferred from unified_infographic.h we read earlier
    // ChartPiePlainText
    auto info = create_infographic(TemplateType::ChartPiePlainText);
    info->set_title("Market Share Analysis");
    
    // Add items matching the user image
    info->add_item(DataItem::create_with_value("Product A", 40.0)); // Blue
    info->add_item(DataItem::create_with_value("Product B", 30.0)); // Green
    info->add_item(DataItem::create_with_value("Product C", 20.0)); // Orange
    info->add_item(DataItem::create_with_value("Others", 10.0));    // Gray
    
    // Assuming the renderer picks colors automatically from default theme.
    // Default theme probably has a palette.

    auto instance = Instance::create(WIDTH, HEIGHT);
    auto* scene = instance->scene();
    scene->set_background(Color::WHITE);
    
    auto* node = InfographicComponent::build(*info, *instance);
    if (node) {
        node->set_position(50, 50); // Offset as seen in infographic_render_example.cpp
        scene->root()->add_child(node);
    }
    
    scene->root()->perform_layout();
    
    auto flex_renderer = flex::create_thorvg_renderer(canvas);
    
    // Render
    flex_renderer->begin_frame(WIDTH, HEIGHT, 1.0f);
    flex_renderer->clear(Color::WHITE);
    instance->render(*flex_renderer);
    flex_renderer->end_frame();
    
    canvas->sync();
    
    write_ppm("pie_repro.ppm", buffer.data(), WIDTH, HEIGHT);
    
    flex::shutdown();
    delete canvas;
    
    return 0;
}
