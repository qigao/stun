# FlexUI JavaScript 模块示例

这个示例展示如何使用 ES6 模块组织 FlexUI 的 JavaScript 代码。

## 文件结构

```
js_modules/
├── main.js       # 主入口，导入并使用模块
├── counter.js    # 计数器模块
└── timer.js      # 定时器模块
```

## 使用方法

### C++ 端

```cpp
#include <flexui/screen.h>

int main() {
    flexui::Screen screen(500, 400, "Module Demo");
    
    // 加载 UI
    screen.loadXML("...");
    screen.loadCSS("...");
    
    // 加载 ES6 模块（会自动加载依赖）
    screen.loadJSModule("flexui/examples/js_modules/main.js");
    
    while (screen.pollEvents()) {
        screen.draw();
    }
}
```

### JavaScript 端

**counter.js** - 导出可复用的类
```javascript
export class Counter {
    constructor(displayId) {
        this.count = 0;
        this.displayId = displayId;
    }
    
    increment() {
        this.count++;
        this.updateDisplay();
    }
}
```

**main.js** - 导入并使用
```javascript
import { Counter } from './counter.js';

const counter = new Counter('counter-display');

// 导出给 XML onclick 使用
globalThis.increment = function() {
    counter.increment();
};
```

## 内置标准库

FlexUI 提供了内置的标准库模块 `flexui`：

```javascript
import { FlexUI, WidgetHelper, Animation, EventBus } from 'flexui';

// 使用 WidgetHelper 简化操作
WidgetHelper.setText('status', 'Hello!');
const name = WidgetHelper.getText('input');

// 使用 Animation 工具
Animation.animate(1000, (progress) => {
    console.log('Progress:', progress);
});

// 使用 EventBus 解耦组件
const bus = new EventBus();
bus.on('increment', () => counter.increment());
bus.emit('increment');
```

## 注意事项

1. **路径必须明确**：使用 `./counter.js` 而不是 `counter`
2. **必须有 .js 后缀**：模块加载器不会自动添加
3. **相对路径**：相对于当前工作目录，不是相对于模块文件
4. **globalThis**：模块作用域是隔离的，需要通过 globalThis 暴露给 XML onclick
5. **内置模块**：`import from 'flexui'` 不需要路径，直接使用模块名

## 对比

### 旧方式（单文件）
```javascript
screen.loadJS(R"JS(
    var counter = 0;
    function increment() { counter++; }
    function decrement() { counter--; }
    // 200 行代码全在一起...
)JS");
```

### 新方式（模块化）
```javascript
screen.loadJSModule("main.js");
// main.js 自动加载 counter.js 和 timer.js
// 代码分离，可复用
```
