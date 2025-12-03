# 内存池迁移指南

## 🎯 目标

将 nanovg_css 从传统 `new`/`delete` 迁移到高性能内存池。

---

## 📋 迁移清单

### Phase 1: 基础设施 (1-2 天)

- [x] 创建 `nanovg_css_memory.h` 包装器
- [ ] 实现 `memory_pool.c` (如果缺失)
- [ ] 添加单元测试
- [ ] 集成到 CMakeLists.txt

### Phase 2: Renderer 集成 (1 天)

- [ ] 添加 arena 到 `NVGCSSRenderer`
- [ ] 添加对象池到 `NVGCSSRenderer`
- [ ] 实现 `begin_frame()` / `end_frame()`

### Phase 3: 迁移热点代码 (2-3 天)

- [ ] TransitionState 分配
- [ ] AnimationState 分配
- [ ] 临时字符串操作
- [ ] SVG 路径解析

### Phase 4: 测试和优化 (1-2 天)

- [ ] 性能基准测试
- [ ] 内存泄漏检测
- [ ] 压力测试

---

## 🔧 详细步骤

### Step 1: 修改 NVGCSSRenderer

**文件**: `nanovg_css/include/nanovg_css_internal.h`

```cpp
#include "nanovg_css_memory.h"

class NVGCSSRenderer {
public:
    NVGCSSRenderer(NVGcontext* vg)
        : vg_(vg),
          arena_(4 * 1024 * 1024),  // 4MB arena
          transition_pool_(arena_),
          animation_pool_(arena_),
          painter_(vg, this) {
        frame_mark_ = 0;
    }
    
    // Frame lifecycle
    void begin_frame() {
        frame_mark_ = arena_.mark();
    }
    
    void end_frame() {
        arena_.rewind(frame_mark_);
    }
    
    // Memory management
    nvgcss::MemoryArena& arena() { return arena_; }
    nvgcss::ObjectPool<TransitionState>& transition_pool() { return transition_pool_; }
    nvgcss::ObjectPool<AnimationState>& animation_pool() { return animation_pool_; }

private:
    NVGcontext* vg_;
    
    // Memory management
    nvgcss::MemoryArena arena_;
    nvgcss::ObjectPool<TransitionState> transition_pool_;
    nvgcss::ObjectPool<AnimationState> animation_pool_;
    size_t frame_mark_;
    
    // ... rest of members
};
```

---

### Step 2: 修改 TransitionState 分配

**文件**: `nanovg_css/src/nanovg_css.cpp`

**Before** (Line 871):
```cpp
auto get_transition_state = [](NVGCSSElement* element) -> TransitionState* {
    if (!element->transition_state) {
        element->transition_state = new TransitionState();  // ❌
    }
    return static_cast<TransitionState*>(element->transition_state);
};
```

**After**:
```cpp
auto get_transition_state = [renderer](NVGCSSElement* element) -> TransitionState* {
    if (!element->transition_state) {
        element->transition_state = renderer->transition_pool().allocate();  // ✅
    }
    return static_cast<TransitionState*>(element->transition_state);
};
```

**清理** (Line 293):
```cpp
// Before
delete static_cast<TransitionState*>(element->transition_state);  // ❌

// After
renderer->transition_pool().deallocate(
    static_cast<TransitionState*>(element->transition_state)
);  // ✅
```

---

### Step 3: 修改 AnimationState 分配

**文件**: `nanovg_css/src/nanovg_css.cpp`

**Before** (Line 1024):
```cpp
auto get_animation_state = [](NVGCSSElement* elem) -> AnimationState* {
    if (!elem->animation_state) {
        elem->animation_state = new AnimationState();  // ❌
    }
    return static_cast<AnimationState*>(elem->animation_state);
};
```

**After**:
```cpp
auto get_animation_state = [renderer](NVGCSSElement* elem) -> AnimationState* {
    if (!elem->animation_state) {
        elem->animation_state = renderer->animation_pool().allocate();  // ✅
    }
    return static_cast<AnimationState*>(elem->animation_state);
};
```

---

### Step 4: 优化字符串操作

**文件**: `nanovg_css/src/nanovg_css_painter.cpp`

**Before** (Line 1800):
```cpp
std::string classes_str;
for (const auto& cls : element->classes) {
    classes_str += cls + " ";  // ❌ 多次分配
}
```

**After**:
```cpp
// 使用 arena 临时分配
nvgcss::MemoryScope scope(renderer_->arena());
char* classes_str = renderer_->arena().allocate_array<char>(256);
size_t offset = 0;

for (const auto& cls : element->classes) {
    size_t len = cls.size();
    if (offset + len + 2 < 256) {  // 边界检查
        memcpy(classes_str + offset, cls.c_str(), len);
        offset += len;
        classes_str[offset++] = ' ';
    }
}
classes_str[offset] = '\0';
```

---

### Step 5: 优化 SVG 路径解析

**文件**: `nanovg_css/src/nanovg_css_svg_path.cpp`

**Before**:
```cpp
std::vector<SVGPathCommand> parse(const std::string& d) {
    std::vector<SVGPathCommand> commands;  // ❌ 默认 allocator
    // ... 解析逻辑
    return commands;
}
```

**After**:
```cpp
// 添加 arena 参数
std::vector<SVGPathCommand, nvgcss::ArenaAllocator<SVGPathCommand>> 
parse(const std::string& d, nvgcss::MemoryArena& arena) {
    std::vector<SVGPathCommand, nvgcss::ArenaAllocator<SVGPathCommand>> 
        commands(nvgcss::ArenaAllocator<SVGPathCommand>(arena));  // ✅
    // ... 解析逻辑
    return commands;
}
```

---

### Step 6: 修复 malloc 内存泄漏

**文件**: `nanovg_css/src/nanovg_css_svg_xml.cpp`

**Before** (Line 397):
```cpp
char* result_str = (char*)malloc(css_str.size() + 1);  // ❌
if (result_str) {
    std::strcpy(result_str, css_str.c_str());
}
return result_str;
```

**After** (使用 arena):
```cpp
// 假设调用者传入 arena
char* result_str = arena.allocate_array<char>(css_str.size() + 1);  // ✅
std::strcpy(result_str, css_str.c_str());
return result_str;
```

**或者** (返回 std::string):
```cpp
// 最简单的方案
return css_str;  // ✅ 让调用者决定如何存储
```

---

## 🧪 测试策略

### 单元测试

**文件**: `nanovg_css/tests/test_memory_pool.cpp`

```cpp
#include <nanovg_css_memory.h>
#include <cassert>

void test_basic_allocation() {
    nvgcss::MemoryArena arena(1024);
    
    int* ptr = arena.allocate<int>(42);
    assert(*ptr == 42);
    
    std::cout << "✓ Basic allocation\n";
}

void test_object_pool() {
    nvgcss::MemoryArena arena(1024);
    nvgcss::ObjectPool<int> pool(arena);
    
    int* p1 = pool.allocate(10);
    int* p2 = pool.allocate(20);
    
    assert(*p1 == 10);
    assert(*p2 == 20);
    
    pool.deallocate(p1);
    pool.deallocate(p2);
    
    std::cout << "✓ Object pool\n";
}

void test_mark_rewind() {
    nvgcss::MemoryArena arena(1024);
    
    size_t mark1 = arena.mark();
    arena.allocate<int>(1);
    arena.allocate<int>(2);
    
    size_t used_before = arena.used();
    
    arena.rewind(mark1);
    
    assert(arena.used() < used_before);
    
    std::cout << "✓ Mark/Rewind\n";
}

int main() {
    test_basic_allocation();
    test_object_pool();
    test_mark_rewind();
    
    std::cout << "All tests passed!\n";
    return 0;
}
```

---

### 性能基准测试

**文件**: `nanovg_css/tests/benchmark_memory.cpp`

```cpp
#include <nanovg_css_memory.h>
#include <chrono>
#include <iostream>

struct TestObject {
    float data[16];
    TestObject() { for (int i = 0; i < 16; i++) data[i] = i; }
};

void benchmark_new_delete(int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        TestObject* obj = new TestObject();
        delete obj;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "new/delete: " << duration.count() << " µs\n";
}

void benchmark_pool(int iterations) {
    nvgcss::MemoryArena arena(1024 * 1024);
    nvgcss::ObjectPool<TestObject> pool(arena);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        TestObject* obj = pool.allocate();
        pool.deallocate(obj);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Pool: " << duration.count() << " µs\n";
}

int main() {
    const int ITERATIONS = 10000;
    
    std::cout << "Benchmarking " << ITERATIONS << " allocations:\n";
    benchmark_new_delete(ITERATIONS);
    benchmark_pool(ITERATIONS);
    
    return 0;
}
```

---

### 内存泄漏检测

**使用 Valgrind**:
```bash
valgrind --leak-check=full --show-leak-kinds=all \
    ./build/nanovg_css_tests
```

**使用 AddressSanitizer**:
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
make
./build/nanovg_css_tests
```

---

## 📊 预期结果

### 性能提升

| 操作 | Before | After | 提升 |
|------|--------|-------|------|
| TransitionState 分配 | 100ns | 10ns | **10x** |
| 字符串拼接 (100次) | 50µs | 5µs | **10x** |
| SVG 路径解析 | 200µs | 80µs | **2.5x** |
| 总帧时间 (1000元素) | 3.5ms | 1.8ms | **2x** |

### 内存使用

| 指标 | Before | After |
|------|--------|-------|
| 内存碎片 | 高 | 零 |
| 峰值内存 | 10MB | 6MB |
| 分配次数/帧 | 5000 | 50 |

---

## ⚠️ 常见陷阱

### 1. 忘记调用 begin_frame/end_frame

```cpp
// ❌ 错误：内存持续增长
void render_loop() {
    while (running) {
        renderer->render();  // 没有 rewind
    }
}

// ✅ 正确
void render_loop() {
    while (running) {
        renderer->begin_frame();
        renderer->render();
        renderer->end_frame();  // 自动 rewind
    }
}
```

### 2. 对象生命周期超出 arena

```cpp
// ❌ 错误：返回 arena 分配的指针
const char* get_class_name(nvgcss::MemoryArena& arena) {
    nvgcss::MemoryScope scope(arena);
    char* name = arena.allocate_array<char>(64);
    strcpy(name, "my-class");
    return name;  // ❌ scope 结束后 rewind，指针悬空
}

// ✅ 正确：返回 std::string
std::string get_class_name() {
    return "my-class";
}
```

### 3. 多线程竞态

```cpp
// ❌ 错误：多线程共享 arena
std::thread t1([&]() { arena.allocate<int>(); });
std::thread t2([&]() { arena.allocate<int>(); });  // 竞态

// ✅ 正确：每线程独立 arena
thread_local nvgcss::MemoryArena arena(1024 * 1024);
```

---

## 🎯 迁移时间表

| 阶段 | 时间 | 任务 |
|------|------|------|
| Week 1 | 2天 | 基础设施 + 单元测试 |
| Week 1 | 1天 | Renderer 集成 |
| Week 2 | 3天 | 迁移热点代码 |
| Week 2 | 2天 | 测试 + 优化 |
| **总计** | **8天** | **完整迁移** |

---

## ✅ 验收标准

- [ ] 所有单元测试通过
- [ ] 性能提升 >50%
- [ ] Valgrind 零泄漏
- [ ] 内存使用减少 >30%
- [ ] 代码审查通过

---

**迁移指南完成** - 准备开始实施 🚀
