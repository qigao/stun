#include "flexchart/geoshape/geoshape_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include "turbo_parser.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" void flexchart_geoshape_force_link(void) {}

namespace flex {
namespace chart {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool append_geojson_ring(json_value_t* ring, const std::string& projection,
                         float width, float height, std::string& path) {
    if (!ring || turbo_json_type(ring) != TURBO_JSON_ARRAY ||
        turbo_json_array_size(ring) < 3) return false;

    char command[96];
    for (size_t i = 0; i < turbo_json_array_size(ring); ++i) {
        json_value_t* coordinate = turbo_json_array_get(ring, i);
        if (!coordinate || turbo_json_type(coordinate) != TURBO_JSON_ARRAY ||
            turbo_json_array_size(coordinate) < 2) return false;
        json_value_t* longitude_value = turbo_json_array_get(coordinate, 0);
        json_value_t* latitude_value = turbo_json_array_get(coordinate, 1);
        if (turbo_json_type(longitude_value) != TURBO_JSON_NUMBER ||
            turbo_json_type(latitude_value) != TURBO_JSON_NUMBER) return false;

        const double longitude = turbo_json_number(longitude_value);
        const double latitude = std::clamp(turbo_json_number(latitude_value), -85.0, 85.0);
        const float x = static_cast<float>((longitude + 180.0) / 360.0 * width);
        float y;
        if (projection == "mercator") {
            const double radians = latitude * kPi / 180.0;
            const double normalized =
                (1.0 - std::log(std::tan(kPi / 4.0 + radians / 2.0)) / kPi) / 2.0;
            y = static_cast<float>(normalized * height);
        } else {
            y = static_cast<float>((90.0 - latitude) / 180.0 * height);
        }
        std::snprintf(command, sizeof(command), i == 0 ? "M %.4f %.4f" : " L %.4f %.4f", x, y);
        path += command;
    }
    path += " Z";
    return true;
}

bool append_geojson_polygon(json_value_t* polygon, const std::string& projection,
                            float width, float height, std::string& path) {
    if (!polygon || turbo_json_type(polygon) != TURBO_JSON_ARRAY) return false;
    bool appended = false;
    for (size_t i = 0; i < turbo_json_array_size(polygon); ++i) {
        appended = append_geojson_ring(turbo_json_array_get(polygon, i), projection,
                                       width, height, path) || appended;
    }
    return appended;
}

bool geojson_to_path(const std::string& source, const std::string& projection,
                     float width, float height, std::string& path) {
    json_value_t* root = nullptr;
    if (turbo_parse_json(reinterpret_cast<const uint8_t*>(source.data()),
                         source.size(), &root) != 0 || !root) return false;

    if (turbo_json_type(root) != TURBO_JSON_OBJECT) {
        turbo_free_json(&root);
        return false;
    }

    json_value_t* geometry = root;
    json_value_t* root_type = turbo_json_object_get(root, "type");
    if (root_type && turbo_json_type(root_type) == TURBO_JSON_STRING &&
        std::strcmp(turbo_json_string(root_type), "Feature") == 0) {
        geometry = turbo_json_object_get(root, "geometry");
    }

    bool converted = false;
    if (geometry && turbo_json_type(geometry) == TURBO_JSON_OBJECT) {
        json_value_t* type = turbo_json_object_get(geometry, "type");
        json_value_t* coordinates = turbo_json_object_get(geometry, "coordinates");
        if (type && turbo_json_type(type) == TURBO_JSON_STRING) {
            const char* type_name = turbo_json_string(type);
            if (std::strcmp(type_name, "Polygon") == 0) {
                converted = append_geojson_polygon(coordinates, projection, width, height, path);
            } else if (std::strcmp(type_name, "MultiPolygon") == 0 && coordinates &&
                       turbo_json_type(coordinates) == TURBO_JSON_ARRAY) {
                for (size_t i = 0; i < turbo_json_array_size(coordinates); ++i) {
                    converted = append_geojson_polygon(turbo_json_array_get(coordinates, i),
                                                       projection, width, height, path) || converted;
                }
            }
        }
    }
    turbo_free_json(&root);
    return converted;
}

} // namespace

class GeoshapeMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override {
        if (!mark || records.empty() || !ctx.overlay) return;

        std::string shape_field;
        for (const auto& enc : mark->encodings) {
            if (enc->channel == "shape") shape_field = enc->field;
        }
        if (shape_field.empty()) return;

        const Color palette[] = {
            Color(0.24f, 0.51f, 1.0f, 0.65f),
            Color(0.12f, 0.73f, 0.52f, 0.65f),
            Color(1.0f, 0.75f, 0.04f, 0.65f)
        };
        for (size_t i = 0; i < records.size(); ++i) {
            const std::string shape_data = get_string_val(records[i].get(shape_field));
            if (shape_data.empty()) continue;

            std::string path;
            if (shape_data[0] == 'M' || shape_data[0] == 'm') {
                path = shape_data;
            } else {
                std::string projection = "equirectangular";
                auto projection_style = mark->styles.find("projection");
                if (projection_style != mark->styles.end())
                    projection = get_string_val(projection_style->second);
                if (!geojson_to_path(shape_data, projection, ctx.available_plot_w,
                                     ctx.estimated_plot_h, path)) continue;
            }

            auto shape = ctx.arena.create<Shape>();
            if (!shape) return;
            shape->set_id("geoshape-mark-" + std::to_string(i));
            shape->set_path(path);
            shape->set_anchor(Anchor::TopLeft);
            shape->set_position(0, 0);
            shape->set_fill(palette[i % 3]);
            shape->set_stroke(Color(1.0f, 1.0f, 1.0f), 1.0f);
            shape->set_position_absolute(true);
            ctx.overlay->add_child(shape);
        }
    }
};

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("geoshape", flex::chart::GeoshapeMarkRenderer)
