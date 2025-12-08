#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <nanovg.h>

namespace flexchart {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    
    NVGcolor toNVG() const { return nvgRGBA(r, g, b, a); }
    
    static Color fromHex(const std::string& hex);
    static Color fromRGB(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }
    static Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return {r, g, b, a}; }
};

enum class SeriesType {
    Line,
    Bar,
    Pie,
    Scatter,
    Area,
    Radar,
    Gauge,
    Funnel,
    Candlestick,
    Heatmap,
    Treemap,
    BoxPlot,
    Waterfall,
    Sunburst,
    Polar,
    Ring,
    Parallel,
    Sankey,
    Graph,
    Calendar,
    ThemeRiver,
    PictorialBar,
    Liquidfill,
    Tree,
    Bullet,
    Nightingale,
    Histogram,
    Step,
    BarStack,
    AreaStack,
    Bubble,
    Lollipop,
    Dumbbell,
    RangeBar,
    Gantt,
    Waffle,
    RadialBar,
    Pyramid,
    Violin,
    ErrorBar,
    Slope,
    Dot,
    MapChina
};

struct DataPoint {
    std::variant<double, std::string> x;
    double y = 0;
    std::optional<double> value;
    std::optional<std::string> name;
};

struct CandlestickData {
    double open = 0;
    double close = 0;
    double low = 0;
    double high = 0;
};

struct HeatmapData {
    int x = 0;
    int y = 0;
    double value = 0;
};

struct TreemapData {
    std::string name;
    double value = 0;
    std::vector<TreemapData> children;
};

struct BoxPlotData {
    double min = 0;
    double q1 = 0;
    double median = 0;
    double q3 = 0;
    double max = 0;
    std::vector<double> outliers;
};

struct SunburstData {
    std::string name;
    double value = 0;
    std::vector<SunburstData> children;
};

struct SankeyNode {
    std::string name;
    double value = 0;
};

struct SankeyLink {
    std::string source;
    std::string target;
    double value = 0;
};

struct ParallelAxis {
    std::string name;
    double min = 0;
    double max = 100;
};

struct GraphNode {
    std::string name;
    double x = 0;
    double y = 0;
    double value = 10;
    int category = 0;
};

struct GraphLink {
    std::string source;
    std::string target;
    double value = 1;
};

struct CalendarData {
    std::string date;  // "YYYY-MM-DD"
    double value = 0;
};

struct ThemeRiverData {
    std::string date;
    double value = 0;
    std::string name;
};

struct TreeNode {
    std::string name;
    double value = 0;
    std::vector<TreeNode> children;
};

struct BulletData {
    double actual = 0;
    double target = 0;
    std::vector<double> ranges;  // background ranges
    std::string name;
};

struct HistogramBin {
    double min = 0;
    double max = 0;
    int count = 0;
};

struct BubblePoint {
    double x = 0;
    double y = 0;
    double size = 10;
    std::string name;
};

struct DumbbellData {
    double start = 0;
    double end = 0;
    std::string name;
};

struct RangeBarData {
    double start = 0;
    double end = 0;
    std::string name;
};

struct GanttTask {
    std::string name;
    double start = 0;  // can be day index or timestamp
    double end = 0;
    int category = 0;
};

struct ViolinData {
    std::vector<double> values;
    std::string name;
};

struct ErrorBarData {
    double value = 0;
    double errorLow = 0;
    double errorHigh = 0;
};

struct SlopeData {
    double start = 0;
    double end = 0;
    std::string name;
};

struct MapDataItem {
    std::string name;  // Province name
    double value = 0;
};

struct RadarIndicator {
    std::string name;
    double max = 100;
    double min = 0;
};

struct SeriesData {
    SeriesType type = SeriesType::Line;
    std::string name;
    std::vector<double> data;
    std::optional<Color> color;
    
    bool smooth = false;
    bool showSymbol = true;
    float symbolSize = 4.0f;
    float lineWidth = 2.0f;
    bool areaStyle = false;
    float barWidth = 0.6f;
    float innerRadius = 0.0f;
    
    // Gauge specific
    double gaugeMin = 0;
    double gaugeMax = 100;
    double gaugeValue = 0;
    float startAngle = 225.0f;
    float endAngle = -45.0f;
    bool showGaugeDetail = true;
    
    // Funnel specific
    std::string funnelSort = "descending";
    float funnelGap = 2.0f;
    
    // Radar specific
    std::vector<RadarIndicator> radarIndicators;
    std::vector<std::vector<double>> radarData;
    bool radarAreaStyle = false;
    
    // Candlestick specific
    std::vector<CandlestickData> candlestickData;
    Color upColor = {236, 72, 72, 255};
    Color downColor = {16, 185, 129, 255};
    
    // Heatmap specific
    std::vector<HeatmapData> heatmapData;
    std::vector<std::string> heatmapYLabels;
    Color heatmapMinColor = {255, 255, 255, 255};
    Color heatmapMaxColor = {91, 143, 249, 255};
    
    // Treemap specific
    std::vector<TreemapData> treemapData;
    
    // BoxPlot specific
    std::vector<BoxPlotData> boxPlotData;
    
    // Waterfall specific
    bool waterfallShowTotal = true;
    std::string waterfallTotalLabel = "Total";
    
    // Sunburst specific
    std::vector<SunburstData> sunburstData;
    
    // Ring specific (simple progress ring)
    double ringValue = 0;
    double ringMax = 100;
    float ringWidth = 20.0f;
    Color ringBackgroundColor = {230, 230, 230, 255};
    
    // Polar specific
    std::string polarType = "line";  // "line", "bar", "scatter"
    
    // Parallel specific
    std::vector<ParallelAxis> parallelAxes;
    std::vector<std::vector<double>> parallelData;
    
    // Sankey specific
    std::vector<SankeyNode> sankeyNodes;
    std::vector<SankeyLink> sankeyLinks;
    
    // Graph specific
    std::vector<GraphNode> graphNodes;
    std::vector<GraphLink> graphLinks;
    std::vector<std::string> graphCategories;
    bool graphForceLayout = true;
    
    // Calendar specific
    std::vector<CalendarData> calendarData;
    int calendarYear = 2024;
    
    // ThemeRiver specific
    std::vector<ThemeRiverData> themeRiverData;
    
    // PictorialBar specific
    std::string pictorialSymbol = "rect";  // "rect", "circle", "triangle"
    float pictorialSymbolSize = 20.0f;
    
    // Liquidfill specific
    double liquidValue = 0.5;
    int liquidWaves = 3;
    
    // Tree specific
    std::vector<TreeNode> treeData;
    std::string treeLayout = "orthogonal";  // "orthogonal", "radial"
    
    // Bullet specific
    std::vector<BulletData> bulletData;
    
    // Nightingale specific (uses data[] and xAxis.data for labels)
    bool nightingaleRoseType = true;  // area or radius mode
    
    // Histogram specific
    std::vector<HistogramBin> histogramBins;
    int histogramBinCount = 10;
    
    // Step specific (uses data[] like line chart)
    std::string stepType = "start";  // "start", "middle", "end"
    
    // BarStack/AreaStack specific (uses multiple series)
    std::string stack = "";  // stack group name
    
    // Bubble specific
    std::vector<BubblePoint> bubbleData;
    
    // Dumbbell specific
    std::vector<DumbbellData> dumbbellData;
    
    // RangeBar specific
    std::vector<RangeBarData> rangeBarData;
    
    // Gantt specific
    std::vector<GanttTask> ganttTasks;
    std::vector<std::string> ganttCategories;
    
    // Waffle specific
    double waffleValue = 0;
    double waffleMax = 100;
    int waffleCols = 10;
    int waffleRows = 10;
    
    // RadialBar specific (uses data[] for values)
    float radialBarWidth = 15.0f;
    
    // Violin specific
    std::vector<ViolinData> violinData;
    
    // ErrorBar specific
    std::vector<ErrorBarData> errorBarData;
    
    // Slope specific
    std::vector<SlopeData> slopeData;
    std::string slopeStartLabel = "Start";
    std::string slopeEndLabel = "End";
    
    // Dot specific (uses data[] for counts per category)
    int dotSize = 8;
    
    // Map specific
    std::vector<MapDataItem> mapData;
    bool mapShowLabel = true;
    Color mapLowColor = {217, 217, 217, 255};
    Color mapHighColor = {91, 143, 249, 255};
};

struct AxisConfig {
    std::string type = "category";
    std::vector<std::string> data;
    bool show = true;
    std::optional<double> min;
    std::optional<double> max;
    bool splitLine = true;
};

struct TitleConfig {
    std::string text;
    std::string subtext;
    std::string left = "center";
    float fontSize = 18.0f;
    float subtextFontSize = 12.0f;
};

struct LegendConfig {
    bool show = true;
    std::string orient = "horizontal";
    std::string left = "center";
    std::string top = "bottom";
    std::vector<std::string> data;
};

struct TooltipConfig {
    bool show = true;
    std::string trigger = "item";
};

struct GridConfig {
    float left = 60.0f;
    float right = 20.0f;
    float top = 60.0f;
    float bottom = 40.0f;
    bool containLabel = true;
};

struct ChartOption {
    TitleConfig title;
    LegendConfig legend;
    TooltipConfig tooltip;
    GridConfig grid;
    AxisConfig xAxis;
    AxisConfig yAxis;
    std::vector<SeriesData> series;
    std::vector<Color> color;
};

inline std::vector<Color> defaultColorPalette() {
    return {
        {91, 143, 249, 255},
        {16, 185, 129, 255},
        {249, 115, 22, 255},
        {139, 92, 246, 255},
        {236, 72, 153, 255},
        {234, 179, 8, 255},
        {99, 102, 241, 255},
        {20, 184, 166, 255},
    };
}

} // namespace flexchart
