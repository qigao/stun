#pragma once

#include "shadcn.h"
#include "fluent.h"

namespace flexui {
namespace themes {

enum class Theme {
    Shadcn,
    Fluent
};

inline const char* get(Theme theme) {
    switch (theme) {
        case Theme::Shadcn: return shadcn::css;
        case Theme::Fluent: return fluent::css;
        default: return shadcn::css;
    }
}

} // namespace themes
} // namespace flexui
