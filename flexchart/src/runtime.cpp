#include <flexchart/runtime.h>
#include <nlohmann/json.hpp>
#include <fmtlog.h>
#include <iostream>
#include <functional>

using json = nlohmann::json;

namespace flexchart {

static JSClassID js_chart_class_id = 0;

ChartManager& ChartManager::instance() {
    static ChartManager mgr;
    return mgr;
}

void ChartManager::init(JSContext* ctx, cssboxRenderer* renderer) {
    ctx_ = ctx;
    renderer_ = renderer;
}

void ChartManager::shutdown() {
    charts_.clear();
}

FlexChart* ChartManager::createChart(const std::string& containerId) {
    auto it = charts_.find(containerId);
    if (it != charts_.end()) {
        return it->second.get();
    }
    
    auto chart = std::make_unique<FlexChart>(renderer_, containerId);
    auto* ptr = chart.get();
    charts_[containerId] = std::move(chart);
    return ptr;
}

FlexChart* ChartManager::getChart(const std::string& containerId) {
    auto it = charts_.find(containerId);
    return (it != charts_.end()) ? it->second.get() : nullptr;
}

void ChartManager::destroyChart(const std::string& containerId) {
    charts_.erase(containerId);
}

void ChartManager::renderAll(NVGcontext* vg) {
    for (auto& [id, chart] : charts_) {
        chart->draw(vg);
    }
}

bool ChartManager::handleMouseMove(float x, float y) {
    bool handled = false;
    for (auto& [id, chart] : charts_) {
        if (chart->handleMouseMove(x, y)) {
            handled = true;
        }
    }
    return handled;
}

bool ChartManager::handleMouseDown(float x, float y) {
    for (auto& [id, chart] : charts_) {
        if (chart->handleMouseDown(x, y)) {
            return true;
        }
    }
    return false;
}

static Color parseColor(const json& j) {
    if (j.is_string()) {
        return Color::fromHex(j.get<std::string>());
    }
    if (j.is_array() && j.size() >= 3) {
        return Color::fromRGBA(
            j[0].get<uint8_t>(),
            j[1].get<uint8_t>(),
            j[2].get<uint8_t>(),
            j.size() > 3 ? j[3].get<uint8_t>() : 255
        );
    }
    return {91, 143, 249, 255};
}

static SeriesType parseSeriesType(const std::string& type) {
    if (type == "bar") return SeriesType::Bar;
    if (type == "pie") return SeriesType::Pie;
    if (type == "scatter") return SeriesType::Scatter;
    if (type == "area") return SeriesType::Area;
    if (type == "radar") return SeriesType::Radar;
    if (type == "gauge") return SeriesType::Gauge;
    if (type == "funnel") return SeriesType::Funnel;
    if (type == "candlestick") return SeriesType::Candlestick;
    if (type == "heatmap") return SeriesType::Heatmap;
    if (type == "treemap") return SeriesType::Treemap;
    if (type == "boxplot") return SeriesType::BoxPlot;
    if (type == "waterfall") return SeriesType::Waterfall;
    if (type == "sunburst") return SeriesType::Sunburst;
    if (type == "polar") return SeriesType::Polar;
    if (type == "ring") return SeriesType::Ring;
    if (type == "parallel") return SeriesType::Parallel;
    if (type == "sankey") return SeriesType::Sankey;
    if (type == "graph") return SeriesType::Graph;
    if (type == "calendar") return SeriesType::Calendar;
    if (type == "themeRiver") return SeriesType::ThemeRiver;
    if (type == "pictorialBar") return SeriesType::PictorialBar;
    if (type == "liquidfill") return SeriesType::Liquidfill;
    if (type == "tree") return SeriesType::Tree;
    if (type == "bullet") return SeriesType::Bullet;
    if (type == "nightingale") return SeriesType::Nightingale;
    if (type == "histogram") return SeriesType::Histogram;
    if (type == "step") return SeriesType::Step;
    if (type == "barStack") return SeriesType::BarStack;
    if (type == "areaStack") return SeriesType::AreaStack;
    if (type == "bubble") return SeriesType::Bubble;
    if (type == "lollipop") return SeriesType::Lollipop;
    if (type == "dumbbell") return SeriesType::Dumbbell;
    if (type == "rangeBar") return SeriesType::RangeBar;
    if (type == "gantt") return SeriesType::Gantt;
    if (type == "waffle") return SeriesType::Waffle;
    if (type == "radialBar") return SeriesType::RadialBar;
    if (type == "pyramid") return SeriesType::Pyramid;
    if (type == "violin") return SeriesType::Violin;
    if (type == "errorBar") return SeriesType::ErrorBar;
    if (type == "slope") return SeriesType::Slope;
    if (type == "dot") return SeriesType::Dot;
    if (type == "mapChina") return SeriesType::MapChina;
    return SeriesType::Line;
}

ChartOption parseOptionFromString(const std::string& jsonStr) {
    ChartOption opt;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("title")) {
            auto& t = j["title"];
            if (t.contains("text")) opt.title.text = t["text"].get<std::string>();
            if (t.contains("subtext")) opt.title.subtext = t["subtext"].get<std::string>();
            if (t.contains("fontSize")) opt.title.fontSize = t["fontSize"].get<float>();
        }
        
        if (j.contains("legend")) {
            auto& l = j["legend"];
            if (l.contains("show")) opt.legend.show = l["show"].get<bool>();
            if (l.contains("orient")) opt.legend.orient = l["orient"].get<std::string>();
            if (l.contains("left")) opt.legend.left = l["left"].get<std::string>();
            if (l.contains("top")) opt.legend.top = l["top"].get<std::string>();
        }
        
        if (j.contains("tooltip")) {
            auto& t = j["tooltip"];
            if (t.contains("show")) opt.tooltip.show = t["show"].get<bool>();
            if (t.contains("trigger")) opt.tooltip.trigger = t["trigger"].get<std::string>();
        }
        
        if (j.contains("grid")) {
            auto& g = j["grid"];
            if (g.contains("left")) opt.grid.left = g["left"].get<float>();
            if (g.contains("right")) opt.grid.right = g["right"].get<float>();
            if (g.contains("top")) opt.grid.top = g["top"].get<float>();
            if (g.contains("bottom")) opt.grid.bottom = g["bottom"].get<float>();
        }
        
        if (j.contains("xAxis")) {
            auto& x = j["xAxis"];
            if (x.contains("type")) opt.xAxis.type = x["type"].get<std::string>();
            if (x.contains("show")) opt.xAxis.show = x["show"].get<bool>();
            if (x.contains("data") && x["data"].is_array()) {
                for (const auto& item : x["data"]) {
                    opt.xAxis.data.push_back(item.get<std::string>());
                }
            }
        }
        
        if (j.contains("yAxis")) {
            auto& y = j["yAxis"];
            if (y.contains("type")) opt.yAxis.type = y["type"].get<std::string>();
            if (y.contains("show")) opt.yAxis.show = y["show"].get<bool>();
            if (y.contains("min")) opt.yAxis.min = y["min"].get<double>();
            if (y.contains("max")) opt.yAxis.max = y["max"].get<double>();
            if (y.contains("splitLine")) opt.yAxis.splitLine = y["splitLine"].get<bool>();
        }
        
        if (j.contains("series") && j["series"].is_array()) {
            for (const auto& s : j["series"]) {
                SeriesData sd;
                
                if (s.contains("type")) sd.type = parseSeriesType(s["type"].get<std::string>());
                if (s.contains("name")) sd.name = s["name"].get<std::string>();
                if (s.contains("smooth")) sd.smooth = s["smooth"].get<bool>();
                if (s.contains("showSymbol")) sd.showSymbol = s["showSymbol"].get<bool>();
                if (s.contains("symbolSize")) sd.symbolSize = s["symbolSize"].get<float>();
                if (s.contains("lineWidth")) sd.lineWidth = s["lineWidth"].get<float>();
                if (s.contains("areaStyle")) sd.areaStyle = true;
                if (s.contains("barWidth")) sd.barWidth = s["barWidth"].get<float>();
                if (s.contains("radius") && s["radius"].is_array() && s["radius"].size() >= 2) {
                    std::string inner = s["radius"][0].get<std::string>();
                    if (inner.back() == '%') {
                        sd.innerRadius = std::stof(inner.substr(0, inner.size()-1)) / 100.0f;
                    }
                }
                if (s.contains("color")) sd.color = parseColor(s["color"]);
                
                // Gauge specific
                if (s.contains("min")) sd.gaugeMin = s["min"].get<double>();
                if (s.contains("max")) sd.gaugeMax = s["max"].get<double>();
                if (s.contains("startAngle")) sd.startAngle = s["startAngle"].get<float>();
                if (s.contains("endAngle")) sd.endAngle = s["endAngle"].get<float>();
                if (s.contains("detail")) {
                    if (s["detail"].is_object() && s["detail"].contains("show")) {
                        sd.showGaugeDetail = s["detail"]["show"].get<bool>();
                    }
                }
                
                // Funnel specific
                if (s.contains("sort")) sd.funnelSort = s["sort"].get<std::string>();
                if (s.contains("gap")) sd.funnelGap = s["gap"].get<float>();
                
                // Radar specific
                if (s.contains("radarIndicator") && s["radarIndicator"].is_array()) {
                    for (const auto& ind : s["radarIndicator"]) {
                        RadarIndicator ri;
                        if (ind.contains("name")) ri.name = ind["name"].get<std::string>();
                        if (ind.contains("max")) ri.max = ind["max"].get<double>();
                        if (ind.contains("min")) ri.min = ind["min"].get<double>();
                        sd.radarIndicators.push_back(ri);
                    }
                }
                if (s.contains("radarAreaStyle")) sd.radarAreaStyle = true;
                
                // Candlestick upColor/downColor
                if (s.contains("itemStyle")) {
                    auto& style = s["itemStyle"];
                    if (style.contains("color")) sd.upColor = parseColor(style["color"]);
                    if (style.contains("color0")) sd.downColor = parseColor(style["color0"]);
                }
                
                if (s.contains("data") && s["data"].is_array()) {
                    for (const auto& d : s["data"]) {
                        if (d.is_number()) {
                            sd.data.push_back(d.get<double>());
                        } else if (d.is_object() && d.contains("value")) {
                            if (d["value"].is_array()) {
                                // Radar data: array of values
                                std::vector<double> radarVals;
                                for (const auto& v : d["value"]) {
                                    radarVals.push_back(v.get<double>());
                                }
                                sd.radarData.push_back(radarVals);
                            } else {
                                sd.data.push_back(d["value"].get<double>());
                            }
                            if (d.contains("name") && opt.xAxis.data.size() < sd.data.size()) {
                                opt.xAxis.data.push_back(d["name"].get<std::string>());
                            }
                        } else if (d.is_array() && d.size() >= 4) {
                            // Candlestick data: [open, close, low, high]
                            CandlestickData cd;
                            cd.open = d[0].get<double>();
                            cd.close = d[1].get<double>();
                            cd.low = d[2].get<double>();
                            cd.high = d[3].get<double>();
                            sd.candlestickData.push_back(cd);
                        } else if (d.is_array() && d.size() == 3) {
                            // Heatmap data: [x, y, value]
                            HeatmapData hd;
                            hd.x = d[0].get<int>();
                            hd.y = d[1].get<int>();
                            hd.value = d[2].get<double>();
                            sd.heatmapData.push_back(hd);
                        } else if (d.is_array() && d.size() == 5) {
                            // BoxPlot data: [min, q1, median, q3, max]
                            BoxPlotData bp;
                            bp.min = d[0].get<double>();
                            bp.q1 = d[1].get<double>();
                            bp.median = d[2].get<double>();
                            bp.q3 = d[3].get<double>();
                            bp.max = d[4].get<double>();
                            sd.boxPlotData.push_back(bp);
                        }
                    }
                }
                
                // Heatmap Y labels
                if (s.contains("yAxisData") && s["yAxisData"].is_array()) {
                    for (const auto& y : s["yAxisData"]) {
                        sd.heatmapYLabels.push_back(y.get<std::string>());
                    }
                }
                
                // Treemap data
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::Treemap) {
                    std::function<TreemapData(const json&)> parseTreemap = [&](const json& node) -> TreemapData {
                        TreemapData td;
                        if (node.contains("name")) td.name = node["name"].get<std::string>();
                        if (node.contains("value")) td.value = node["value"].get<double>();
                        if (node.contains("children") && node["children"].is_array()) {
                            for (const auto& child : node["children"]) {
                                td.children.push_back(parseTreemap(child));
                            }
                        }
                        return td;
                    };
                    
                    for (const auto& item : s["data"]) {
                        sd.treemapData.push_back(parseTreemap(item));
                    }
                }
                
                // Waterfall options
                if (s.contains("showTotal")) sd.waterfallShowTotal = s["showTotal"].get<bool>();
                if (s.contains("totalLabel")) sd.waterfallTotalLabel = s["totalLabel"].get<std::string>();
                
                // Sunburst data
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::Sunburst) {
                    std::function<SunburstData(const json&)> parseSunburst = [&](const json& node) -> SunburstData {
                        SunburstData sb;
                        if (node.contains("name")) sb.name = node["name"].get<std::string>();
                        if (node.contains("value")) sb.value = node["value"].get<double>();
                        if (node.contains("children") && node["children"].is_array()) {
                            for (const auto& child : node["children"]) {
                                sb.children.push_back(parseSunburst(child));
                            }
                        }
                        return sb;
                    };
                    for (const auto& item : s["data"]) {
                        sd.sunburstData.push_back(parseSunburst(item));
                    }
                }
                
                // Ring options
                if (s.contains("ringValue")) sd.ringValue = s["ringValue"].get<double>();
                if (s.contains("ringMax")) sd.ringMax = s["ringMax"].get<double>();
                if (s.contains("ringWidth")) sd.ringWidth = s["ringWidth"].get<float>();
                if (sd.type == SeriesType::Ring && !sd.data.empty()) {
                    sd.ringValue = sd.data[0];
                }
                
                // Polar options
                if (s.contains("polarType")) sd.polarType = s["polarType"].get<std::string>();
                
                // Parallel axes and data
                if (s.contains("parallelAxis") && s["parallelAxis"].is_array()) {
                    for (const auto& ax : s["parallelAxis"]) {
                        ParallelAxis pa;
                        if (ax.contains("name")) pa.name = ax["name"].get<std::string>();
                        if (ax.contains("min")) pa.min = ax["min"].get<double>();
                        if (ax.contains("max")) pa.max = ax["max"].get<double>();
                        sd.parallelAxes.push_back(pa);
                    }
                }
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::Parallel) {
                    for (const auto& line : s["data"]) {
                        if (line.is_array()) {
                            std::vector<double> vals;
                            for (const auto& v : line) vals.push_back(v.get<double>());
                            sd.parallelData.push_back(vals);
                        }
                    }
                }
                
                // Sankey nodes and links
                if (s.contains("nodes") && s["nodes"].is_array()) {
                    for (const auto& n : s["nodes"]) {
                        SankeyNode sn;
                        if (n.contains("name")) sn.name = n["name"].get<std::string>();
                        if (n.contains("value")) sn.value = n["value"].get<double>();
                        sd.sankeyNodes.push_back(sn);
                    }
                }
                if (s.contains("links") && s["links"].is_array()) {
                    for (const auto& l : s["links"]) {
                        SankeyLink sl;
                        if (l.contains("source")) sl.source = l["source"].get<std::string>();
                        if (l.contains("target")) sl.target = l["target"].get<std::string>();
                        if (l.contains("value")) sl.value = l["value"].get<double>();
                        sd.sankeyLinks.push_back(sl);
                    }
                }
                
                // Graph nodes and links
                if (s.contains("nodes") && s["nodes"].is_array() && sd.type == SeriesType::Graph) {
                    for (const auto& n : s["nodes"]) {
                        GraphNode gn;
                        if (n.contains("name")) gn.name = n["name"].get<std::string>();
                        if (n.contains("x")) gn.x = n["x"].get<double>();
                        if (n.contains("y")) gn.y = n["y"].get<double>();
                        if (n.contains("value")) gn.value = n["value"].get<double>();
                        if (n.contains("category")) gn.category = n["category"].get<int>();
                        sd.graphNodes.push_back(gn);
                    }
                }
                if (s.contains("links") && s["links"].is_array() && sd.type == SeriesType::Graph) {
                    for (const auto& l : s["links"]) {
                        GraphLink gl;
                        if (l.contains("source")) gl.source = l["source"].get<std::string>();
                        if (l.contains("target")) gl.target = l["target"].get<std::string>();
                        if (l.contains("value")) gl.value = l["value"].get<double>();
                        sd.graphLinks.push_back(gl);
                    }
                }
                if (s.contains("categories") && s["categories"].is_array()) {
                    for (const auto& c : s["categories"]) {
                        sd.graphCategories.push_back(c.get<std::string>());
                    }
                }
                
                // Calendar data
                if (s.contains("calendarYear")) sd.calendarYear = s["calendarYear"].get<int>();
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::Calendar) {
                    for (const auto& d : s["data"]) {
                        CalendarData cd;
                        if (d.is_array() && d.size() >= 2) {
                            cd.date = d[0].get<std::string>();
                            cd.value = d[1].get<double>();
                        }
                        sd.calendarData.push_back(cd);
                    }
                }
                
                // ThemeRiver data
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::ThemeRiver) {
                    for (const auto& d : s["data"]) {
                        ThemeRiverData tr;
                        if (d.is_array() && d.size() >= 3) {
                            tr.date = d[0].get<std::string>();
                            tr.value = d[1].get<double>();
                            tr.name = d[2].get<std::string>();
                        }
                        sd.themeRiverData.push_back(tr);
                    }
                }
                
                // PictorialBar options
                if (s.contains("symbol")) sd.pictorialSymbol = s["symbol"].get<std::string>();
                if (s.contains("symbolSize")) sd.pictorialSymbolSize = s["symbolSize"].get<float>();
                
                // Liquidfill options
                if (s.contains("liquidValue")) sd.liquidValue = s["liquidValue"].get<double>();
                if (s.contains("waves")) sd.liquidWaves = s["waves"].get<int>();
                if (sd.type == SeriesType::Liquidfill && !sd.data.empty()) {
                    sd.liquidValue = sd.data[0];
                }
                
                // Tree data
                if (s.contains("data") && s["data"].is_array() && sd.type == SeriesType::Tree) {
                    std::function<TreeNode(const json&)> parseTree = [&](const json& node) -> TreeNode {
                        TreeNode tn;
                        if (node.contains("name")) tn.name = node["name"].get<std::string>();
                        if (node.contains("value")) tn.value = node["value"].get<double>();
                        if (node.contains("children") && node["children"].is_array()) {
                            for (const auto& child : node["children"]) {
                                tn.children.push_back(parseTree(child));
                            }
                        }
                        return tn;
                    };
                    for (const auto& item : s["data"]) {
                        sd.treeData.push_back(parseTree(item));
                    }
                }
                if (s.contains("layout")) sd.treeLayout = s["layout"].get<std::string>();
                
                // Bullet data
                if (s.contains("bulletData") && s["bulletData"].is_array()) {
                    for (const auto& b : s["bulletData"]) {
                        BulletData bd;
                        if (b.contains("actual")) bd.actual = b["actual"].get<double>();
                        if (b.contains("target")) bd.target = b["target"].get<double>();
                        if (b.contains("name")) bd.name = b["name"].get<std::string>();
                        if (b.contains("ranges") && b["ranges"].is_array()) {
                            for (const auto& r : b["ranges"]) bd.ranges.push_back(r.get<double>());
                        }
                        sd.bulletData.push_back(bd);
                    }
                }
                
                // Nightingale options
                if (s.contains("roseType")) sd.nightingaleRoseType = s["roseType"].get<bool>();
                
                // Histogram options
                if (s.contains("binCount")) sd.histogramBinCount = s["binCount"].get<int>();
                if (s.contains("bins") && s["bins"].is_array()) {
                    for (const auto& bin : s["bins"]) {
                        HistogramBin hb;
                        if (bin.contains("min")) hb.min = bin["min"].get<double>();
                        if (bin.contains("max")) hb.max = bin["max"].get<double>();
                        if (bin.contains("count")) hb.count = bin["count"].get<int>();
                        sd.histogramBins.push_back(hb);
                    }
                }
                
                // Step options
                if (s.contains("stepType")) sd.stepType = s["stepType"].get<std::string>();
                
                // Stack options
                if (s.contains("stack")) sd.stack = s["stack"].get<std::string>();
                
                // Bubble data
                if (s.contains("bubbleData") && s["bubbleData"].is_array()) {
                    for (const auto& b : s["bubbleData"]) {
                        BubblePoint bp;
                        if (b.contains("x")) bp.x = b["x"].get<double>();
                        if (b.contains("y")) bp.y = b["y"].get<double>();
                        if (b.contains("size")) bp.size = b["size"].get<double>();
                        if (b.contains("name")) bp.name = b["name"].get<std::string>();
                        sd.bubbleData.push_back(bp);
                    }
                }
                
                // Dumbbell data
                if (s.contains("dumbbellData") && s["dumbbellData"].is_array()) {
                    for (const auto& d : s["dumbbellData"]) {
                        DumbbellData dd;
                        if (d.contains("start")) dd.start = d["start"].get<double>();
                        if (d.contains("end")) dd.end = d["end"].get<double>();
                        if (d.contains("name")) dd.name = d["name"].get<std::string>();
                        sd.dumbbellData.push_back(dd);
                    }
                }
                
                // RangeBar data
                if (s.contains("rangeBarData") && s["rangeBarData"].is_array()) {
                    for (const auto& r : s["rangeBarData"]) {
                        RangeBarData rd;
                        if (r.contains("start")) rd.start = r["start"].get<double>();
                        if (r.contains("end")) rd.end = r["end"].get<double>();
                        if (r.contains("name")) rd.name = r["name"].get<std::string>();
                        sd.rangeBarData.push_back(rd);
                    }
                }
                
                // Gantt tasks
                if (s.contains("ganttTasks") && s["ganttTasks"].is_array()) {
                    for (const auto& t : s["ganttTasks"]) {
                        GanttTask gt;
                        if (t.contains("name")) gt.name = t["name"].get<std::string>();
                        if (t.contains("start")) gt.start = t["start"].get<double>();
                        if (t.contains("end")) gt.end = t["end"].get<double>();
                        if (t.contains("category")) gt.category = t["category"].get<int>();
                        sd.ganttTasks.push_back(gt);
                    }
                }
                
                // Waffle options
                if (s.contains("waffleValue")) sd.waffleValue = s["waffleValue"].get<double>();
                if (s.contains("waffleMax")) sd.waffleMax = s["waffleMax"].get<double>();
                if (s.contains("waffleCols")) sd.waffleCols = s["waffleCols"].get<int>();
                if (s.contains("waffleRows")) sd.waffleRows = s["waffleRows"].get<int>();
                
                // RadialBar options
                if (s.contains("radialBarWidth")) sd.radialBarWidth = s["radialBarWidth"].get<float>();
                
                // Violin data
                if (s.contains("violinData") && s["violinData"].is_array()) {
                    for (const auto& v : s["violinData"]) {
                        ViolinData vd;
                        if (v.contains("name")) vd.name = v["name"].get<std::string>();
                        if (v.contains("values") && v["values"].is_array()) {
                            for (const auto& val : v["values"]) vd.values.push_back(val.get<double>());
                        }
                        sd.violinData.push_back(vd);
                    }
                }
                
                // ErrorBar data
                if (s.contains("errorBarData") && s["errorBarData"].is_array()) {
                    for (const auto& e : s["errorBarData"]) {
                        ErrorBarData ed;
                        if (e.contains("value")) ed.value = e["value"].get<double>();
                        if (e.contains("errorLow")) ed.errorLow = e["errorLow"].get<double>();
                        if (e.contains("errorHigh")) ed.errorHigh = e["errorHigh"].get<double>();
                        sd.errorBarData.push_back(ed);
                    }
                }
                
                // Slope data
                if (s.contains("slopeData") && s["slopeData"].is_array()) {
                    for (const auto& sl : s["slopeData"]) {
                        SlopeData sd2;
                        if (sl.contains("start")) sd2.start = sl["start"].get<double>();
                        if (sl.contains("end")) sd2.end = sl["end"].get<double>();
                        if (sl.contains("name")) sd2.name = sl["name"].get<std::string>();
                        sd.slopeData.push_back(sd2);
                    }
                }
                if (s.contains("slopeStartLabel")) sd.slopeStartLabel = s["slopeStartLabel"].get<std::string>();
                if (s.contains("slopeEndLabel")) sd.slopeEndLabel = s["slopeEndLabel"].get<std::string>();
                
                // Dot options
                if (s.contains("dotSize")) sd.dotSize = s["dotSize"].get<int>();
                
                // Map data
                if (s.contains("mapData") && s["mapData"].is_array()) {
                    for (const auto& m : s["mapData"]) {
                        MapDataItem md;
                        if (m.contains("name")) md.name = m["name"].get<std::string>();
                        if (m.contains("value")) md.value = m["value"].get<double>();
                        sd.mapData.push_back(md);
                    }
                }
                if (s.contains("mapShowLabel")) sd.mapShowLabel = s["mapShowLabel"].get<bool>();
                
                // Gauge value from data
                if (sd.type == SeriesType::Gauge && !sd.data.empty()) {
                    sd.gaugeValue = sd.data[0];
                }
                
                opt.series.push_back(std::move(sd));
            }
        }
        
        if (j.contains("color") && j["color"].is_array()) {
            opt.color.clear();
            for (const auto& c : j["color"]) {
                opt.color.push_back(parseColor(c));
            }
        }
        
    } catch (const std::exception& e) {
        loge("[FlexChart] JSON parse error: {}", e.what());
    }
    
    return opt;
}

struct ChartWrapper {
    FlexChart* chart;
};

static void js_chart_finalizer(JSRuntime* rt, JSValue val) {
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(val, js_chart_class_id));
    delete w;
}

static JSValue js_chart_setOption(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(this_val, js_chart_class_id));
    if (!w || !w->chart) return JS_UNDEFINED;
    
    const char* jsonStr = JS_ToCString(ctx, argv[0]);
    if (!jsonStr) return JS_UNDEFINED;
    
    auto opt = parseOptionFromString(jsonStr);
    w->chart->setOption(opt);
    
    JS_FreeCString(ctx, jsonStr);
    return JS_UNDEFINED;
}

static JSValue js_chart_resize(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(this_val, js_chart_class_id));
    if (!w || !w->chart) return JS_UNDEFINED;
    
    double width, height;
    JS_ToFloat64(ctx, &width, argv[0]);
    JS_ToFloat64(ctx, &height, argv[1]);
    
    w->chart->resize((float)width, (float)height);
    return JS_UNDEFINED;
}

static JSValue js_chart_setPosition(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(this_val, js_chart_class_id));
    if (!w || !w->chart) return JS_UNDEFINED;
    
    double x, y;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    
    w->chart->setPosition((float)x, (float)y);
    return JS_UNDEFINED;
}

static JSValue js_chart_on(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;
    
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(this_val, js_chart_class_id));
    if (!w || !w->chart) return JS_UNDEFINED;
    
    const char* eventName = JS_ToCString(ctx, argv[0]);
    if (!eventName) return JS_UNDEFINED;
    
    JSValue callback = JS_DupValue(ctx, argv[1]);
    std::string event(eventName);
    JS_FreeCString(ctx, eventName);
    
    w->chart->on(event, [ctx, callback](const ChartEvent& e) {
        JSValue global = JS_GetGlobalObject(ctx);
        
        JSValue eventObj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, eventObj, "type", JS_NewString(ctx, e.type.c_str()));
        JS_SetPropertyStr(ctx, eventObj, "seriesIndex", JS_NewInt32(ctx, e.seriesIndex));
        JS_SetPropertyStr(ctx, eventObj, "dataIndex", JS_NewInt32(ctx, e.dataIndex));
        JS_SetPropertyStr(ctx, eventObj, "name", JS_NewString(ctx, e.name.c_str()));
        JS_SetPropertyStr(ctx, eventObj, "value", JS_NewFloat64(ctx, e.value));
        JS_SetPropertyStr(ctx, eventObj, "x", JS_NewFloat64(ctx, e.x));
        JS_SetPropertyStr(ctx, eventObj, "y", JS_NewFloat64(ctx, e.y));
        
        JSValue args[1] = {eventObj};
        JSValue result = JS_Call(ctx, callback, global, 1, args);
        
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, eventObj);
        JS_FreeValue(ctx, global);
    });
    
    return JS_UNDEFINED;
}

static JSValue js_chart_init(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    
    const char* containerId = JS_ToCString(ctx, argv[0]);
    if (!containerId) return JS_NULL;
    
    FlexChart* chart = ChartManager::instance().createChart(containerId);
    JS_FreeCString(ctx, containerId);
    
    if (!chart) return JS_NULL;
    
    JSValue obj = JS_NewObjectClass(ctx, js_chart_class_id);
    ChartWrapper* w = new ChartWrapper{chart};
    JS_SetOpaque(obj, w);
    
    return obj;
}

static JSValue js_chart_getChart(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    
    const char* containerId = JS_ToCString(ctx, argv[0]);
    if (!containerId) return JS_NULL;
    
    FlexChart* chart = ChartManager::instance().getChart(containerId);
    JS_FreeCString(ctx, containerId);
    
    if (!chart) return JS_NULL;
    
    JSValue obj = JS_NewObjectClass(ctx, js_chart_class_id);
    ChartWrapper* w = new ChartWrapper{chart};
    JS_SetOpaque(obj, w);
    
    return obj;
}

static JSValue js_chart_dispose(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    
    const char* containerId = JS_ToCString(ctx, argv[0]);
    if (!containerId) return JS_UNDEFINED;
    
    ChartManager::instance().destroyChart(containerId);
    JS_FreeCString(ctx, containerId);
    
    return JS_UNDEFINED;
}

void setupChartRuntime(JSContext* ctx, cssboxRenderer* renderer) {
    ChartManager::instance().init(ctx, renderer);
    
    JSRuntime* rt = JS_GetRuntime(ctx);
    JS_NewClassID(rt, &js_chart_class_id);
    
    JSClassDef chart_class_def;
    memset(&chart_class_def, 0, sizeof(chart_class_def));
    chart_class_def.class_name = "FlexChart";
    chart_class_def.finalizer = js_chart_finalizer;
    
    JS_NewClass(rt, js_chart_class_id, &chart_class_def);
    
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "setOption",
                      JS_NewCFunction(ctx, js_chart_setOption, "setOption", 1));
    JS_SetPropertyStr(ctx, proto, "resize",
                      JS_NewCFunction(ctx, js_chart_resize, "resize", 2));
    JS_SetPropertyStr(ctx, proto, "setPosition",
                      JS_NewCFunction(ctx, js_chart_setPosition, "setPosition", 2));
    JS_SetPropertyStr(ctx, proto, "on",
                      JS_NewCFunction(ctx, js_chart_on, "on", 2));
    
    JS_SetClassProto(ctx, js_chart_class_id, proto);
    
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue flexchart = JS_NewObject(ctx);
    
    JS_SetPropertyStr(ctx, flexchart, "init",
                      JS_NewCFunction(ctx, js_chart_init, "init", 1));
    JS_SetPropertyStr(ctx, flexchart, "getChart",
                      JS_NewCFunction(ctx, js_chart_getChart, "getChart", 1));
    JS_SetPropertyStr(ctx, flexchart, "dispose",
                      JS_NewCFunction(ctx, js_chart_dispose, "dispose", 1));
    
    JS_SetPropertyStr(ctx, global, "FlexChart", flexchart);
    JS_FreeValue(ctx, global);
}

} // namespace flexchart
