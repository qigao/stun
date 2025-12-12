#pragma once

#include <quickjs.h>
#include <cssbox.h>
#include <flexchart/flexchart.h>
#include <unordered_map>
#include <memory>

namespace flexchart {

class ChartManager {
public:
    static ChartManager& instance();
    
    void init(JSContext* ctx, cssboxRenderer* renderer);
    void shutdown();
    
    FlexChart* createChart(const std::string& containerId);
    FlexChart* getChart(const std::string& containerId);
    void destroyChart(const std::string& containerId);
    
    void renderAll(NVGcontext* vg);
    bool handleMouseMove(float x, float y);
    bool handleMouseDown(float x, float y);
    
    JSContext* context() const { return ctx_; }
    cssboxRenderer* renderer() const { return renderer_; }

private:
    ChartManager() = default;
    
    JSContext* ctx_ = nullptr;
    cssboxRenderer* renderer_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<FlexChart>> charts_;
};

void setupChartRuntime(JSContext* ctx, cssboxRenderer* renderer);

ChartOption parseOptionFromJSON(JSContext* ctx, JSValue obj);
ChartOption parseOptionFromString(const std::string& json);

} // namespace flexchart
