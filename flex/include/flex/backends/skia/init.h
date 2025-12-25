/*
 * Flex Engine - Skia Backend Initialization
 *
 * Initialize Flex with Skia rendering backend.
 */

#pragma once

#include <fmtlog.h>

namespace flex {

// Initialize Flex with Skia backend
inline void init() {
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setThreadName("main");
    // Skia doesn't require global init
}

inline void shutdown() {
    fmtlog::shutdown();
}

} // namespace flex
