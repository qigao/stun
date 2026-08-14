/*
 * Flex Binary Format Tests
 * Tests binary serialization, CRC32, and round-trip consistency
 */

#include "tinytest.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "flex/binary/compiler.h"
#include "flex/binary/format.h"
#include "flex/binary/reader.h"
#include "flex/binary/writer.h"
#include "flex.h"
#include "flex/runtime.h"

using namespace flex;
using namespace flex::binary;

namespace {

inline void check_close(float actual, float expected, float eps = 0.001f) {
    check_float_eq(actual, expected, eps);
}

struct TempBinaryFile {
    std::filesystem::path path;

    explicit TempBinaryFile(const std::vector<uint8_t>& bytes) {
        path = std::filesystem::temp_directory_path() / "flex_definition_load_binary.flexb";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    ~TempBinaryFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

struct TempFlexFile {
    std::filesystem::path path;

    explicit TempFlexFile(const std::string& source) {
        path = std::filesystem::temp_directory_path() / "flex_instance_node_binary_roundtrip.flex";
        std::ofstream out(path, std::ios::binary);
        out.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    ~TempFlexFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

constexpr uint32_t LEGACY_SVG_VERSION = 0x00010001;
constexpr uint32_t LEGACY_RADIAL_VERSION = 0x00010002;

#pragma pack(push, 1)
struct LegacyRadialGradientPaintPayload {
    PaintType type;
    uint8_t reserved[3];
    float cx, cy, radius;
    uint32_t stop_count;
};
#pragma pack(pop)

} // namespace

suite("flex::binary") {
    group("crc32") {
        it("calculates a basic checksum") {
            const char* data = "Hello, World!";
            uint32_t crc = calculate_crc32(data, std::strlen(data));

            check(crc != 0);
            check(crc != 0xFFFFFFFF);
        }

        it("is deterministic for the same input") {
            const char* data = "Test data";
            uint32_t crc1 = calculate_crc32(data, std::strlen(data));
            uint32_t crc2 = calculate_crc32(data, std::strlen(data));

            check(crc1 == crc2);
        }

        it("changes when the input changes") {
            const char* data1 = "Test data 1";
            const char* data2 = "Test data 2";

            uint32_t crc1 = calculate_crc32(data1, std::strlen(data1));
            uint32_t crc2 = calculate_crc32(data2, std::strlen(data2));

            check(crc1 != crc2);
        }

        it("matches incremental calculation") {
            const char* data = "This is a longer test string to verify incremental CRC32";
            size_t len = std::strlen(data);

            uint32_t full_crc = calculate_crc32(data, len);

            size_t split = len / 2;
            uint32_t part1_crc = calculate_crc32(data, split);
            uint32_t incremental_crc = calculate_crc32_continue(data + split, len - split, part1_crc);

            check(full_crc == incremental_crc);
        }

        it("verifies a checksum") {
            const char* data = "Verify me";
            uint32_t crc = calculate_crc32(data, std::strlen(data));

            check(verify_checksum(data, std::strlen(data), crc));
            check(!verify_checksum(data, std::strlen(data), crc + 1));
        }
    }

    group("writer") {
        it("serializes an empty scene") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);

            check(!binary.empty());
            check(binary.size() >= sizeof(FileHeader));
            if (binary.empty() || binary.size() < sizeof(FileHeader)) {
                return;
            }

            const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
            check(header->magic == MAGIC_NUMBER);
            check(header->version == FORMAT_VERSION);
        }

        it("stores scene dimensions correctly") {
            ArenaAllocator arena(4096);
            float width = 1920.0f;
            float height = 1080.0f;
            auto scene = Scene::create(width, height, arena);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
            check_close(header->canvas_width, width);
            check_close(header->canvas_height, height);
        }

        it("tracks binary size") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);

            check(writer.binary_size() > 0);
            check(writer.binary_size() == binary.size());
        }
    }

    group("reader") {
        it("rejects an invalid magic number") {
            std::vector<uint8_t> bad_data(sizeof(FileHeader), 0);
            auto* header = reinterpret_cast<FileHeader*>(bad_data.data());
            header->magic = 0xDEADBEEF;

            BinaryReader reader;
            bool loaded = reader.load_memory(bad_data.data(), bad_data.size());

            check(!loaded);
            check(!reader.is_valid());
        }

        it("rejects files that are too small") {
            std::vector<uint8_t> tiny_data(10, 0);

            BinaryReader reader;
            bool loaded = reader.load_memory(tiny_data.data(), tiny_data.size());

            check(!loaded);
            check(!reader.is_valid());
        }

        it("rejects an invalid checksum") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->checksum = 0xDEADBEEF;

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
            check(!reader.is_valid());
        }

        it("rejects encrypted binaries on the regular load path") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->flags |= FLAG_ENCRYPTED;
            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
            check(!reader.is_valid());
        }
    }

    group("roundtrip") {
        it("round-trips an empty scene") {
            ArenaAllocator arena(4096);
            float width = 1024.0f;
            float height = 768.0f;

            auto original = Scene::create(width, height, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(original);

            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }
            check(reader.is_valid());
            if (!reader.is_valid()) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            check_close(loaded->width(), width);
            check_close(loaded->height(), height);
        }

        it("round-trips multiple scene sizes") {
            std::vector<std::pair<float, float>> sizes = {
                {100.0f, 100.0f},
                {800.0f, 600.0f},
                {1920.0f, 1080.0f},
                {3840.0f, 2160.0f},
            };

            for (auto [width, height] : sizes) {
                ArenaAllocator arena(4096);
                auto original = Scene::create(width, height, arena);

                BinaryWriter writer;
                std::vector<uint8_t> binary = writer.write(original);
                check(!binary.empty());
                if (binary.empty()) {
                    return;
                }

                BinaryReader reader;
                bool loaded_ok = reader.load_memory(binary.data(), binary.size());
                check(loaded_ok);
                if (!loaded_ok) {
                    return;
                }
                check(reader.is_valid());
                if (!reader.is_valid()) {
                    return;
                }

                auto loaded = reader.create_scene();
                check(loaded != nullptr);
                if (!loaded) {
                    return;
                }

                check_close(loaded->width(), width);
                check_close(loaded->height(), height);
            }
        }

        it("round-trips a node tree with common properties") {
            ArenaAllocator arena(16384);
            auto original = Scene::create(800.0f, 600.0f, arena);

            auto* container = Group::create(arena);
            container->set_id("container");
            container->set_position(10.0f, 20.0f);
            container->set_opacity(0.75f);
            container->set_layout_size(320.0f, 180.0f);
            container->set_flex(1.0f, 0.0f, 48.0f);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(120.0f, 80.0f, 6.0f);
            box->set_fill(Color(1.0f, 0.0f, 0.0f, 1.0f));
            box->set_stroke(Color(0.0f, 0.0f, 0.0f, 1.0f), 2.0f);
            box->set_position(5.0f, 8.0f);
            box->set_rotation(15.0f);
            box->set_scale(1.5f, 0.5f);

            auto* label = Text::create(arena);
            label->set_id("label");
            label->set_content("Binary");
            label->set_font_family("Arial");
            label->set_font_size(18.0f);
            label->set_font_weight(FontWeight::Bold);
            label->set_font_style(FontStyle::Italic);
            label->set_color(Color(0.2f, 0.4f, 0.6f, 1.0f));

            auto* image = Image::create(arena);
            image->set_id("icon");
            image->set_src("images/icon.png");
            image->set_width(64.0f);
            image->set_height(32.0f);
            image->set_position(200.0f, 120.0f);

            container->add_child(box);
            container->add_child(label);
            original->add_child(container);
            original->add_child(image);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(original);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_container = dynamic_cast<Group*>(loaded->find("container"));
            auto* loaded_box = dynamic_cast<Shape*>(loaded->find("box"));
            auto* loaded_label = dynamic_cast<Text*>(loaded->find("label"));
            auto* loaded_image = dynamic_cast<Image*>(loaded->find("icon"));

            check(loaded_container != nullptr);
            check(loaded_box != nullptr);
            check(loaded_label != nullptr);
            check(loaded_image != nullptr);
            if (!loaded_container || !loaded_box || !loaded_label || !loaded_image) {
                return;
            }

            check(loaded_container->child_count() == 2);
            check_close(loaded_container->x(), 10.0f);
            check_close(loaded_container->y(), 20.0f);
            check_close(loaded_container->opacity(), 0.75f);
            check_close(loaded_container->layout_width(), 320.0f);
            check_close(loaded_container->layout_height(), 180.0f);
            check_close(loaded_container->flex_grow(), 1.0f);
            check_close(loaded_container->flex_shrink(), 0.0f);
            check_close(loaded_container->flex_basis(), 48.0f);

            check(loaded_box->geometry_type() == GeometryType::Rect);
            check_close(loaded_box->x(), 5.0f);
            check_close(loaded_box->y(), 8.0f);
            check_close(loaded_box->rotation(), 15.0f);
            check_close(loaded_box->scale_x(), 1.5f);
            check_close(loaded_box->scale_y(), 0.5f);
            check(loaded_box->has_fill());
            check(loaded_box->has_stroke());
            check_close(loaded_box->rect().width, 120.0f);
            check_close(loaded_box->rect().height, 80.0f);
            check_close(loaded_box->rect().corner_radius, 6.0f);
            check_close(loaded_box->stroke().width, 2.0f);

            check(loaded_label->content() == "Binary");
            check(loaded_label->font_family() == "Arial");
            check_close(loaded_label->font_size(), 18.0f);
            check(loaded_label->font_weight() == FontWeight::Bold);
            check(loaded_label->font_style() == FontStyle::Italic);
            check_close(loaded_label->color().r, 0.2f);
            check_close(loaded_label->color().g, 0.4f);
            check_close(loaded_label->color().b, 0.6f);

            check(loaded_image->src() == "images/icon.png");
            check_close(loaded_image->width(), 64.0f);
            check_close(loaded_image->height(), 32.0f);
            check_close(loaded_image->x(), 200.0f);
            check_close(loaded_image->y(), 120.0f);
        }

        it("round-trips gradient paints on shapes") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* shape = Shape::create(arena);
            shape->set_id("gradientBox");
            shape->set_rect(120.0f, 80.0f, 4.0f);

            LinearGradient fill(0.0f, 0.0f, 1.0f, 1.0f);
            fill.add_stop(0.0f, Color(1.0f, 0.0f, 0.0f, 1.0f));
            fill.add_stop(1.0f, Color(0.0f, 0.0f, 1.0f, 1.0f));
            shape->set_fill(fill);

            RadialGradient stroke(0.5f, 0.5f, 0.75f);
            stroke.fx = 0.2f;
            stroke.fy = 0.8f;
            stroke.add_stop(0.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
            stroke.add_stop(1.0f, Color(0.0f, 0.0f, 0.0f, 1.0f));
            shape->set_stroke(stroke, 3.0f);

            scene->add_child(shape);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_shape = dynamic_cast<Shape*>(loaded->find("gradientBox"));
            check(loaded_shape != nullptr);
            if (!loaded_shape) {
                return;
            }

            auto loaded_fill = loaded_shape->fill();
            auto loaded_stroke = loaded_shape->stroke();

            check(loaded_fill.type == FillType::LinearGradient);
            check(loaded_fill.linear_gradient.stops.size() == 2);
            check_close(loaded_fill.linear_gradient.x1, 0.0f);
            check_close(loaded_fill.linear_gradient.y1, 0.0f);
            check_close(loaded_fill.linear_gradient.x2, 1.0f);
            check_close(loaded_fill.linear_gradient.y2, 1.0f);
            check_close(loaded_fill.linear_gradient.stops[0].offset, 0.0f);
            check_close(loaded_fill.linear_gradient.stops[1].offset, 1.0f);
            check_close(loaded_fill.linear_gradient.stops[0].color.r, 1.0f);
            check_close(loaded_fill.linear_gradient.stops[1].color.b, 1.0f);

            check(loaded_stroke.type == StrokeType::RadialGradient);
            check_close(loaded_stroke.width, 3.0f);
            check(loaded_stroke.radial_gradient.stops.size() == 2);
            check_close(loaded_stroke.radial_gradient.cx, 0.5f);
            check_close(loaded_stroke.radial_gradient.cy, 0.5f);
            check_close(loaded_stroke.radial_gradient.radius, 0.75f);
            check_close(loaded_stroke.radial_gradient.fx, 0.2f);
            check_close(loaded_stroke.radial_gradient.fy, 0.8f);
            check_close(loaded_stroke.radial_gradient.stops[0].offset, 0.0f);
            check_close(loaded_stroke.radial_gradient.stops[1].offset, 1.0f);
            check_close(loaded_stroke.radial_gradient.stops[0].color.r, 1.0f);
            check_close(loaded_stroke.radial_gradient.stops[1].color.r, 0.0f);
        }

        it("loads legacy radial gradients without focal coordinates") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* shape = Shape::create(arena);
            shape->set_id("legacyRadial");
            shape->set_rect(80.0f, 60.0f, 2.0f);

            RadialGradient stroke(0.3f, 0.7f, 0.6f);
            stroke.fx = 0.1f;
            stroke.fy = 0.9f;
            stroke.add_stop(0.0f, Color(1.0f, 0.0f, 0.0f, 1.0f));
            stroke.add_stop(1.0f, Color(0.0f, 0.0f, 1.0f, 1.0f));
            shape->set_stroke(stroke, 2.5f);
            scene->add_child(shape);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(ShapeData) + sizeof(RadialGradientPaint));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(ShapeData) + sizeof(RadialGradientPaint)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* shape_data = reinterpret_cast<ShapeData*>(binary.data() + child->data_offset);
            auto* radial = reinterpret_cast<RadialGradientPaint*>(binary.data() + shape_data->stroke_paint_offset);
            auto* stops = reinterpret_cast<const flex::binary::ColorStop*>(binary.data() + shape_data->stroke_paint_offset + sizeof(RadialGradientPaint));

            LegacyRadialGradientPaintPayload legacy = {};
            legacy.type = radial->type;
            legacy.cx = radial->cx;
            legacy.cy = radial->cy;
            legacy.radius = radial->radius;
            legacy.stop_count = radial->stop_count;

            std::memcpy(binary.data() + shape_data->stroke_paint_offset, &legacy, sizeof(legacy));
            std::memmove(binary.data() + shape_data->stroke_paint_offset + sizeof(legacy),
                         stops,
                         radial->stop_count * sizeof(flex::binary::ColorStop));

            header->version = LEGACY_RADIAL_VERSION;
            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_shape = dynamic_cast<Shape*>(loaded->find("legacyRadial"));
            check(loaded_shape != nullptr);
            if (!loaded_shape) {
                return;
            }

            auto loaded_stroke = loaded_shape->stroke();
            check(loaded_stroke.type == StrokeType::RadialGradient);
            check_close(loaded_stroke.radial_gradient.cx, 0.3f);
            check_close(loaded_stroke.radial_gradient.cy, 0.7f);
            check_close(loaded_stroke.radial_gradient.radius, 0.6f);
            check_close(loaded_stroke.radial_gradient.fx, 0.3f);
            check_close(loaded_stroke.radial_gradient.fy, 0.7f);
            check(loaded_stroke.radial_gradient.stops.size() == 2);
            check_close(loaded_stroke.radial_gradient.stops[0].offset, 0.0f);
            check_close(loaded_stroke.radial_gradient.stops[1].offset, 1.0f);
        }

        it("round-trips line and ring geometries") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* line = Shape::create(arena);
            line->set_id("lineShape");
            line->set_line(32.0f, 48.0f);

            auto* ring = Shape::create(arena);
            ring->set_id("ringShape");
            ring->set_ring(24.0f, 12.0f);

            scene->add_child(line);
            scene->add_child(ring);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_line = dynamic_cast<Shape*>(loaded->find("lineShape"));
            auto* loaded_ring = dynamic_cast<Shape*>(loaded->find("ringShape"));
            check(loaded_line != nullptr);
            check(loaded_ring != nullptr);
            if (!loaded_line || !loaded_ring) {
                return;
            }

            check(loaded_line->geometry_type() == GeometryType::Line);
            check_close(loaded_line->line().x2, 32.0f);
            check_close(loaded_line->line().y2, 48.0f);

            check(loaded_ring->geometry_type() == GeometryType::Ring);
            check_close(loaded_ring->ring().outer_radius, 24.0f);
            check_close(loaded_ring->ring().inner_radius, 12.0f);
        }

        it("round-trips svg source and inline data") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* svg_file = Svg::create(arena);
            svg_file->set_id("svgFile");
            svg_file->set_src("icons/check.svg");
            svg_file->set_width(24.0f);
            svg_file->set_height(24.0f);

            auto* svg_inline = Svg::create(arena);
            svg_inline->set_id("svgInline");
            svg_inline->set_data("<svg viewBox=\"0 0 10 10\"><circle cx=\"5\" cy=\"5\" r=\"4\"/></svg>");
            svg_inline->set_width(10.0f);
            svg_inline->set_height(10.0f);

            scene->add_child(svg_file);
            scene->add_child(svg_inline);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_svg_file = dynamic_cast<Svg*>(loaded->find("svgFile"));
            auto* loaded_svg_inline = dynamic_cast<Svg*>(loaded->find("svgInline"));
            check(loaded_svg_file != nullptr);
            check(loaded_svg_inline != nullptr);
            if (!loaded_svg_file || !loaded_svg_inline) {
                return;
            }

            check(loaded_svg_file->src() == "icons/check.svg");
            check(loaded_svg_file->data().empty());
            check_close(loaded_svg_file->width(), 24.0f);
            check_close(loaded_svg_file->height(), 24.0f);

            check(loaded_svg_inline->src().empty());
            check(loaded_svg_inline->data() == "<svg viewBox=\"0 0 10 10\"><circle cx=\"5\" cy=\"5\" r=\"4\"/></svg>");
            check_close(loaded_svg_inline->width(), 10.0f);
            check_close(loaded_svg_inline->height(), 10.0f);
        }

        it("loads legacy svg payloads stored as image data") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* svg = Svg::create(arena);
            svg->set_id("legacySvg");
            svg->set_src("icons/legacy.svg");
            svg->set_width(48.0f);
            svg->set_height(32.0f);
            scene->add_child(svg);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2);
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* svg_data = reinterpret_cast<const SvgData*>(binary.data() + child->data_offset);

            ImageData legacy = {};
            legacy.source = svg_data->source;
            legacy.width = svg_data->width;
            legacy.height = svg_data->height;
            legacy.embedded = 0;
            std::memcpy(binary.data() + child->data_offset, &legacy, sizeof(ImageData));

            header->version = LEGACY_SVG_VERSION;
            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_svg = dynamic_cast<Svg*>(loaded->find("legacySvg"));
            check(loaded_svg != nullptr);
            if (!loaded_svg) {
                return;
            }

            check(loaded_svg->src() == "icons/legacy.svg");
            check(loaded_svg->data().empty());
            check_close(loaded_svg->width(), 48.0f);
            check_close(loaded_svg->height(), 32.0f);
        }

        it("round-trips instance nodes with inputs") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* instance = arena.create<InstanceNode>();
            instance->set_id("nested");
            instance->set_source("components/badge.flex");
            instance->set_input("width", 42.0f);
            instance->set_input("title", "Badge");
            instance->set_input("enabled", true);
            instance->set_position(12.0f, 24.0f);
            scene->add_child(instance);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            auto* loaded_instance = dynamic_cast<InstanceNode*>(loaded->find("nested"));
            check(loaded_instance != nullptr);
            if (!loaded_instance) {
                return;
            }

            check(loaded_instance->source() == "components/badge.flex");
            check_close(loaded_instance->get_float_input("width"), 42.0f);
            check(loaded_instance->get_string_input("title") == "Badge");
            check(loaded_instance->get_bool_input("enabled"));
            check_close(loaded_instance->x(), 12.0f);
            check_close(loaded_instance->y(), 24.0f);
        }

        it("preserves instance node inputs through load and advance") {
            TempFlexFile child(R"(
scene Child {
    width: 100
    height: 50

    rect badge {
        x: ${enabled}
        y: ${offset}
        width: 10
        height: 10
    }

    text label {
        content: ${title}
    }
}
)");

            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);
            auto* instance = arena.create<InstanceNode>();
            instance->set_id("runtimeChild");
            instance->set_source(child.path.string());
            instance->set_input("enabled", true);
            instance->set_input("offset", 7.0f);
            instance->set_input("title", "FromBinary");
            scene->add_child(instance);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            auto* loaded_instance = dynamic_cast<InstanceNode*>(loaded_scene->find("runtimeChild"));
            check(loaded_instance != nullptr);
            if (!loaded_instance) {
                return;
            }

            check(loaded_instance->load());
            loaded_instance->advance(0.0f);

            auto* content = loaded_instance->content();
            check(content != nullptr);
            if (!content) {
                return;
            }

            auto* badge = dynamic_cast<Shape*>(content->find("badge"));
            auto* label = dynamic_cast<Text*>(content->find("label"));
            check(badge != nullptr);
            check(label != nullptr);
            if (!badge || !label) {
                return;
            }

            check_close(badge->x(), 1.0f);
            check_close(badge->y(), 7.0f);
            check(label->content() == "FromBinary");
        }

        it("round-trips simple timelines") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("fade", arena);
            timeline->set_duration(1.5f);
            timeline->set_loop_mode(LoopMode::Loop);

            auto track = timeline->add_track("#box/opacity");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.5f, 1.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.size() == 1);
            if (timelines.size() != 1) {
                return;
            }

            const auto& loaded_timeline = timelines[0];
            check(loaded_timeline != nullptr);
            if (!loaded_timeline) {
                return;
            }

            check(std::string(loaded_timeline->name()) == "fade");
            check_close(loaded_timeline->duration(), 1.5f);
            check(loaded_timeline->loop_mode() == LoopMode::Loop);
            check(loaded_timeline->tracks().size() == 1);
            if (loaded_timeline->tracks().size() == 0) {
                return;
            }

            const auto& loaded_track = loaded_timeline->tracks()[0];
            check(loaded_track != nullptr);
            if (!loaded_track) {
                return;
            }

            check(std::string(loaded_track->property()) == "#box/opacity");
            check(loaded_track->keyframe_count() == 2);
            check_close(std::get<float>(loaded_track->sample(0.0f)), 0.0f);
            check_close(std::get<float>(loaded_track->sample(1.5f)), 1.0f);
        }

        it("rejects MIR tracks that the current binary format cannot preserve") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);
            auto timeline = Timeline::create("jit", arena);
            auto track = timeline->add_track("x");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 1.0f);
            track->set_numeric_expression("smoothstep(0, 1, progress)");

            BinaryWriter writer;
            check(writer.write(scene, {timeline}).empty());
        }

        it("rejects vec2 motion paths that the current binary format cannot preserve") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);
            auto timeline = Timeline::create("curve", arena);
            auto track = timeline->add_track("position");
            track->set_spatial_interpolation(SpatialInterpolation::CatmullRom);
            track->add_keyframe(0.0f, Vec2{0.0f, 0.0f});
            track->add_keyframe(1.0f, Vec2{10.0f, 20.0f});

            BinaryWriter writer;
            check(writer.write(scene, {timeline}).empty());
        }

        it("rejects timeline triggers that the current binary format cannot preserve") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);
            auto timeline = Timeline::create("interactive", arena);
            timeline->add_trigger(0.5f, "halfway");

            BinaryWriter writer;
            check(writer.write(scene, {timeline}).empty());
        }

        it("round-trips string and color timelines") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("statusText");
            label->set_content("Ready");
            label->set_color(Color(0.5f, 0.5f, 0.5f, 1.0f));
            scene->add_child(label);

            auto timeline = Timeline::create("status", arena);
            timeline->set_duration(0.2f);

            auto content_track = timeline->add_track("#statusText/content");
            content_track->add_keyframe(0.0f, "Playing...");

            auto color_track = timeline->add_track("#statusText/color");
            color_track->add_keyframe(0.0f, Color(0.0f, 1.0f, 0.5333333f, 1.0f));

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.size() == 1);
            if (timelines.size() != 1) {
                return;
            }

            const auto& loaded_timeline = timelines[0];
            check(loaded_timeline->tracks().size() == 2);
            if (loaded_timeline->tracks().size() != 2) {
                return;
            }

            const auto& loaded_content_track = loaded_timeline->tracks()[0];
            const auto& loaded_color_track = loaded_timeline->tracks()[1];
            check(std::string(loaded_content_track->property()) == "#statusText/content");
            check(std::string(loaded_color_track->property()) == "#statusText/color");
            check(std::get<std::string>(loaded_content_track->sample(0.0f)) == "Playing...");

            Color color = std::get<Color>(loaded_color_track->sample(0.0f));
            check_close(color.r, 0.0f);
            check_close(color.g, 1.0f);
            check_close(color.b, 0.5333333f);
            check_close(color.a, 1.0f);
        }
    }

    group("compiler") {
        it("compiles a simple scene source") {
            const char* source = R"(
scene TestScene {
    width: 800
    height: 600
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(!compiler.has_error());
            check(!binary.empty());
            if (compiler.has_error() || binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }
            check(reader.is_valid());
            if (!reader.is_valid()) {
                return;
            }

            auto scene = reader.create_scene();
            check(scene != nullptr);
            if (!scene) {
                return;
            }

            check_close(scene->width(), 800.0f);
            check_close(scene->height(), 600.0f);
        }

        it("serializes simple animations into binary timelines") {
            const char* source = R"(
anim "fadeIn" {
    duration: 2.0
    loop: once

    track "opacity" {
        keyframe 0 -> 0.0
        keyframe 2.0 -> 1.0
    }
}

scene TestScene {
    width: 800
    height: 600
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(!compiler.has_error());
            check(!binary.empty());
            if (compiler.has_error() || binary.empty()) {
                return;
            }

            check(compiler.stats().timeline_count == 1);

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.size() == 1);
            if (timelines.size() != 1) {
                return;
            }

            check(std::string(timelines[0]->name()) == "fadeIn");
            check_close(timelines[0]->duration(), 2.0f);
            check(timelines[0]->tracks().size() == 1);
            if (timelines[0]->tracks().size() == 0) {
                return;
            }

            check(std::string(timelines[0]->tracks()[0]->property()) == "opacity");
            check(timelines[0]->tracks()[0]->keyframe_count() == 2);
        }

        it("serializes string and color animations into binary timelines") {
            const char* source = R"(
anim "status" {
    duration: 0.2

    track "#statusText/content" {
        keyframe 0 -> "Playing..."
    }

    track "#statusText/color" {
        keyframe 0 -> #00ff88
    }
}

scene TestScene {
    width: 800
    height: 600

    text statusText {
        content: "Ready"
        color: #888888
    }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(!compiler.has_error());
            check(!binary.empty());
            if (compiler.has_error() || binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.size() == 1);
            if (timelines.size() != 1) {
                return;
            }

            check(std::string(timelines[0]->name()) == "status");
            check(timelines[0]->tracks().size() == 2);
            if (timelines[0]->tracks().size() != 2) {
                return;
            }

            auto content = std::get<std::string>(timelines[0]->tracks()[0]->sample(0.0f));
            auto color = std::get<Color>(timelines[0]->tracks()[1]->sample(0.0f));
            check(content == "Playing...");
            check_close(color.r, 0.0f);
            check_close(color.g, 1.0f);
            check_close(color.b, 0.5333333f);
            check_close(color.a, 1.0f);
        }

        it("serializes svg nodes into binary scenes") {
            const char* source = R"(
scene TestScene {
    width: 800
    height: 600

    svg fileIcon {
        src: "icons/check.svg"
        width: 24
        height: 24
    }

    svg inlineIcon {
        data: "<svg viewBox=\"0 0 10 10\"><rect x=\"1\" y=\"1\" width=\"8\" height=\"8\"/></svg>"
        width: 10
        height: 10
    }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(!compiler.has_error());
            check(!binary.empty());
            if (compiler.has_error() || binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto scene = reader.create_scene();
            check(scene != nullptr);
            if (!scene) {
                return;
            }

            auto* file_icon = dynamic_cast<Svg*>(scene->find("fileIcon"));
            auto* inline_icon = dynamic_cast<Svg*>(scene->find("inlineIcon"));
            check(file_icon != nullptr);
            check(inline_icon != nullptr);
            if (!file_icon || !inline_icon) {
                return;
            }

            check(file_icon->src() == "icons/check.svg");
            check(file_icon->data().empty());
            check(inline_icon->src().empty());
            check(inline_icon->data() == R"(<svg viewBox=\"0 0 10 10\"><rect x=\"1\" y=\"1\" width=\"8\" height=\"8\"/></svg>)");
        }

        it("reports parse errors") {
            const char* bad_source = "this is not valid flex syntax {{{";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(bad_source);

            check(compiler.has_error());
            check(binary.empty());
            if (!compiler.has_error()) {
                return;
            }
            const char* error_message = compiler.error_message();
            check(error_message != nullptr);
            if (!error_message) {
                return;
            }
            check(std::strlen(error_message) > 0);
        }

        it("rejects runtime var schemas until the binary format can preserve them") {
            const char* source = R"(
var offset = 12
scene RuntimeInput {
    rect box { x: $offset, y: 0, width: 10, height: 10 }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(compiler.has_error());
            check(binary.empty());
            check_str_contains(compiler.error_message(), "runtime var schemas");
        }

        it("rejects property bindings instead of silently dropping them") {
            const char* source = R"(
scene BoundScene {
    rect box { x: $offset, y: 0, width: 10, height: 10 }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(compiler.has_error());
            check(binary.empty());
            check_str_contains(compiler.error_message(), "property bindings");
        }

        it("rejects component bindings instead of silently dropping them") {
            const char* source = R"(
component Badge {
    width: 10
    rect body { width: $width, height: 10 }
}
scene ComponentScene {
    Badge badge { width: $badgeWidth }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(compiler.has_error());
            check(binary.empty());
            check_str_contains(compiler.error_message(), "component bindings");
        }

        it("rejects state machines instead of silently dropping them") {
            const char* source = R"(
scene StatefulScene {
    rect box { x: 0, y: 0, width: 10, height: 10 }
}
machine visibility {
    layer main {
        state visible { initial: true }
        state hidden {}
        transition visible -> hidden when ${hide}
    }
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(compiler.has_error());
            check(binary.empty());
            check_str_contains(compiler.error_message(), "state machines");
        }

        it("rejects asset manifests instead of silently dropping them") {
            const char* source = R"(
assets {
    image logo: "images/logo.png"
}
scene AssetScene {}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(compiler.has_error());
            check(binary.empty());
            check_str_contains(compiler.error_message(), "asset manifests");
        }

        it("clears stale errors and statistics before each compilation") {
            BinaryCompiler compiler;

            std::vector<uint8_t> failed = compiler.compile_source(nullptr);
            check(failed.empty());
            check(compiler.has_error());

            const char* valid_source = R"(
scene Recovered {
    width: 64
    height: 64
}
)";
            std::vector<uint8_t> recovered = compiler.compile_source(valid_source);

            check(!recovered.empty());
            check(!compiler.has_error());
            check(compiler.stats().source_size == std::strlen(valid_source));
            check(compiler.stats().binary_size == recovered.size());

            std::vector<uint8_t> failed_again = compiler.compile_source("");
            check(failed_again.empty());
            check(compiler.has_error());
            check(compiler.stats().source_size == 0);
            check(compiler.stats().binary_size == 0);
            check(compiler.stats().node_count == 0);
            check(compiler.stats().string_count == 0);
            check(compiler.stats().timeline_count == 0);
        }

        it("rejects an empty output path before reading the input") {
            BinaryCompiler compiler;

            check(!compiler.compile_to_file("missing.flex", nullptr));
            check(compiler.has_error());
            check_str_eq(compiler.error_message(), "Empty output path");
        }

        it("collects compilation statistics") {
            const char* source = R"(
scene Test {
    width: 640
    height: 480
}
)";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);

            check(!compiler.has_error());
            if (compiler.has_error()) {
                return;
            }

            const auto& stats = compiler.stats();
            check(stats.source_size > 0);
            check(stats.binary_size > 0);
            check(stats.binary_size == binary.size());
            check(stats.node_count > 0);
        }
    }

    group("boundary") {
        it("rejects an out-of-bounds string table") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->string_table_offset = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("rejects truncated string table entries") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            auto* label = Text::create(arena);
            label->set_id("label");
            label->set_content("truncate-me");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->string_table_size > 0);
            if (header->string_table_size == 0) {
                return;
            }
            header->string_table_size -= 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("rejects out-of-bounds node data") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->node_data_offset = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("rejects out-of-bounds asset table") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->asset_table_offset = 0xFFFFFFFF;
            header->asset_table_size = 16;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("rejects out-of-bounds fsm data") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->fsm_data_offset = 0xFFFFFFFF;
            header->fsm_data_size = 32;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("rejects out-of-bounds animation data") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);
            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader));
            if (binary.size() < sizeof(FileHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->animation_data_offset = 0xFFFFFFFF;
            header->animation_data_size = 16;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());

            check(!loaded);
        }

        it("returns no timelines when timeline count has no animation data offset") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("opacity")->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->timeline_count == 1);
            check(header->animation_data_size > 0);
            if (header->timeline_count != 1 || header->animation_data_size == 0) {
                return;
            }

            header->animation_data_offset = 0;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 1);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("returns no timelines when animation data exists without timeline count") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("opacity")->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->timeline_count == 1);
            check(header->animation_data_size > 0);
            if (header->timeline_count != 1 || header->animation_data_size == 0) {
                return;
            }

            header->timeline_count = 0;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 0);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("returns no timelines when animation data points at node data") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->node_data_size >= sizeof(NodeHeader));
            if (header->node_data_size < sizeof(NodeHeader)) {
                return;
            }

            header->animation_data_offset = header->node_data_offset;
            header->animation_data_size = sizeof(TimelineHeader);
            header->timeline_count = 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 1);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("returns no timelines when animation data points at the string table") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("opacity")->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->string_table_size >= sizeof(uint32_t));
            if (header->string_table_size < sizeof(uint32_t)) {
                return;
            }

            header->animation_data_offset = header->string_table_offset;
            header->animation_data_size = sizeof(TimelineHeader);
            header->timeline_count = 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 1);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("returns no timelines when animation data truncates a timeline header") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("opacity")->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->animation_data_size = sizeof(TimelineHeader) - 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 1);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("returns no timelines when animation data truncates a track header") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("opacity")->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->animation_data_size = sizeof(TimelineHeader) + sizeof(TrackHeader) - 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            check(reader.timeline_count() == 1);
            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("rejects svg payloads with conflicting source and data flags") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);
            auto* svg = Svg::create(arena);
            svg->set_id("badSvg");
            svg->set_src("icons/check.svg");
            svg->set_width(24.0f);
            svg->set_height(24.0f);
            scene->add_child(svg);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* svg_data = reinterpret_cast<SvgData*>(binary.data() + child->data_offset);
            svg_data->has_source = 1;
            svg_data->has_data = 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("badSvg") == nullptr);
        }

        it("rejects instance payloads with missing input names") {
            ArenaAllocator arena(16384);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* instance = arena.create<InstanceNode>();
            instance->set_id("nested");
            instance->set_source("components/badge.flex");
            instance->set_input("enabled", true);
            scene->add_child(instance);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(InstanceData) + sizeof(InstanceBoolInput));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(InstanceData) + sizeof(InstanceBoolInput)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* data = reinterpret_cast<InstanceData*>(binary.data() + child->data_offset);
            auto* bool_input = reinterpret_cast<InstanceBoolInput*>(binary.data() + child->data_offset + sizeof(InstanceData));
            check(data->bool_input_count == 1);
            if (data->bool_input_count != 1) {
                return;
            }
            bool_input->name = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("nested") == nullptr);
        }

        it("keeps valid scene nodes before a corrupted one") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("label");
            label->set_content("ok");
            scene->add_child(label);

            auto* svg = Svg::create(arena);
            svg->set_id("badSvg");
            svg->set_src("icons/check.svg");
            svg->set_width(24.0f);
            svg->set_height(24.0f);
            scene->add_child(svg);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 3 + sizeof(TextData) + sizeof(SvgData));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 3 + sizeof(TextData) + sizeof(SvgData)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* first_child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* second_child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset + sizeof(NodeHeader) + sizeof(TextData));
            check(first_child->type == flex::binary::NodeType::Text);
            check(second_child->type == flex::binary::NodeType::Svg);
            if (first_child->type != flex::binary::NodeType::Text || second_child->type != flex::binary::NodeType::Svg) {
                return;
            }

            auto* svg_data = reinterpret_cast<SvgData*>(binary.data() + second_child->data_offset);
            svg_data->has_source = 1;
            svg_data->has_data = 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 1);
            auto* loaded_label = dynamic_cast<Text*>(loaded_scene->find("label"));
            check(loaded_label != nullptr);
            if (!loaded_label) {
                return;
            }
            check(loaded_label->content() == "ok");
            check(loaded_scene->find("badSvg") == nullptr);
        }

        it("drops a corrupted nested group while preserving earlier top-level nodes") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* top_label = Text::create(arena);
            top_label->set_id("topLabel");
            top_label->set_content("top");
            scene->add_child(top_label);

            auto* group = Group::create(arena);
            group->set_id("badGroup");

            auto* group_label = Text::create(arena);
            group_label->set_id("groupLabel");
            group_label->set_content("nested");
            group->add_child(group_label);

            auto* bad_svg = Svg::create(arena);
            bad_svg->set_id("groupBadSvg");
            bad_svg->set_src("icons/check.svg");
            bad_svg->set_width(24.0f);
            bad_svg->set_height(24.0f);
            group->add_child(bad_svg);

            scene->add_child(group);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 5 + sizeof(TextData) * 2 + sizeof(SvgData));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 5 + sizeof(TextData) * 2 + sizeof(SvgData)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* first_child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* group_header = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset + sizeof(NodeHeader) + sizeof(TextData));
            check(first_child->type == flex::binary::NodeType::Text);
            check(group_header->type == flex::binary::NodeType::Group);
            if (first_child->type != flex::binary::NodeType::Text ||
                group_header->type != flex::binary::NodeType::Group) {
                return;
            }

            auto* nested_text_header = reinterpret_cast<NodeHeader*>(binary.data() + group_header->data_offset);
            auto* nested_svg_header = reinterpret_cast<NodeHeader*>(binary.data() + group_header->data_offset + sizeof(NodeHeader) + sizeof(TextData));
            check(nested_text_header->type == flex::binary::NodeType::Text);
            check(nested_svg_header->type == flex::binary::NodeType::Svg);
            if (nested_text_header->type != flex::binary::NodeType::Text ||
                nested_svg_header->type != flex::binary::NodeType::Svg) {
                return;
            }

            auto* svg_data = reinterpret_cast<SvgData*>(binary.data() + nested_svg_header->data_offset);
            svg_data->has_source = 1;
            svg_data->has_data = 1;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 1);
            auto* loaded_top = dynamic_cast<Text*>(loaded_scene->find("topLabel"));
            check(loaded_top != nullptr);
            if (!loaded_top) {
                return;
            }
            check(loaded_top->content() == "top");
            check(loaded_scene->find("badGroup") == nullptr);
            check(loaded_scene->find("groupLabel") == nullptr);
            check(loaded_scene->find("groupBadSvg") == nullptr);
        }

        it("drops a nested group when a child shape paint payload truncates") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* top_label = Text::create(arena);
            top_label->set_id("topLabel");
            top_label->set_content("top");
            scene->add_child(top_label);

            auto* group = Group::create(arena);
            group->set_id("badGroup");

            auto* shape = Shape::create(arena);
            shape->set_id("groupShape");
            shape->set_rect(80.0f, 40.0f, 6.0f);
            shape->set_fill(Color(1.0f, 0.2f, 0.2f, 1.0f));
            group->add_child(shape);

            scene->add_child(group);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 4 + sizeof(TextData) + sizeof(ShapeData) +
                sizeof(SolidPaint) + sizeof(flex::binary::RectGeometry));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 4 + sizeof(TextData) + sizeof(ShapeData) +
                sizeof(SolidPaint) + sizeof(flex::binary::RectGeometry)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* first_child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* group_header = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset + sizeof(NodeHeader) + sizeof(TextData));
            check(first_child->type == flex::binary::NodeType::Text);
            check(group_header->type == flex::binary::NodeType::Group);
            if (first_child->type != flex::binary::NodeType::Text ||
                group_header->type != flex::binary::NodeType::Group) {
                return;
            }

            auto* nested_shape_header = reinterpret_cast<NodeHeader*>(binary.data() + group_header->data_offset);
            check(nested_shape_header->type == flex::binary::NodeType::Shape);
            if (nested_shape_header->type != flex::binary::NodeType::Shape) {
                return;
            }

            auto* shape_data = reinterpret_cast<ShapeData*>(binary.data() + nested_shape_header->data_offset);
            check(shape_data->fill_paint_offset > header->node_data_offset);
            if (shape_data->fill_paint_offset <= header->node_data_offset) {
                return;
            }

            header->node_data_size =
                shape_data->fill_paint_offset + sizeof(SolidPaint) - 1 - header->node_data_offset;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 1);
            auto* loaded_top = dynamic_cast<Text*>(loaded_scene->find("topLabel"));
            check(loaded_top != nullptr);
            if (!loaded_top) {
                return;
            }
            check(loaded_top->content() == "top");
            check(loaded_scene->find("badGroup") == nullptr);
            check(loaded_scene->find("groupShape") == nullptr);
        }

        it("drops a nested group when a child shape geometry payload truncates") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* top_label = Text::create(arena);
            top_label->set_id("topLabel");
            top_label->set_content("top");
            scene->add_child(top_label);

            auto* group = Group::create(arena);
            group->set_id("badGroup");

            auto* shape = Shape::create(arena);
            shape->set_id("groupShape");
            shape->set_rect(80.0f, 40.0f, 6.0f);
            shape->set_fill(Color(1.0f, 0.2f, 0.2f, 1.0f));
            group->add_child(shape);

            scene->add_child(group);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 4 + sizeof(TextData) + sizeof(ShapeData) +
                sizeof(SolidPaint) + sizeof(flex::binary::RectGeometry));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 4 + sizeof(TextData) + sizeof(ShapeData) +
                sizeof(SolidPaint) + sizeof(flex::binary::RectGeometry)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            auto* first_child = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset);
            auto* group_header = reinterpret_cast<NodeHeader*>(binary.data() + root->data_offset + sizeof(NodeHeader) + sizeof(TextData));
            check(first_child->type == flex::binary::NodeType::Text);
            check(group_header->type == flex::binary::NodeType::Group);
            if (first_child->type != flex::binary::NodeType::Text ||
                group_header->type != flex::binary::NodeType::Group) {
                return;
            }

            auto* nested_shape_header = reinterpret_cast<NodeHeader*>(binary.data() + group_header->data_offset);
            check(nested_shape_header->type == flex::binary::NodeType::Shape);
            if (nested_shape_header->type != flex::binary::NodeType::Shape) {
                return;
            }

            auto* shape_data = reinterpret_cast<ShapeData*>(binary.data() + nested_shape_header->data_offset);
            check(shape_data->geometry_offset > header->node_data_offset);
            if (shape_data->geometry_offset <= header->node_data_offset) {
                return;
            }

            header->node_data_size =
                shape_data->geometry_offset + sizeof(flex::binary::RectGeometry) - 1 - header->node_data_offset;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 1);
            auto* loaded_top = dynamic_cast<Text*>(loaded_scene->find("topLabel"));
            check(loaded_top != nullptr);
            if (!loaded_top) {
                return;
            }
            check(loaded_top->content() == "top");
            check(loaded_scene->find("badGroup") == nullptr);
            check(loaded_scene->find("groupShape") == nullptr);
        }

        it("keeps parsed root children when root child_count overruns into animation data") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("#rootLabel/opacity")->add_keyframe(0.0f, 1.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->child_count = 2;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 1);
            auto* loaded_label = dynamic_cast<Text*>(loaded_scene->find("rootLabel"));
            check(loaded_label != nullptr);
            if (!loaded_label) {
                return;
            }
            check(loaded_label->content() == "root");
        }

        it("returns an empty scene when the root node is not a scene") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->type = flex::binary::NodeType::Group;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check_close(loaded_scene->width(), 400.0f);
            check_close(loaded_scene->height(), 300.0f);
            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("returns an empty scene when root child data offset is missing") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->child_count = 1;
            root->data_offset = 0;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("returns an empty scene when root child data offset is out of bounds") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->child_count = 1;
            root->data_offset = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("returns an empty scene when root child data offset points at the string table") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->string_table_size > 0);
            if (header->string_table_size == 0) {
                return;
            }

            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->data_offset = header->string_table_offset;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("returns an empty scene when root child data offset points at animation data") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            auto timeline = Timeline::create("fade", arena);
            timeline->add_track("#rootLabel/opacity")->add_keyframe(0.0f, 1.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            check(header->animation_data_size >= sizeof(TimelineHeader));
            if (header->animation_data_size < sizeof(TimelineHeader)) {
                return;
            }

            auto* root = reinterpret_cast<NodeHeader*>(binary.data() + header->node_data_offset);
            root->data_offset = header->animation_data_offset;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("returns an empty scene when node section truncates a child payload") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(400.0f, 300.0f, arena);

            auto* label = Text::create(arena);
            label->set_id("rootLabel");
            label->set_content("root");
            scene->add_child(label);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene);
            check(binary.size() >= sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData));
            if (binary.size() < sizeof(FileHeader) + sizeof(NodeHeader) * 2 + sizeof(TextData)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->node_data_size = sizeof(NodeHeader) * 2; // Exclude trailing TextData payload

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto loaded_scene = reader.create_scene();
            check(loaded_scene != nullptr);
            if (!loaded_scene) {
                return;
            }

            check(loaded_scene->root()->child_count() == 0);
            check(loaded_scene->find("rootLabel") == nullptr);
        }

        it("rejects timelines with missing names") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("fade", arena);
            auto track = timeline->add_track("#box/opacity");
            track->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* timeline_header = reinterpret_cast<TimelineHeader*>(binary.data() + header->animation_data_offset);
            timeline_header->name = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("rejects tracks with missing property names") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("fade", arena);
            auto track = timeline->add_track("#box/opacity");
            track->add_keyframe(0.0f, 0.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader));
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader)) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* track_header = reinterpret_cast<TrackHeader*>(binary.data() + header->animation_data_offset + sizeof(TimelineHeader));
            track_header->property = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("keeps valid timelines before a corrupted one") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto fade = Timeline::create("fade", arena);
            fade->add_track("#box/opacity")->add_keyframe(0.0f, 0.0f);

            auto move = Timeline::create("move", arena);
            move->add_track("#box/x")->add_keyframe(0.0f, 10.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {fade, move});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) * 2 + sizeof(TrackHeader) * 2 + sizeof(flex::binary::Keyframe) * 2);
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) * 2 + sizeof(TrackHeader) * 2 + sizeof(flex::binary::Keyframe) * 2) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            uint8_t* second_timeline = binary.data() + header->animation_data_offset +
                                       sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe);
            auto* second_track = reinterpret_cast<TrackHeader*>(second_timeline + sizeof(TimelineHeader));
            second_track->property = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.size() == 1);
            if (timelines.size() != 1) {
                return;
            }

            check(std::string(timelines[0]->name()) == "fade");
            check(timelines[0]->tracks().size() == 1);
            if (timelines[0]->tracks().size() != 1) {
                return;
            }
            check(std::string(timelines[0]->tracks()[0]->property()) == "#box/opacity");
        }

        it("drops a timeline when a later track is corrupted") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("combo", arena);
            timeline->add_track("#box/opacity")->add_keyframe(0.0f, 0.0f);
            timeline->add_track("#box/x")->add_keyframe(0.0f, 10.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) * 2 + sizeof(flex::binary::Keyframe) * 2);
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) * 2 + sizeof(flex::binary::Keyframe) * 2) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* second_track = reinterpret_cast<TrackHeader*>(
                binary.data() + header->animation_data_offset +
                sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe));
            second_track->property = 0xFFFFFFFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("drops a timeline when a later keyframe is corrupted") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("fade", arena);
            auto track = timeline->add_track("#box/opacity");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 1.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe) * 2);
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe) * 2) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            auto* second_keyframe = reinterpret_cast<flex::binary::Keyframe*>(
                binary.data() + header->animation_data_offset +
                sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe));
            second_keyframe->value_type = 0xFF;

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }

        it("drops timelines when animation section truncates a keyframe") {
            ArenaAllocator arena(8192);
            auto scene = Scene::create(640.0f, 360.0f, arena);

            auto* box = Shape::create(arena);
            box->set_id("box");
            box->set_rect(40.0f, 40.0f, 0.0f);
            scene->add_child(box);

            auto timeline = Timeline::create("fade", arena);
            auto track = timeline->add_track("#box/opacity");
            track->add_keyframe(0.0f, 0.0f);
            track->add_keyframe(1.0f, 1.0f);

            BinaryWriter writer;
            std::vector<uint8_t> binary = writer.write(scene, {timeline});
            check(binary.size() >= sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe) * 2);
            if (binary.size() < sizeof(FileHeader) + sizeof(TimelineHeader) + sizeof(TrackHeader) + sizeof(flex::binary::Keyframe) * 2) {
                return;
            }

            auto* header = reinterpret_cast<FileHeader*>(binary.data());
            header->animation_data_size -= sizeof(flex::binary::Keyframe);

            header->checksum = 0;
            header->checksum = calculate_crc32(binary.data(), binary.size());

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(loaded);
            if (!loaded) {
                return;
            }

            auto timelines = reader.create_timelines();
            check(timelines.empty());
        }
    }

    group("compression") {
        it("reduces size when enabled") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(1920.0f, 1080.0f, arena);

            BinaryWriter writer_uncompressed;
            std::vector<uint8_t> binary_uncompressed = writer_uncompressed.write(scene);

            BinaryWriter writer_compressed;
            writer_compressed.set_compress(true);
            std::vector<uint8_t> binary_compressed = writer_compressed.write(scene);

            check(!binary_uncompressed.empty());
            check(!binary_compressed.empty());
            check(binary_compressed.size() <= binary_uncompressed.size());
            if (binary_uncompressed.empty() || binary_compressed.empty()) {
                return;
            }
            check(binary_uncompressed.size() >= sizeof(FileHeader));
            check(binary_compressed.size() >= sizeof(FileHeader));
            if (binary_uncompressed.size() < sizeof(FileHeader) || binary_compressed.size() < sizeof(FileHeader)) {
                return;
            }

            const FileHeader* header_uncompressed = reinterpret_cast<const FileHeader*>(binary_uncompressed.data());
            const FileHeader* header_compressed = reinterpret_cast<const FileHeader*>(binary_compressed.data());

            check((header_uncompressed->flags & FLAG_COMPRESSED) == 0);
            if (binary_compressed.size() < binary_uncompressed.size()) {
                check((header_compressed->flags & FLAG_COMPRESSED) != 0);
            }
        }

        it("loads a compressed file") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            BinaryWriter writer;
            writer.set_compress(true);
            std::vector<uint8_t> binary = writer.write(scene);

            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            check_close(loaded->width(), 800.0f);
            check_close(loaded->height(), 600.0f);
        }

        it("rejects corrupted compressed payloads") {
            ArenaAllocator arena(4096);
            auto scene = Scene::create(800.0f, 600.0f, arena);

            BinaryWriter writer;
            writer.set_compress(true);
            std::vector<uint8_t> binary = writer.write(scene);

            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            auto* header = reinterpret_cast<const FileHeader*>(binary.data());
            check((header->flags & FLAG_COMPRESSED) != 0);
            if ((header->flags & FLAG_COMPRESSED) == 0) {
                return;
            }

            binary.back() ^= 0x5A;

            BinaryReader reader;
            bool loaded = reader.load_memory(binary.data(), binary.size());
            check(!loaded);
        }

        it("compiles with compression") {
            const char* source = R"(
scene TestScene {
    width: 1024
    height: 768
}
)";

            BinaryCompiler compiler_uncompressed;
            std::vector<uint8_t> binary_uncompressed = compiler_uncompressed.compile_source(source);

            BinaryCompiler compiler_compressed;
            compiler_compressed.set_compress(true);
            std::vector<uint8_t> binary_compressed = compiler_compressed.compile_source(source);

            check(!compiler_uncompressed.has_error());
            check(!compiler_compressed.has_error());
            check(!binary_uncompressed.empty());
            check(!binary_compressed.empty());
            check(binary_compressed.size() <= binary_uncompressed.size());
            if (compiler_uncompressed.has_error() || compiler_compressed.has_error() ||
                binary_uncompressed.empty() || binary_compressed.empty()) {
                return;
            }

            BinaryReader reader_uncompressed;
            bool loaded_uncompressed = reader_uncompressed.load_memory(binary_uncompressed.data(), binary_uncompressed.size());
            check(loaded_uncompressed);
            if (!loaded_uncompressed) {
                return;
            }

            BinaryReader reader_compressed;
            bool loaded_compressed = reader_compressed.load_memory(binary_compressed.data(), binary_compressed.size());
            check(loaded_compressed);
            if (!loaded_compressed) {
                return;
            }

            auto scene_uncompressed = reader_uncompressed.create_scene();
            auto scene_compressed = reader_compressed.create_scene();

            check(scene_uncompressed != nullptr);
            check(scene_compressed != nullptr);
            if (!scene_uncompressed || !scene_compressed) {
                return;
            }

            check_close(scene_uncompressed->width(), scene_compressed->width());
            check_close(scene_uncompressed->height(), scene_compressed->height());
        }

        it("round-trips compressed data") {
            ArenaAllocator arena(4096);
            float width = 1280.0f;
            float height = 720.0f;
            auto scene = Scene::create(width, height, arena);

            BinaryWriter writer;
            writer.set_compress(true);
            std::vector<uint8_t> binary = writer.write(scene);

            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            BinaryReader reader;
            bool loaded_ok = reader.load_memory(binary.data(), binary.size());
            check(loaded_ok);
            if (!loaded_ok) {
                return;
            }

            check_close(reader.canvas_width(), width);
            check_close(reader.canvas_height(), height);

            auto loaded = reader.create_scene();
            check(loaded != nullptr);
            if (!loaded) {
                return;
            }

            check_close(loaded->width(), width);
            check_close(loaded->height(), height);
        }
    }

    group("definition facade") {
        it("loads binary data through Definition facade") {
            const char* source = R"(
                scene BinaryFacade {
                    width: 640
                    height: 360
                }
            )";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            auto definition = Definition::load_binary_data(binary.data(), binary.size());
            check(definition != nullptr);
            if (!definition) {
                return;
            }

            check(!definition->has_error());
            check(definition->scene() != nullptr);
            if (!definition->scene()) {
                return;
            }

            check_close(definition->scene()->width(), 640.0f);
            check_close(definition->scene()->height(), 360.0f);
        }

        it("loads binary file through Definition facade") {
            const char* source = R"(
                scene BinaryFileFacade {
                    width: 320
                    height: 240
                }
            )";

            BinaryCompiler compiler;
            std::vector<uint8_t> binary = compiler.compile_source(source);
            check(!binary.empty());
            if (binary.empty()) {
                return;
            }

            TempBinaryFile temp(binary);
            auto definition = Definition::load_binary(temp.path.string().c_str());
            check(definition != nullptr);
            if (!definition) {
                return;
            }

            check(!definition->has_error());
            check(definition->scene() != nullptr);
            if (!definition->scene()) {
                return;
            }

            check_close(definition->scene()->width(), 320.0f);
            check_close(definition->scene()->height(), 240.0f);
        }
    }
}
