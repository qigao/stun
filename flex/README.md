# Flex Engine - 动画系统

## 📚 文档导航

- **[dsl.md](docs/dsl.md)** - DSL 语法规范（当前实现）
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - 实际架构和实现细节
- **[VISION.md](docs/VISION.md)** - 理论设计和未来愿景

---

## 🎯 项目简介

Flex Engine 是一个现代化的 2.5D 游戏引擎和 UI 框架，具有强大的动画系统。本项目实现了完整的动画系统，包括类型安全的 C++ API、声明式 DSL 语法，以及可插拔的渲染后端接入能力。

## 🧭 模块命名

当前推荐的语义化入口是：

- `#include "flex/dsl.h"`：DSL 前端，负责 lexer/parser/AST
- `#include "flex/core.h"`：核心执行模型，负责 scene/layout/timeline/binding
- `#include "flex/lowering.h"`：DSL 到 core object 的装配/落地
- `#include "flex/bridge/renderer.h"`：公开的后端无关 renderer factory/registry

`backends/*` 目录中的 backend init / register 头主要用于仓库内或宿主应用集成层，不是面向安装包终端用户的公开表面。

旧名字 `flex/compiler.h`、`flex/runtime.h`、`flex/bridge/*` 仍然保留，用作兼容层。

## ✨ 核心特性

### 动画系统
- ✅ **类型安全**: 使用 `std::variant` 实现编译时类型检查
- ✅ **多类型动画**: 支持 float、string、Color 动画值
- ✅ **智能插值**: 自动计算关键帧之间的中间值
- ✅ **缓动函数**: linear、ease、ease-in、ease-out、ease-in-out
- ✅ **循环模式**: Once、Loop、PingPong
- ✅ **动画混合**: Override、Additive、Multiply 模式
- ✅ **实时渲染**: 支持 ThorVG / NanoVG / Direct2D / TUI 等可插拔后端

### 模块导入
```flex
// main.flex - 导入其他模块
import "animations/fade.flex"
import "components/button.flex"
import "machines/player_state.flex"

scene main {
  // 使用导入的动画和组件
}
```

导入特性：
- 支持相对路径导入
- 自动检测循环导入（跳过已加载文件）
- 合并 animations、machines、components、assets
- 递归处理嵌套导入

### DSL 动画定义
```flex
anim "PlayerMove" {
    duration: 3s
    loop: loop

    track "x" {
        keyframe 0s -> 100
        keyframe 0.5s -> 300
        keyframe 1s -> 500
    }

    track "y" {
        keyframe 0s -> 400
        keyframe 0.5s -> 300
        keyframe 1s -> 400
    }
}
```

### C++ API
```cpp
// 创建动画
auto anim = flex::Timeline::create("Move", *instance->object_allocator());
auto track = anim->add_track("x");
track->add_keyframe(0.0f, 100.0f);
track->add_keyframe(1.0f, 200.0f);

// 播放动画
instance->add_timeline(anim);
instance->play("Move", player_node);
```

## 📁 项目结构

```
flex/
├── include/flex/
│   ├── dsl.h              # 语义化 DSL 前端入口
│   ├── core.h             # 语义化核心运行时入口
│   ├── lowering.h         # AST/runtime 装配入口
│   ├── bridge/renderer.h  # 公开 renderer registry/factory
│   ├── compiler.h         # 旧入口（兼容）
│   ├── runtime.h          # 旧入口（兼容）
│   └── bridge/            # 旧路径（兼容）
├── src/
│   ├── dsl/               # DSL frontend implementation
│   ├── core/              # Core runtime implementation
│   ├── lowering/          # AST/runtime lowering implementation
│   ├── backends/          # Backend 实现与仓库内集成头
│   └── binary/            # .flexb format support
├── examples/               # 示例程序
│   ├── *_demo.cpp         # SDL/ThorVG/TUI examples
│   └── *.flex             # DSL examples
├── docs/
│   └── dsl.md             # DSL reference
└── CMakeLists.txt          # 构建配置
```

## 🚀 快速开始

### 1. 编译项目

```bash
# 进入项目目录
cd flex

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake ..

# 编译
cmake --build . --config Release
```

### 2. 运行示例

```bash
# ThorVG 动画演示
./bin/loading_animation_demo.exe

# NanoVG + GLFW 演示
./bin/nanovg_glfw_demo.exe

# Direct2D + HWND 演示（Windows）
./bin/direct2d_hwnd_demo.exe

# 终端渲染演示
./bin/tui_flex_demo.exe
```

### 3. 创建你的第一个动画

```cpp
#include <flex.h>
#include <iostream>

int main() {
    // 创建实例
    auto instance = flex::Instance::create(800, 600);
    auto* scene = instance->scene();
    auto* object_alloc = instance->object_allocator();
    if (!object_alloc) return 1;

    // 创建节点（当前 core API 由对象分配器持有节点生命周期）
    auto* player = flex::Shape::create(*object_alloc);
    player->set_rect(80, 80);
    player->set_position(100, 350);
    player->set_fill(flex::Color::Blue);
    scene->add_child(player);

    // 创建动画
    auto anim = flex::Timeline::create("Move", *object_alloc);
    anim->set_loop_mode(flex::LoopMode::Loop);

    auto track = anim->add_track("x");
    track->add_keyframe(0.0f, 100.0f);
    track->add_keyframe(1.0f, 500.0f);
    track->add_keyframe(2.0f, 100.0f);

    instance->add_timeline(anim);
    instance->play("Move", player);

    // 模拟游戏循环
    for (int i = 0; i < 60; i++) {
        instance->advance(1.0f / 60.0f);
        std::cout << "Frame " << i << ": x = " << player->x() << "\n";
    }

    return 0;
}
```

## 📚 文档

### 核心文档
- **[ANIMATION.md](docs/ANIMATION.md)** - DSL 动画完整指南
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - 当前模块边界与装配方式
- **[RENDERING_BACKENDS.md](RENDERING_BACKENDS.md)** - 当前 backend registry / plugin 结构

### 示例程序
| 示例 | 描述 | 依赖 |
|------|------|------|
| `loading_animation_demo.cpp` | 动画与状态演示 | ThorVG |
| `nanovg_glfw_demo.cpp` | NanoVG + GLFW 集成 | NanoVG + GLFW + OpenGL |
| `direct2d_hwnd_demo.cpp` | Direct2D + HWND 集成 | Direct2D |
| `tui_flex_demo.cpp` | 终端渲染演示 | TUI |

## 🎮 应用场景

### 1. 游戏开发
- 角色移动和跳跃动画
- UI 界面过渡效果
- 特效动画（爆炸、粒子）
- 场景切换动画

### 2. UI/UX 设计
- 按钮悬停效果
- 页面转场动画
- 加载动画
- 进度条动画

### 3. 数据可视化
- 图表绘制动画
- 数据更新过渡
- 交互反馈动画

## 🔧 技术栈

### 核心
- **C++17** - 现代 C++ 特性
- **std::variant** - 类型安全联合体
- **CMake** - 跨平台构建

### 渲染
- **ThorVG / NanoVG / Direct2D / TUI** - 可插拔渲染后端
- **SDL2 / GLFW / Win32** - 典型宿主窗口与输入集成方式

### 工具链
- **re2c** - 词法分析器生成器
- **Lemon** - 解析器生成器

## 🐛 调试与性能分析

Flex Engine 支持通过环境变量启用调试日志和性能分析，无需重新编译：

### 环境变量

| 变量 | 值 | 描述 |
|------|-----|------|
| `FLEX_DEBUG` | `1` 或 `true` | 启用调试日志输出 |
| `FLEX_PROFILE` | `1` 或 `true` | 启用性能分析（后台线程收集） |
| `FLEX_LOG_LEVEL` | `DBG`/`INF`/`WRN`/`ERR` | 设置日志级别 |

### 使用示例

```bash
# 启用调试日志
FLEX_DEBUG=1 ./your_app

# 启用性能分析
FLEX_PROFILE=1 ./your_app

# 同时启用调试和性能分析
FLEX_DEBUG=1 FLEX_PROFILE=1 ./your_app

# 只显示警告和错误
FLEX_LOG_LEVEL=WRN FLEX_DEBUG=1 ./your_app
```

### Windows (PowerShell)

```powershell
$env:FLEX_DEBUG=1; ./your_app.exe
$env:FLEX_PROFILE=1; ./your_app.exe
```

### Windows (CMD)

```cmd
set FLEX_DEBUG=1 && your_app.exe
set FLEX_PROFILE=1 && your_app.exe
```

### 在代码中初始化

```cpp
#include <flex.h>

int main() {
    // 初始化调试/性能系统（读取环境变量）
    flex::debug::init();

    // ... 你的代码 ...

    // 关闭时刷新日志和性能数据
    flex::debug::shutdown();
    return 0;
}
```

### 获取性能统计

```cpp
// 获取作用域统计
auto scope_stats = flex::debug::get_scope_stats();
for (const auto& [name, stats] : scope_stats) {
    printf("%s: count=%lld, avg=%.2fus\n",
           name.c_str(), stats.count, stats.avg_ns() / 1000.0f);
}

// 获取帧统计
auto frame_stats = flex::debug::get_frame_stats();
printf("FPS: %.1f\n", frame_stats.avg_fps());
```

## 📊 性能指标 (实测数据)

**✅ 经过验证的高性能** - 详见 [FINAL_PERFORMANCE_REPORT.md](docs/FINAL_PERFORMANCE_REPORT.md)

- **复杂场景**: **120 FPS** (500 nodes + 100 animations) - 2x 超越 60 FPS 目标
- **渲染性能**: **96 FPS** @ 1000 shapes
- **节点更新**: **66 ns/node** - 业界领先
- **动画采样**: **241 ns/sample** - 可支持数千条并发动画
- **内存分配**: **5.7x 快于 malloc/free** - Arena allocator
- **内存占用**: **0.06 MB** for 500 nodes (极低)

**性能优化**: 通过 bounds 缓存和内联优化,性能提升 **43%**

详细 benchmark 结果请查看:
- [PERFORMANCE.md](PERFORMANCE.md) - 完整性能报告
- [docs/FINAL_PERFORMANCE_REPORT.md](docs/FINAL_PERFORMANCE_REPORT.md) - 最终报告

## 🎯 支持的动画类型

### 变换动画
- `x`, `y` - 位置
- `rotation` - 旋转
- `scaleX`, `scaleY` - 缩放

### 外观动画
- `opacity` - 透明度
- `fill.color` - 填充颜色
- `stroke.color` - 描边颜色

### 几何动画
- `width`, `height` - 尺寸
- `radius` - 半径

## 🏆 项目成就

- ✅ **类型安全** - 编译时类型检查，无运行时错误
- ✅ **功能完整** - 支持所有常见动画需求
- ✅ **易于使用** - 简洁的 API 和 DSL 语法
- ✅ **高性能** - 优化的插值和内存管理
- ✅ **跨平台** - Windows、Linux、macOS
- ✅ **可扩展** - 易于添加新特性

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 开发流程
1. Fork 项目
2. 创建特性分支
3. 提交更改
4. 创建 Pull Request

### 代码规范
- 遵循 C++ Core Guidelines
- 添加单元测试
- 更新文档

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 🙏 致谢

- **ThorVG** - 优秀的向量图形库
- **SDL2** - 跨平台多媒体库
- **Flex/Lex** - 词法和语法分析器

## 📞 联系我们

- **项目主页**: https://github.com/your-org/flex-engine
- **问题反馈**: https://github.com/your-org/flex-engine/issues
- **讨论区**: https://github.com/your-org/flex-engine/discussions

---

**Flex Engine - 让动画更简单！** 🎨✨

*Built with ❤️ by the Flex Engine Team*
