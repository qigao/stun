# FlexChart JavaScript Runtime

## 概述

FlexChart Runtime 提供了 JavaScript 脚本支持，让你可以通过 JavaScript 代码创建和配置图表。

## 功能特性

- ✅ **QuickJS 集成**：使用 QuickJS 引擎执行 JavaScript 代码
- ✅ **JavaScript API**：通过 JavaScript 创建和配置图表
- ✅ **ChartManager**：统一管理所有图表实例
- ✅ **JSON 配置**：使用 JSON 格式配置图表选项
- ✅ **鼠标交互**：支持悬停和点击交互

## JavaScript API

### 创建图表

```javascript
var chart = FlexChart.init('chart-id');
```

### 设置位置和大小

```javascript
chart.setPosition(x, y);
chart.resize(width, height);
```

### 配置图表

```javascript
chart.setOption(JSON.stringify({
    title: {
        text: 'Chart Title',
        subtext: 'Subtitle'
    },
    legend: { show: true },
    xAxis: {
        data: ['A', 'B', 'C', 'D']
    },
    series: [
        {
            type: 'line',  // 或 'bar', 'pie', 'scatter' 等
            name: 'Series 1',
            data: [10, 20, 30, 40],
            smooth: true
        }
    ]
}));
```

### 事件监听

```javascript
chart.on('click', function(event) {
    console.log('Clicked:', event.seriesIndex, event.dataIndex, event.value);
});
```

## 已知问题

### 事件监听器内存泄漏

**问题描述**：
当前实现中，`chart.on()` 注册的事件监听器会导致 QuickJS 垃圾回收断言失败：
```
Assertion failed: list_empty(&rt->gc_obj_list), file quickjs.c, line 2145
```

**原因**：
在 `runtime.cpp` 的 `js_chart_on` 函数中，JavaScript 回调函数通过 `JS_DupValue` 复制后存储在 C++ lambda 中，但在 chart 销毁时没有被正确释放。

```cpp
// 问题代码片段
JSValue callback = JS_DupValue(ctx, argv[1]);  // 复制 JSValue
w->chart->on(event, [ctx, callback](const ChartEvent& e) {
    // callback 被捕获，但永远不会被 JS_FreeValue
    ...
});
```

**临时解决方案**：
当前 demo 中事件监听器已被注释掉。如果需要使用事件，请注意在程序退出前可能会遇到断言错误。

**完整解决方案**（需要修改 `runtime.cpp`）：

1. 在 `ChartWrapper` 中存储回调引用：
```cpp
struct ChartWrapper {
    FlexChart* chart;
    std::unordered_map<std::string, JSValue> callbacks;  // 存储回调
};
```

2. 在 finalizer 中释放所有回调：
```cpp
static void js_chart_finalizer(JSRuntime* rt, JSValue val) {
    ChartWrapper* w = static_cast<ChartWrapper*>(JS_GetOpaque(val, js_chart_class_id));
    if (w) {
        // 释放所有注册的回调
        for (auto& [event, callback] : w->callbacks) {
            JS_FreeValueRT(rt, callback);
        }
        delete w;
    }
}
```

3. 在 `js_chart_on` 中存储回调：
```cpp
static JSValue js_chart_on(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
    // ... 获取 w 和 event ...
    
    JSValue callback = JS_DupValue(ctx, argv[1]);
    
    // 存储旧的回调（如果有）以便释放
    auto it = w->callbacks.find(event);
    if (it != w->callbacks.end()) {
        JS_FreeValue(ctx, it->second);
    }
    
    // 存储新的回调
    w->callbacks[event] = callback;
    
    w->chart->on(event, [ctx, callback](const ChartEvent& e) {
        // ... 事件处理 ...
    });
    
    return JS_UNDEFINED;
}
```

## 编译和运行

```bash
cmake --build build --target bindingsflexchart_demo
./build/bin/bindingsflexchart_demo
```

## 示例输出

```
=== FlexChart JavaScript Bindings Demo ===
Executing JavaScript to create charts...
All charts created successfully!
Charts created via JavaScript successfully!
[鼠标移动和点击图表进行交互]
Shutting down...
Demo completed!
```

## 架构说明

### C++ 层
1. 初始化 QuickJS runtime 和 context
2. 设置 FlexChart 的 JavaScript 运行时（`setupChartRuntime`）
3. 设置 `console.log` 支持
4. 执行 JavaScript 代码创建图表
5. 渲染循环：处理事件 + 绘制所有图表
6. 清理：销毁图表 → GC → 释放 JS 资源

### JavaScript 层
- 完全用 JavaScript 定义图表配置
- JSON 格式配置（类似 ECharts）
- 事件驱动的交互模型

### 依赖
- `flexchart`：图表库
- `cssbox`：CSS 渲染器
- `qjs`：QuickJS JavaScript 引擎
- `SDL3::SDL3`：窗口和输入系统
- `glad::glad`：OpenGL 加载器

## 扩展建议

1. **从外部文件加载配置**
   ```cpp
   executeJSFile(ctx, "chart_config.js");
   ```

2. **动态更新图表**
   ```javascript
   var chart = FlexChart.getChart('chart-id');
   chart.setOption(newConfig);
   ```

3. **双向通信**
   - C++ 调用 JavaScript 函数
   - JavaScript 调用 C++ API

4. **更复杂的事件处理**
   - 支持多个监听器
   - 支持事件移除（`off` 方法）
