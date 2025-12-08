#include <cmath>
#include <flexchart/flexchart.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

using json = nlohmann::json;

namespace flexchart {

struct ProvinceGeometry {
  std::string name;
  std::string id;
  std::vector<std::vector<std::pair<double, double>>> coordinates;
  double centerLon;
  double centerLat;
};

static std::vector<ProvinceGeometry> loadChinaMapData(const std::string& jsonPath) {
  std::vector<ProvinceGeometry> data;
  
  std::ifstream file(jsonPath);
  if (!file.is_open()) {
    return data;
  }
  
  try {
    json j = json::parse(file);
    
    if (!j.contains("features") || !j["features"].is_array()) {
      return data;
    }
    
    for (const auto& feature : j["features"]) {
      if (!feature.contains("properties") || !feature.contains("geometry")) {
        continue;
      }
      
      ProvinceGeometry province;
      const auto& props = feature["properties"];
      const auto& geom = feature["geometry"];
      
      province.name = props.value("name", "");
      province.id = props.value("id", "");
      
      if (props.contains("cp") && props["cp"].is_array() && props["cp"].size() >= 2) {
        province.centerLon = props["cp"][0].get<double>();
        province.centerLat = props["cp"][1].get<double>();
      }
      
      if (geom.contains("coordinates") && geom["coordinates"].is_array()) {
        const auto& coords = geom["coordinates"];
        
        if (geom["type"] == "Polygon" && coords.size() > 0) {
          for (const auto& ring : coords) {
            if (ring.is_array()) {
              std::vector<std::pair<double, double>> polygon;
              for (const auto& point : ring) {
                if (point.is_array() && point.size() >= 2) {
                  polygon.push_back({point[0].get<double>(), point[1].get<double>()});
                }
              }
              if (!polygon.empty()) {
                province.coordinates.push_back(polygon);
              }
            }
          }
        }
        else if (geom["type"] == "MultiPolygon") {
          for (const auto& polygon_group : coords) {
            if (polygon_group.is_array()) {
              for (const auto& ring : polygon_group) {
                if (ring.is_array()) {
                  std::vector<std::pair<double, double>> polygon;
                  for (const auto& point : ring) {
                    if (point.is_array() && point.size() >= 2) {
                      polygon.push_back({point[0].get<double>(), point[1].get<double>()});
                    }
                  }
                  if (!polygon.empty()) {
                    province.coordinates.push_back(polygon);
                  }
                }
              }
            }
          }
        }
      }
      
      if (!province.coordinates.empty()) {
        data.push_back(province);
      }
    }
  } catch (const std::exception& e) {
    data.clear();
  }
  
  return data;
}

static std::vector<ProvinceGeometry> CHINA_MAP_DATA;
static bool CHINA_MAP_DATA_LOADED = false;

static const std::vector<ProvinceGeometry>& getChinaMapData() {
  if (!CHINA_MAP_DATA_LOADED) {
    CHINA_MAP_DATA = loadChinaMapData("china.json");
    CHINA_MAP_DATA_LOADED = true;
  }
  return CHINA_MAP_DATA;
}

static std::unordered_map<std::string, std::string> PROVINCE_NAME_MAP = {
    {"新疆", "新疆维吾尔自治区"}, {"西藏", "西藏自治区"}, {"内蒙古", "内蒙古自治区"},
    {"青海", "青海省"},           {"四川", "四川省"},     {"黑龙江", "黑龙江省"},
    {"甘肃", "甘肃省"},           {"云南", "云南省"},     {"广西", "广西壮族自治区"},
    {"湖南", "湖南省"},           {"陕西", "陕西省"},     {"广东", "广东省"},
    {"吉林", "吉林省"},           {"河北", "河北省"},     {"湖北", "湖北省"},
    {"贵州", "贵州省"},           {"山东", "山东省"},     {"江西", "江西省"},
    {"河南", "河南省"},           {"辽宁", "辽宁省"},     {"山西", "山西省"},
    {"安徽", "安徽省"},           {"福建", "福建省"},     {"浙江", "浙江省"},
    {"江苏", "江苏省"},           {"重庆", "重庆市"},     {"宁夏", "宁夏回族自治区"},
    {"海南", "海南省"},           {"台湾", "台湾省"},     {"北京", "北京市"},
    {"天津", "天津市"},           {"上海", "上海市"},     {"香港", "香港特别行政区"},
    {"澳门", "澳门特别行政区"},
};

static Color interpolateColor(const Color &low, const Color &high, float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  return {static_cast<uint8_t>(low.r + (high.r - low.r) * t),
          static_cast<uint8_t>(low.g + (high.g - low.g) * t),
          static_cast<uint8_t>(low.b + (high.b - low.b) * t),
          static_cast<uint8_t>(low.a + (high.a - low.a) * t)};
}

static bool pointInPolygon(float x, float y, const std::vector<std::pair<double, double>>& polygon) {
  int n = polygon.size();
  if (n < 3) return false;
  
  bool inside = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    float xi = polygon[i].first, yi = polygon[i].second;
    float xj = polygon[j].first, yj = polygon[j].second;
    
    if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
      inside = !inside;
    }
  }
  return inside;
}

int findHoveredProvinceIndex(float mouseX, float mouseY, float chartX, float chartY, 
                              float chartW, float chartH) {
  const double MIN_LON = 73.0;
  const double MAX_LON = 136.0;
  const double MIN_LAT = 18.0;
  const double MAX_LAT = 54.0;
  
  float relX = (mouseX - chartX) / chartW;
  float relY = (mouseY - chartY) / chartH;
  
  if (relX < 0 || relX > 1 || relY < 0 || relY > 1) {
    return -1;
  }
  
  double lon = MIN_LON + relX * (MAX_LON - MIN_LON);
  double lat = MAX_LAT - relY * (MAX_LAT - MIN_LAT);
  
  const auto& mapData = getChinaMapData();
  for (size_t i = 0; i < mapData.size(); i++) {
    const auto& province = mapData[i];
    for (const auto& polygon : province.coordinates) {
      if (pointInPolygon(lon, lat, polygon)) {
        return static_cast<int>(i);
      }
    }
  }
  
  return -1;
}

void renderMapChinaSeries(NVGcontext *vg, const SeriesData &series, float x, float y, float w,
                          float h, int hoveredIdx) {
  const double MIN_LON = 73.0;
  const double MAX_LON = 136.0;
  const double MIN_LAT = 18.0;
  const double MAX_LAT = 54.0;

  std::unordered_map<std::string, double> valueMap;
  double minValue = 1e9;
  double maxValue = -1e9;

  for (const auto &item : series.mapData) {
    std::string fullName = item.name;
    auto it = PROVINCE_NAME_MAP.find(item.name);
    if (it != PROVINCE_NAME_MAP.end()) {
      fullName = it->second;
    }
    valueMap[fullName] = item.value;
    minValue = std::min(minValue, item.value);
    maxValue = std::max(maxValue, item.value);
  }

  if (maxValue <= minValue) {
    maxValue = minValue + 1.0;
  }

  const auto& mapData = getChinaMapData();
  
  std::string hoveredProvinceName;
  float hoveredCenterX = 0, hoveredCenterY = 0;
  double hoveredValue = 0;
  bool hasHoveredTooltip = false;
  
  for (size_t provinceIdx = 0; provinceIdx < mapData.size(); provinceIdx++) {
    const auto &province = mapData[provinceIdx];
    auto valueIt = valueMap.find(province.name);
    Color fillColor = series.mapLowColor;

    if (valueIt != valueMap.end()) {
      float t = (valueIt->second - minValue) / (maxValue - minValue);
      fillColor = interpolateColor(series.mapLowColor, series.mapHighColor, t);
    }

    bool isHovered = (hoveredIdx >= 0 && static_cast<size_t>(hoveredIdx) == provinceIdx);
    
    if (isHovered && valueIt != valueMap.end()) {
      hoveredProvinceName = province.name;
      hoveredCenterX = x + (province.centerLon - MIN_LON) / (MAX_LON - MIN_LON) * w;
      hoveredCenterY = y + h - (province.centerLat - MIN_LAT) / (MAX_LAT - MIN_LAT) * h;
      hoveredValue = valueIt->second;
      hasHoveredTooltip = true;
    }

    for (const auto &polygon : province.coordinates) {
      nvgBeginPath(vg);
      bool first = true;
      for (const auto &coord : polygon) {
        float px = x + (coord.first - MIN_LON) / (MAX_LON - MIN_LON) * w;
        float py = y + h - (coord.second - MIN_LAT) / (MAX_LAT - MIN_LAT) * h;

        if (first) {
          nvgMoveTo(vg, px, py);
          first = false;
        } else {
          nvgLineTo(vg, px, py);
        }
      }
      nvgClosePath(vg);
      
      if (isHovered) {
        Color hoverColor = {
          static_cast<uint8_t>(std::min(255, fillColor.r + 40)),
          static_cast<uint8_t>(std::min(255, fillColor.g + 40)),
          static_cast<uint8_t>(std::min(255, fillColor.b + 40)),
          fillColor.a
        };
        nvgFillColor(vg, hoverColor.toNVG());
      } else {
        nvgFillColor(vg, fillColor.toNVG());
      }
      nvgFill(vg);

      if (isHovered) {
        nvgStrokeColor(vg, nvgRGBA(255, 200, 0, 255));
        nvgStrokeWidth(vg, 2.5f);
      } else {
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 180));
        nvgStrokeWidth(vg, 1.0f);
      }
      nvgStroke(vg);
    }

    if (series.mapShowLabel && valueIt != valueMap.end()) {
      float cx = x + (province.centerLon - MIN_LON) / (MAX_LON - MIN_LON) * w;
      float cy = y + h - (province.centerLat - MIN_LAT) / (MAX_LAT - MIN_LAT) * h;

      float fontSize = std::min(14.0f, w * 0.018f);
      nvgFontSize(vg, fontSize);
      nvgFontFace(vg, "sans-serif");
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, nvgRGBA(60, 60, 60, 255));
      nvgText(vg, cx, cy, province.name.c_str(), nullptr);
    }
  }
  
  if (hasHoveredTooltip) {
    char tooltip[128];
    snprintf(tooltip, sizeof(tooltip), "%s: %.1f", hoveredProvinceName.c_str(), hoveredValue);
    
    float tooltipFontSize = std::min(18.0f, w * 0.022f);
    nvgFontSize(vg, tooltipFontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    
    float bounds[4];
    nvgTextBounds(vg, hoveredCenterX, hoveredCenterY, tooltip, nullptr, bounds);
    float tw = bounds[2] - bounds[0];
    float th = bounds[3] - bounds[1];
    
    nvgBeginPath(vg);
    nvgRoundedRect(vg, hoveredCenterX - tw/2 - 8, hoveredCenterY - th/2 - 6, tw + 16, th + 12, 4);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 245));
    nvgFill(vg);
    
    nvgStrokeColor(vg, nvgRGBA(200, 200, 200, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
    
    nvgFillColor(vg, nvgRGBA(40, 40, 40, 255));
    nvgText(vg, hoveredCenterX, hoveredCenterY, tooltip, nullptr);
  }
}

} // namespace flexchart