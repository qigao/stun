#include <flexchart/flexchart.h>
#include <flexchart/runtime.h>
#include <cssbox.h>
#include <nanovg.h>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <memory>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

// Global state for GLFW callbacks
static float g_scrollY = 0;
static float g_contentHeight = 0;
static int g_winW = 1400, g_winH = 800;
static bool g_needsRelayout = true;
static std::vector<std::unique_ptr<flexchart::FlexChart>>* g_charts = nullptr;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    g_scrollY -= (float)yoffset * 50;
    g_scrollY = std::max(0.0f, std::min(g_scrollY, std::max(0.0f, g_contentHeight - g_winH)));
    g_needsRelayout = true;
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_charts) {
        for (auto& c : *g_charts) c->handleMouseMove((float)xpos, (float)ypos);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && g_charts) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        for (auto& c : *g_charts) c->handleMouseDown((float)xpos, (float)ypos);
    }
}

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::cerr << "GLFW init failed" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    GLFWwindow* window = glfwCreateWindow(1400, 800, "FlexChart - 43 Chart Types (Scroll to see all)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    gladLoadGL();

    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    nvgCreateFont(vg, "sans-serif", "C:/Windows/Fonts/segoeui.ttf");
    nvgCreateFont(vg, "sans-serif-cn", "C:/Windows/Fonts/msyh.ttc");
    nvgAddFallbackFontId(vg, nvgFindFont(vg, "sans-serif"), nvgFindFont(vg, "sans-serif-cn"));
    cssboxRenderer* renderer = cssboxCreateRenderer(vg);

    std::vector<std::unique_ptr<flexchart::FlexChart>> charts;
    
    auto make = [&](const std::string& id, float x, float y, float w, float h) {
        auto c = std::make_unique<flexchart::FlexChart>(renderer, id);
        c->setPosition(x, y);
        c->resize(w, h);
        auto* p = c.get();
        charts.push_back(std::move(c));
        return p;
    };

    float cw = 480, ch = 360, gap = 10;
    int col = 0, row = 0;
    
    
    int cols = 4;  // Will be recalculated based on window width
    
    auto layoutCharts = [&](int windowW, int windowH) {
        cols = std::max(1, (int)((windowW - gap) / (cw + gap)));
        col = 0; row = 0;
        for (auto& c : charts) {
            float x = gap + col * (cw + gap);
            float y = gap + row * (ch + gap) - g_scrollY;
            c->setPosition(x, y);
            c->resize(cw, ch);
            col++;
            if (col >= cols) { col = 0; row++; }
        }
        g_contentHeight = (row + (col > 0 ? 1 : 0)) * (ch + gap) + gap;
    };
    
    auto pos = [&]() -> std::pair<float, float> {
        float x = gap + col * (cw + gap);
        float y = gap + row * (ch + gap);
        col++;
        if (col >= cols) { col = 0; row++; }
        return {x, y};
    };

    // Row 1: 1-7
    { auto [x,y] = pos(); auto* c = make("line", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "1. Line";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Line;
      s.data = {150,230,224,218,135}; s.smooth = true;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("bar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "2. Bar";
      o.xAxis.data = {"Q1","Q2","Q3","Q4"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Bar;
      s.data = {320,200,150,280};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("pie", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "3. Pie";
      o.xAxis.data = {"A","B","C","D"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Pie;
      s.data = {40,30,20,10}; s.innerRadius = 0.4f;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("scatter", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "4. Scatter";
      o.xAxis.data = {"A","B","C","D","E"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Scatter;
      s.data = {80,120,50,180,90}; s.symbolSize = 8;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("area", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "5. Area";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Area;
      s.data = {820,932,901,934,1290}; s.smooth = true; s.areaStyle = true;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("radar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "6. Radar";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Radar;
      s.radarIndicators = {{"A",100,0},{"B",100,0},{"C",100,0},{"D",100,0},{"E",100,0}};
      s.radarData = {{80,90,70,85,95}}; s.radarAreaStyle = true;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("gauge", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "7. Gauge";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Gauge;
      s.gaugeValue = 72; s.gaugeMax = 100; s.name = "Score";
      o.series = {s}; c->setOption(o); }

    // Row 2: 8-14
    { auto [x,y] = pos(); auto* c = make("funnel", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "8. Funnel";
      o.xAxis.data = {"Visit","Click","Order","Pay"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Funnel;
      s.data = {100,80,60,40};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("candle", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "9. Candlestick";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Candlestick;
      s.candlestickData = {{20,34,10,38},{40,35,30,50},{31,38,28,42},{38,30,25,44},{30,42,26,48}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("heatmap", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "10. Heatmap";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Heatmap;
      s.heatmapYLabels = {"AM","PM","Night"};
      for(int i=0;i<5;i++) for(int j=0;j<3;j++) s.heatmapData.push_back({i,j,(double)(rand()%100)});
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("treemap", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "11. Treemap";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Treemap;
      s.treemapData = {{"Tech",500,{}},{"Finance",300,{}},{"Health",200,{}}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("boxplot", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "12. BoxPlot";
      o.xAxis.data = {"A","B","C"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::BoxPlot;
      s.boxPlotData = {{10,25,35,45,60,{}},{15,30,40,50,65,{}},{20,35,45,55,70,{}}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("waterfall", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "13. Waterfall";
      o.xAxis.data = {"Rev","Cost","Tax","Net"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Waterfall;
      s.data = {100,-30,-10,-20}; s.waterfallShowTotal = true;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("sunburst", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "14. Sunburst";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Sunburst;
      s.sunburstData = {{"A",0,{{"A1",30,{}},{"A2",20,{}}}},{"B",0,{{"B1",25,{}}}}};
      o.series = {s}; c->setOption(o); }

    // Row 3: 15-21
    { auto [x,y] = pos(); auto* c = make("polar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "15. Polar";
      o.xAxis.data = {"N","NE","E","SE","S","SW","W","NW"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Polar;
      s.data = {80,90,60,70,85,75,65,80}; s.polarType = "line";
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("ring", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "16. Ring";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Ring;
      s.ringValue = 75; s.ringMax = 100; s.name = "Progress";
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("parallel", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "17. Parallel";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Parallel;
      s.parallelAxes = {{"P",0,100},{"Q",0,100},{"S",0,100},{"T",0,100}};
      s.parallelData = {{80,60,90,70},{50,80,60,90},{70,70,80,80}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("sankey", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "18. Sankey";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Sankey;
      s.sankeyNodes = {{"A",0},{"B",0},{"X",0},{"Y",0}};
      s.sankeyLinks = {{"A","X",30},{"A","Y",20},{"B","X",25},{"B","Y",15}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("graph", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "19. Graph";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Graph;
      s.graphNodes = {{"N1",0,0,15,0},{"N2",0,0,12,1},{"N3",0,0,10,0},{"N4",0,0,8,1}};
      s.graphLinks = {{"N1","N2",2},{"N1","N3",1},{"N2","N4",1},{"N3","N4",2}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("calendar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "20. Calendar";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Calendar;
      s.calendarYear = 2024;
      for(int m=1;m<=12;m++) for(int d=1;d<=28;d++) {
        char dt[16]; snprintf(dt,16,"2024-%02d-%02d",m,d);
        s.calendarData.push_back({dt,(double)(rand()%100)});
      }
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("themeriver", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "21. ThemeRiver";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::ThemeRiver;
      const char* dates[] = {"Jan","Feb","Mar","Apr","May","Jun"};
      const char* cats[] = {"A","B","C"};
      for(int d=0;d<6;d++) for(int c=0;c<3;c++)
        s.themeRiverData.push_back({dates[d],(double)(30+rand()%70),cats[c]});
      o.series = {s}; c->setOption(o); }

    // Row 4: 22-28
    { auto [x,y] = pos(); auto* c = make("pictorial", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "22. PictorialBar";
      o.xAxis.data = {"Q1","Q2","Q3","Q4"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::PictorialBar;
      s.data = {80,60,90,70}; s.pictorialSymbol = "star"; s.pictorialSymbolSize = 12;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("liquid", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "23. Liquidfill";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Liquidfill;
      s.liquidValue = 0.65; s.liquidWaves = 3; s.name = "Done";
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("tree", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "24. Tree";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Tree;
      s.treeData = {{"Root",0,{{"C1",0,{{"L1",0,{}},{"L2",0,{}}}},{"C2",0,{{"L3",0,{}}}}}}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("bullet", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "25. Bullet";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Bullet;
      s.bulletData = {{75,90,{50,75,100},"Rev"},{60,80,{40,60,100},"Profit"}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("nightingale", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "26. Nightingale";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Nightingale;
      s.data = {40,32,28,45,38,25,35};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("histogram", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "27. Histogram";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Histogram;
      for(int i=0;i<100;i++) s.data.push_back(50+(rand()%100)-50+(rand()%30));
      s.histogramBinCount = 12;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("step", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "28. Step";
      o.xAxis.data = {"Mon","Tue","Wed","Thu","Fri"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Step;
      s.data = {120,200,150,180,90}; s.stepType = "middle"; s.areaStyle = true;
      o.series = {s}; c->setOption(o); }

    // Row 5: 29-35
    { auto [x,y] = pos(); auto* c = make("barstack", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "29. BarStack";
      o.xAxis.data = {"Q1","Q2","Q3","Q4"};
      flexchart::SeriesData s1,s2,s3;
      s1.type = s2.type = s3.type = flexchart::SeriesType::BarStack;
      s1.data = {30,40,35,45}; s2.data = {25,30,40,35}; s3.data = {20,25,30,40};
      o.series = {s1,s2,s3}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("areastack", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "30. AreaStack";
      o.xAxis.data = {"Jan","Feb","Mar","Apr","May"};
      flexchart::SeriesData s1,s2,s3;
      s1.type = s2.type = s3.type = flexchart::SeriesType::AreaStack;
      s1.data = {120,132,101,134,90}; s2.data = {220,182,191,234,290}; s3.data = {150,232,201,154,190};
      o.series = {s1,s2,s3}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("bubble", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "31. Bubble";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Bubble;
      s.bubbleData = {{10,20,30,"A"},{30,40,50,"B"},{50,30,20,"C"},{70,60,40,"D"},{40,80,25,"E"}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("lollipop", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "32. Lollipop";
      o.xAxis.data = {"A","B","C","D","E"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Lollipop;
      s.data = {80,120,60,150,90};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("dumbbell", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "33. Dumbbell";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Dumbbell;
      s.dumbbellData = {{30,80,"2022 vs 2023"},{40,70,"Q1 vs Q4"},{50,90,"Before/After"}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("rangebar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "34. RangeBar";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::RangeBar;
      s.rangeBarData = {{20,80,"Task A"},{30,60,"Task B"},{10,90,"Task C"}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("gantt", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "35. Gantt";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Gantt;
      s.ganttTasks = {{"Design",0,3,0},{"Develop",2,7,1},{"Test",6,9,2},{"Deploy",8,10,0}};
      o.series = {s}; c->setOption(o); }

    // Row 6: 36-42
    { auto [x,y] = pos(); auto* c = make("waffle", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "36. Waffle";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Waffle;
      s.waffleValue = 73; s.waffleMax = 100; s.waffleCols = 10; s.waffleRows = 10;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("radialbar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "37. RadialBar";
      o.xAxis.data = {"A","B","C","D"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::RadialBar;
      s.data = {80,60,90,45}; s.radialBarWidth = 12;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("pyramid", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "38. Pyramid";
      o.xAxis.data = {"Top","Mid","Base"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Pyramid;
      s.data = {30,60,100};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("violin", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "39. Violin";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Violin;
      std::vector<double> v1,v2,v3;
      for(int i=0;i<50;i++) { v1.push_back(50+rand()%30); v2.push_back(60+rand()%40); v3.push_back(40+rand()%50); }
      s.violinData = {{v1,"A"},{v2,"B"},{v3,"C"}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("errorbar", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "40. ErrorBar";
      o.xAxis.data = {"A","B","C","D"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::ErrorBar;
      s.errorBarData = {{50,10,15},{70,8,12},{60,15,10},{80,5,20}};
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("slope", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "41. Slope";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Slope;
      s.slopeData = {{80,60,"Product A"},{50,70,"Product B"},{30,90,"Product C"}};
      s.slopeStartLabel = "2022"; s.slopeEndLabel = "2023";
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); auto* c = make("dot", x, y, cw, ch);
      flexchart::ChartOption o; o.title.text = "42. Dot";
      o.xAxis.data = {"A","B","C","D","E"};
      flexchart::SeriesData s; s.type = flexchart::SeriesType::Dot;
      s.data = {5,8,3,10,6}; s.dotSize = 10;
      o.series = {s}; c->setOption(o); }

    { auto [x,y] = pos(); 
      float mapW = cw * 2.5f;
      float mapH = ch * 2.0f;
      auto* c = make("mapchina", x, y, mapW, mapH);
      flexchart::ChartOption o; o.title.text = "43. China Map";
      flexchart::SeriesData s; s.type = flexchart::SeriesType::MapChina;
      s.mapData = {
        {"北京", 95}, {"上海", 90}, {"广东", 85}, {"江苏", 80}, {"浙江", 78},
        {"山东", 72}, {"河南", 68}, {"四川", 65}, {"湖北", 62}, {"湖南", 58},
        {"河北", 55}, {"福建", 52}, {"安徽", 48}, {"辽宁", 45}, {"陕西", 42},
        {"江西", 38}, {"重庆", 35}, {"广西", 32}, {"云南", 30}, {"山西", 28},
        {"内蒙古", 25}, {"贵州", 22}, {"新疆", 20}, {"天津", 18}, {"黑龙江", 15},
        {"吉林", 12}, {"甘肃", 10}, {"海南", 8}, {"宁夏", 6}, {"青海", 5},
        {"西藏", 3}, {"香港", 88}, {"澳门", 82}, {"台湾", 75}
      };
      s.mapShowLabel = false;
      o.series = {s}; c->setOption(o); }

    int lastW = 0, lastH = 0;

    // Initial layout
    glfwGetWindowSize(window, &g_winW, &g_winH);
    layoutCharts(g_winW, g_winH);
    lastW = g_winW; lastH = g_winH;
    g_charts = &charts;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwGetWindowSize(window, &g_winW, &g_winH);

        // Relayout on window resize or scroll
        if (g_winW != lastW || g_winH != lastH || g_needsRelayout) {
            lastW = g_winW; lastH = g_winH;
            g_scrollY = std::max(0.0f, std::min(g_scrollY, std::max(0.0f, g_contentHeight - g_winH)));
            layoutCharts(g_winW, g_winH);
            g_needsRelayout = false;
        }

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        glViewport(0, 0, fbW, fbH);
        glClearColor(0.94f, 0.94f, 0.96f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float pixelRatio = (float)fbW / (float)g_winW;
        nvgBeginFrame(vg, (float)g_winW, (float)g_winH, pixelRatio);

        // Only draw visible charts
        for (auto& c : charts) {
            float cy = c->y();
            if (cy + ch > 0 && cy < g_winH) {
                c->draw(vg);
            }
        }

        // Draw scrollbar if needed
        if (g_contentHeight > g_winH) {
            float scrollbarH = g_winH * g_winH / g_contentHeight;
            float scrollbarY = g_scrollY * (g_winH - scrollbarH) / (g_contentHeight - g_winH);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, g_winW - 10, scrollbarY, 6, scrollbarH, 3);
            nvgFillColor(vg, nvgRGBA(150, 150, 150, 150));
            nvgFill(vg);
        }

        nvgEndFrame(vg);
        glfwSwapBuffers(window);
    }

    g_charts = nullptr;
    charts.clear();
    nvgDeleteGL3(vg);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
