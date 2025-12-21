/*
 * Shortcut Manager
 *
 * Centralized keyboard shortcut system.
 * Maps key combinations to actions with customization support.
 */

#pragma once

#include "../core/types.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

namespace editor {

// Key modifiers
enum class Modifier : uint8_t {
    None  = 0,
    Ctrl  = 1 << 0,
    Shift = 1 << 1,
    Alt   = 1 << 2,
};

inline Modifier operator|(Modifier a, Modifier b) {
    return static_cast<Modifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(Modifier a, Modifier b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

// Common key codes (matching SDL2 SDLK_* values)
namespace Key {
    constexpr int Backspace = 8;
    constexpr int Tab = 9;
    constexpr int Return = 13;
    constexpr int Escape = 27;
    constexpr int Space = 32;
    constexpr int Delete = 127;

    // Letters (uppercase ASCII)
    constexpr int A = 'A', B = 'B', C = 'C', D = 'D', E = 'E';
    constexpr int F = 'F', G = 'G', H = 'H', I = 'I', J = 'J';
    constexpr int K = 'K', L = 'L', M = 'M', N = 'N', O = 'O';
    constexpr int P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T';
    constexpr int U = 'U', V = 'V', W = 'W', X = 'X', Y = 'Y';
    constexpr int Z = 'Z';

    // Numbers
    constexpr int Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4';
    constexpr int Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9';

    // Function keys (SDL2 values)
    constexpr int F1 = 0x4000003A, F2 = 0x4000003B, F3 = 0x4000003C;
    constexpr int F4 = 0x4000003D, F5 = 0x4000003E, F6 = 0x4000003F;
    constexpr int F7 = 0x40000040, F8 = 0x40000041, F9 = 0x40000042;
    constexpr int F10 = 0x40000043, F11 = 0x40000044, F12 = 0x40000045;

    // Arrow keys
    constexpr int Left = 0x40000050, Right = 0x4000004F;
    constexpr int Up = 0x40000052, Down = 0x40000051;

    // Special
    constexpr int Plus = '+', Minus = '-', Equals = '=';
    constexpr int BracketLeft = '[', BracketRight = ']';
}

// Shortcut definition
struct Shortcut {
    int key = 0;
    Modifier modifiers = Modifier::None;

    Shortcut() = default;
    Shortcut(int k, Modifier m = Modifier::None) : key(k), modifiers(m) {}

    bool matches(int k, bool ctrl, bool shift, bool alt) const {
        if (key != k) return false;
        bool wantCtrl = modifiers & Modifier::Ctrl;
        bool wantShift = modifiers & Modifier::Shift;
        bool wantAlt = modifiers & Modifier::Alt;
        return ctrl == wantCtrl && shift == wantShift && alt == wantAlt;
    }

    std::string toString() const;
};

// Action categories for organization
enum class ActionCategory {
    File,
    Edit,
    View,
    Object,
    Tool,
    Arrange,
};

// Action definition
struct Action {
    std::string id;
    std::string name;
    ActionCategory category;
    Shortcut shortcut;
    std::function<void()> execute;
    std::function<bool()> canExecute;  // Optional: returns false to disable

    Action() = default;
    Action(const std::string& id_, const std::string& name_, ActionCategory cat,
           Shortcut sc, std::function<void()> exec,
           std::function<bool()> canExec = nullptr)
        : id(id_), name(name_), category(cat), shortcut(sc), execute(exec), canExecute(canExec) {}
};

// Forward declaration
class EditorViewModel;

// Shortcut Manager
class ShortcutManager {
public:
    ShortcutManager() = default;

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Register action
    void registerAction(const Action& action);

    // Handle key press - returns true if handled
    bool handleKey(int key, bool ctrl, bool shift, bool alt);

    // Execute action by ID
    bool executeAction(const std::string& id);

    // Get all actions
    const std::vector<Action>& actions() const { return actions_; }

    // Get actions by category
    std::vector<const Action*> actionsByCategory(ActionCategory cat) const;

    // Customize shortcut
    void setShortcut(const std::string& actionId, const Shortcut& sc);

    // Setup default shortcuts
    void setupDefaults();

private:
    EditorViewModel* vm_ = nullptr;
    std::vector<Action> actions_;
    std::unordered_map<std::string, size_t> action_index_;
};

} // namespace editor
