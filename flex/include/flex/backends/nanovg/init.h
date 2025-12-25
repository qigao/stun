/*
 * Flex Engine - NanoVG Backend Initialization
 *
 * Initialize Flex with NanoVG rendering backend.
 */

#pragma once

#include <fmtlog.h>

namespace flex {

// Initialize Flex with NanoVG backend
inline void init() {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");
    // NanoVG doesn't require global init
}

inline void shutdown() {
    fmtlog::shutdown();
}

} // namespace flex
