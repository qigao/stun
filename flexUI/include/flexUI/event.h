/*
 * flexUI - Event System
 *
 * 统一的事件表示
 */

#ifndef FLEXUI_EVENT_H
#define FLEXUI_EVENT_H

#include <string>

namespace flexUI {

// 前向声明
class Element;

// ============================================================================
// 事件类型
// ============================================================================

enum class EventType {
  // 鼠标事件
  MouseMove,
  MouseDown,
  MouseUp,
  MouseWheel,
  MouseEnter,
  MouseLeave,

  // 键盘事件
  KeyDown,
  KeyUp,
  TextInput,

  // 焦点事件
  FocusIn,
  FocusOut,

  // IME 事件
  CompositionStart,
  CompositionUpdate,
  CompositionEnd,

  // 触摸事件（未来）
  TouchStart,
  TouchMove,
  TouchEnd,

  /// Synthesized after an unconsumed matching MouseDown/MouseUp activation.
  /// Appended to preserve the numeric values of existing native event kinds.
  Click,
};

enum class MouseButton {
  Left = 0,
  Right = 1,
  Middle = 2,
};

enum class KeyCode {
  Unknown = 0,

  // 字母
  A = 65,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,

  // 数字
  Num0 = 48,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,

  // 功能键
  Escape = 256,
  Enter = 257,
  Space = 32,
  Tab = 258,
  Backspace = 259,
  Delete = 261,
  Insert = 260,

  // 方向键
  Left = 263,
  Right = 262,
  Up = 265,
  Down = 264,
  Home = 268,
  End = 269,
  PageUp = 266,
  PageDown = 267,

  // 修饰键
  Shift = 340,
  Control = 341,
  Alt = 342,
  Super = 343,
};

enum class KeyMod {
  None = 0,
  Shift = 1 << 0,
  Control = 1 << 1,
  Alt = 1 << 2,
  Super = 1 << 3,
};

// ============================================================================
// 事件对象
// ============================================================================

/**
 * Event - 统一的事件表示
 *
 * 设计原则：
 * 1. 用 type 字段区分事件类型
 * 2. 所有事件数据在一个结构体中
 * 3. 不相关的字段为默认值
 */
struct Event {
  EventType type;

  // 鼠标事件数据
  float x = 0;       // 鼠标 X 坐标（窗口坐标系）
  float y = 0;       // 鼠标 Y 坐标
  float delta_x = 0; // 鼠标滚轮/移动增量
  float delta_y = 0;
  MouseButton button = MouseButton::Left;

  // 键盘事件数据
  KeyCode key = KeyCode::Unknown;
  int mods = 0;                 // KeyMod 位标记
  std::string text;             // TextInput 事件的文本（UTF-8）
  std::string composition_text; // IME 构字文本

  // 目标元素（事件路由后填充）
  Element *target = nullptr;

  // 时间戳
  float timestamp_ms = 0;

  // 控制标记
  bool handled = false;  // Widget 是否消费了这个事件
  bool propagate = true; // 是否继续传播到父元素

  // ========== 便捷构造函数 ==========

  static Event mouse_move(float x, float y) {
    Event e;
    e.type = EventType::MouseMove;
    e.x = x;
    e.y = y;
    return e;
  }

  static Event mouse_down(float x, float y, MouseButton btn = MouseButton::Left) {
    Event e;
    e.type = EventType::MouseDown;
    e.x = x;
    e.y = y;
    e.button = btn;
    return e;
  }

  static Event mouse_up(float x, float y, MouseButton btn = MouseButton::Left) {
    Event e;
    e.type = EventType::MouseUp;
    e.x = x;
    e.y = y;
    e.button = btn;
    return e;
  }

  static Event mouse_wheel(float x, float y, float dx, float dy) {
    Event e;
    e.type = EventType::MouseWheel;
    e.x = x;
    e.y = y;
    e.delta_x = dx;
    e.delta_y = dy;
    return e;
  }

  static Event key_down(KeyCode key, int mods = 0) {
    Event e;
    e.type = EventType::KeyDown;
    e.key = key;
    e.mods = mods;
    return e;
  }

  static Event key_up(KeyCode key, int mods = 0) {
    Event e;
    e.type = EventType::KeyUp;
    e.key = key;
    e.mods = mods;
    return e;
  }

  static Event text_input(const std::string &text) {
    Event e;
    e.type = EventType::TextInput;
    e.text = text;
    return e;
  }

  static Event focus_in() {
    Event e;
    e.type = EventType::FocusIn;
    return e;
  }

  static Event focus_out() {
    Event e;
    e.type = EventType::FocusOut;
    return e;
  }

  static Event composition_start() {
    Event e;
    e.type = EventType::CompositionStart;
    return e;
  }

  static Event composition_update(const std::string &text) {
    Event e;
    e.type = EventType::CompositionUpdate;
    e.composition_text = text;
    return e;
  }

  static Event composition_end() {
    Event e;
    e.type = EventType::CompositionEnd;
    return e;
  }
};

} // namespace flexUI

#endif // FLEXUI_EVENT_H
