#pragma once

#include <flexchart/types.h>
#include <cssbox.h>
#include <nanovg.h>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

namespace flexchart {

struct ChartEvent {
    std::string type;
    int seriesIndex = -1;
    int dataIndex = -1;
    std::string name;
    double value = 0;
    float x = 0, y = 0;
};

using EventCallback = std::function<void(const ChartEvent&)>;

class FlexChart {
public:
    FlexChart(cssboxRenderer* renderer, const std::string& id);
    ~FlexChart();

    void setOption(const ChartOption& option);
    const ChartOption& getOption() const { return option_; }
    
    void resize(float width, float height);
    void draw(NVGcontext* vg);
    
    void on(const std::string& event, EventCallback callback);
    void off(const std::string& event);
    
    bool handleMouseMove(float x, float y);
    bool handleMouseDown(float x, float y);
    bool handleMouseUp(float x, float y);
    
    const std::string& id() const { return id_; }
    cssboxRenderer* renderer() const { return renderer_; }
    
    float width() const { return width_; }
    float height() const { return height_; }
    
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }

private:
    cssboxRenderer* renderer_;
    std::string id_;
    ChartOption option_;
    float width_ = 400.0f;
    float height_ = 300.0f;
    float x_ = 0, y_ = 0;
    
    int hoveredSeries_ = -1;
    int hoveredIndex_ = -1;
    float mouseX_ = 0, mouseY_ = 0;
    
    std::unordered_map<std::string, EventCallback> callbacks_;
    
    void emitEvent(const std::string& type, int seriesIdx, int dataIdx);
    void drawTitle(NVGcontext* vg);
    void drawLegend(NVGcontext* vg);
    void drawTooltip(NVGcontext* vg);
    void drawGrid(NVGcontext* vg);
    void drawAxis(NVGcontext* vg);
    
    void getChartArea(float& cx, float& cy, float& cw, float& ch) const;
    void getValueRange(double& minVal, double& maxVal) const;
    Color getSeriesColor(size_t index) const;
};

void renderLineSeries(NVGcontext* vg, const SeriesData& series, 
                      float x, float y, float w, float h,
                      const std::vector<std::string>& labels,
                      double minVal, double maxVal, Color color,
                      int hoveredIdx);

void renderBarSeries(NVGcontext* vg, const SeriesData& series,
                     float x, float y, float w, float h,
                     const std::vector<std::string>& labels,
                     double minVal, double maxVal, Color color,
                     size_t seriesIdx, size_t totalSeries,
                     int hoveredIdx);

void renderPieSeries(NVGcontext* vg, const SeriesData& series,
                     float cx, float cy, float radius,
                     Color color, int hoveredIdx,
                     const std::vector<Color>& colors);

void renderScatterSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         double minVal, double maxVal, Color color,
                         int hoveredIdx);

void renderRadarSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       Color color, int hoveredIdx,
                       const std::vector<Color>& colors);

void renderGaugeSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       Color color);

void renderFunnelSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        const std::vector<std::string>& labels,
                        int hoveredIdx,
                        const std::vector<Color>& colors);

void renderCandlestickSeries(NVGcontext* vg, const SeriesData& series,
                             float x, float y, float w, float h,
                             const std::vector<std::string>& labels,
                             double minVal, double maxVal,
                             int hoveredIdx);

void renderHeatmapSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& xLabels,
                         int hoveredIdx);

void renderTreemapSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         int hoveredIdx,
                         const std::vector<Color>& colors);

void renderBoxPlotSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         double minVal, double maxVal, Color color,
                         int hoveredIdx);

void renderWaterfallSeries(NVGcontext* vg, const SeriesData& series,
                           float x, float y, float w, float h,
                           const std::vector<std::string>& labels,
                           double minVal, double maxVal,
                           int hoveredIdx,
                           const std::vector<Color>& colors);

void renderSunburstSeries(NVGcontext* vg, const SeriesData& series,
                          float cx, float cy, float radius,
                          int hoveredIdx,
                          const std::vector<Color>& colors);

void renderPolarSeries(NVGcontext* vg, const SeriesData& series,
                       float cx, float cy, float radius,
                       const std::vector<std::string>& labels,
                       Color color, int hoveredIdx);

void renderRingSeries(NVGcontext* vg, const SeriesData& series,
                      float cx, float cy, float radius,
                      Color color);

void renderParallelSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx,
                          const std::vector<Color>& colors);

void renderSankeySeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx,
                        const std::vector<Color>& colors);

void renderGraphSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx,
                       const std::vector<Color>& colors);

void renderCalendarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx);

void renderThemeRiverSeries(NVGcontext* vg, const SeriesData& series,
                            float x, float y, float w, float h,
                            int hoveredIdx,
                            const std::vector<Color>& colors);

void renderPictorialBarSeries(NVGcontext* vg, const SeriesData& series,
                              float x, float y, float w, float h,
                              const std::vector<std::string>& labels,
                              double minVal, double maxVal, Color color,
                              int hoveredIdx);

void renderLiquidfillSeries(NVGcontext* vg, const SeriesData& series,
                            float cx, float cy, float radius,
                            Color color);

void renderTreeSeries(NVGcontext* vg, const SeriesData& series,
                      float x, float y, float w, float h,
                      int hoveredIdx,
                      const std::vector<Color>& colors);

void renderBulletSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx,
                        const std::vector<Color>& colors);

void renderNightingaleSeries(NVGcontext* vg, const SeriesData& series,
                             float cx, float cy, float radius,
                             const std::vector<std::string>& labels,
                             int hoveredIdx,
                             const std::vector<Color>& colors);

void renderHistogramSeries(NVGcontext* vg, const SeriesData& series,
                           float x, float y, float w, float h,
                           int hoveredIdx, Color color);

void renderStepSeries(NVGcontext* vg, const SeriesData& series,
                      float x, float y, float w, float h,
                      const std::vector<std::string>& labels,
                      double minVal, double maxVal, Color color,
                      int hoveredIdx);

void renderBarStackSeries(NVGcontext* vg, const std::vector<SeriesData>& allSeries,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          int hoveredSeriesIdx, int hoveredDataIdx,
                          const std::vector<Color>& colors);

void renderAreaStackSeries(NVGcontext* vg, const std::vector<SeriesData>& allSeries,
                           float x, float y, float w, float h,
                           const std::vector<std::string>& labels,
                           int hoveredSeriesIdx, int hoveredDataIdx,
                           const std::vector<Color>& colors);

void renderBubbleSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx, const std::vector<Color>& colors);

void renderLollipopSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          double minVal, double maxVal, Color color,
                          int hoveredIdx);

void renderDumbbellSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx, const std::vector<Color>& colors);

void renderRangeBarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx, const std::vector<Color>& colors);

void renderGanttSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx, const std::vector<Color>& colors);

void renderWaffleSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        Color color);

void renderRadialBarSeries(NVGcontext* vg, const SeriesData& series,
                           float cx, float cy, float radius,
                           const std::vector<std::string>& labels,
                           int hoveredIdx, const std::vector<Color>& colors);

void renderPyramidSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         const std::vector<std::string>& labels,
                         int hoveredIdx, const std::vector<Color>& colors);

void renderViolinSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        int hoveredIdx, const std::vector<Color>& colors);

void renderErrorBarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          double minVal, double maxVal, Color color,
                          int hoveredIdx);

void renderSlopeSeries(NVGcontext* vg, const SeriesData& series,
                       float x, float y, float w, float h,
                       int hoveredIdx, const std::vector<Color>& colors);

void renderDotSeries(NVGcontext* vg, const SeriesData& series,
                     float x, float y, float w, float h,
                     const std::vector<std::string>& labels,
                     int hoveredIdx, Color color);

void renderMapChinaSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx);

int findHoveredProvinceIndex(float mouseX, float mouseY, float chartX, float chartY,
                              float chartW, float chartH);

} // namespace flexchart
