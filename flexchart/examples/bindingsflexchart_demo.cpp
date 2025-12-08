#include <flexchart/flexchart.h>
#include <flexchart/bindingsbindings.h>
#include <cssbox.h>
#include <nanovg.h>
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <quickjs.h>
#include <iostream>
#include <fstream>
#include <sstream>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

// Helper: console.log implementation for QuickJS
static JSValue js_console_log(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    for (int i = 0; i < argc; i++) {
        if (i != 0) std::cout << " ";
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            std::cout << str;
            JS_FreeCString(ctx, str);
        }
    }
    std::cout << std::endl;
    return JS_UNDEFINED;
}

// Helper: Setup console object
void setupConsole(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", 
                      JS_NewCFunction(ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);
    JS_FreeValue(ctx, global);
}

// Helper function to execute JavaScript code
bool executeJS(JSContext* ctx, const char* code, const char* filename = "<eval>") {
    JSValue result = JS_Eval(ctx, code, strlen(code), filename, JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exception);
        std::cerr << "JavaScript Error: " << str << std::endl;
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, exception);
        JS_FreeValue(ctx, result);
        return false;
    }
    
    JS_FreeValue(ctx, result);
    return true;
}

// Helper function to load and execute JS file
bool executeJSFile(JSContext* ctx, const char* filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    return executeJS(ctx, code.c_str(), filepath);
}

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("FlexChart Demo - JavaScript Bindings",
        1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glCtx);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return 1;
    }

    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        std::cerr << "Failed to create NanoVG context" << std::endl;
        return 1;
    }

    nvgCreateFont(vg, "sans-serif", "C:/Windows/Fonts/segoeui.ttf");
    
    cssboxRenderer* renderer = cssboxCreateRenderer(vg);
    
    // Initialize QuickJS
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) {
        std::cerr << "Failed to create QuickJS runtime" << std::endl;
        return 1;
    }
    
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        std::cerr << "Failed to create QuickJS context" << std::endl;
        JS_FreeRuntime(rt);
        return 1;
    }
    
    // Setup FlexChart JavaScript bindings
    flexchart::setupChartBindings(ctx, renderer);
    
    // Setup console.log for debugging
    setupConsole(ctx);
    
    std::cout << "=== FlexChart JavaScript Bindings Demo ===" << std::endl;
    std::cout << "Executing JavaScript to create charts..." << std::endl;
    
    // Execute JavaScript code to create and configure charts
    const char* jsCode = R"(
        // Create line chart
        var lineChart = FlexChart.init('line-chart');
        lineChart.setPosition(50, 50);
        lineChart.resize(500, 300);
        lineChart.setOption(JSON.stringify({
            title: {
                text: 'Sales Trend',
                subtext: 'Monthly Data'
            },
            legend: { show: true },
            xAxis: {
                data: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun']
            },
            series: [
                {
                    type: 'line',
                    name: '2024',
                    data: [150, 230, 224, 218, 135, 147],
                    smooth: true
                },
                {
                    type: 'line',
                    name: '2023',
                    data: [120, 200, 150, 80, 70, 110],
                    smooth: true
                }
            ]
        }));
        
        // Note: Event listeners are commented out to avoid GC issues
        // lineChart.on('click', function(event) {
        //     console.log('Line chart clicked:', event.seriesIndex, event.dataIndex, event.value);
        // });
        
        // Create bar chart
        var barChart = FlexChart.init('bar-chart');
        barChart.setPosition(600, 50);
        barChart.resize(500, 300);
        barChart.setOption(JSON.stringify({
            title: { text: 'Revenue by Category' },
            legend: { show: true },
            xAxis: {
                data: ['Electronics', 'Clothing', 'Food', 'Books']
            },
            series: [
                {
                    type: 'bar',
                    name: 'Q1',
                    data: [320, 200, 150, 80]
                },
                {
                    type: 'bar',
                    name: 'Q2',
                    data: [280, 240, 180, 120]
                }
            ]
        }));
        
        // Create pie chart
        var pieChart = FlexChart.init('pie-chart');
        pieChart.setPosition(50, 400);
        pieChart.resize(500, 300);
        pieChart.setOption(JSON.stringify({
            title: { text: 'Market Share' },
            legend: { show: true, top: 'bottom' },
            xAxis: {
                data: ['Chrome', 'Firefox', 'Safari', 'Edge', 'Other']
            },
            series: [{
                type: 'pie',
                name: 'Browsers',
                data: [65.0, 15.0, 10.0, 5.0, 5.0],
                innerRadius: 0.5
            }]
        }));
        
        // Create scatter chart
        var scatterChart = FlexChart.init('scatter-chart');
        scatterChart.setPosition(600, 400);
        scatterChart.resize(500, 300);
        scatterChart.setOption(JSON.stringify({
            title: { text: 'Performance Metrics' },
            legend: { show: true },
            xAxis: {
                data: ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H']
            },
            series: [{
                type: 'scatter',
                name: 'Dataset',
                data: [80, 120, 50, 180, 90, 150, 70, 200],
                symbolSize: 8.0
            }]
        }));
        
        console.log('All charts created successfully!');
    )";
    
    if (!executeJS(ctx, jsCode)) {
        std::cerr << "Failed to execute JavaScript!" << std::endl;
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return 1;
    }
    
    std::cout << "Charts created via JavaScript successfully!" << std::endl;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float mx = event.motion.x;
                float my = event.motion.y;
                flexchart::ChartManager::instance().handleMouseMove(mx, my);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = event.button.x;
                float my = event.button.y;
                flexchart::ChartManager::instance().handleMouseDown(mx, my);
            }
        }

        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        
        glViewport(0, 0, width, height);
        glClearColor(0.94f, 0.94f, 0.96f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, (float)width, (float)height, 1.0f);
        
        // Render all charts managed by ChartManager
        flexchart::ChartManager::instance().renderAll(vg);
        
        nvgEndFrame(vg);

        SDL_GL_SwapWindow(window);
    }

    std::cout << "Shutting down..." << std::endl;
    
    // Cleanup - Important: destroy charts before freeing JS context
    flexchart::ChartManager::instance().shutdown();
    
    // Run garbage collection to clean up any remaining JS objects
    JS_RunGC(rt);
    
    // Free JS resources
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    cssboxDeleteRenderer(renderer);
    nvgDeleteGL3(vg);
    SDL_GL_DestroyContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Demo completed!" << std::endl;
    return 0;
}
