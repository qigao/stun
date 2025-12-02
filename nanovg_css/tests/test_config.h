/**
 * Test Configuration - Global Setup for All Tests
 *
 * This header provides common test utilities and logging configuration.
 * Include this at the top of test files that need clean output.
 */

#ifndef NANOVG_CSS_TEST_CONFIG_H
#define NANOVG_CSS_TEST_CONFIG_H

#include <fmtlog.h>

/**
 * Global test initializer - disables verbose INFO logs during tests
 *
 * Usage: Add this at the top of your test file (after includes):
 *   #include "test_config.h"
 *   INIT_TEST_LOGGING();
 */
#define INIT_TEST_LOGGING() \
    namespace { \
        struct TestLogConfig { \
            TestLogConfig() { \
                fmtlog::setLogLevel(fmtlog::WRN); \
            } \
        } _test_log_init; \
    }

/**
 * Enable debug logging for specific test (use inside TEST_CASE)
 *
 * Usage:
 *   TEST_CASE("My test") {
 *       ENABLE_DEBUG_LOGGING();
 *       // ... test code with verbose logs
 *   }
 */
#define ENABLE_DEBUG_LOGGING() fmtlog::setLogLevel(fmtlog::DBG)

/**
 * Disable all logging for specific test
 */
#define DISABLE_ALL_LOGGING() fmtlog::setLogLevel(fmtlog::OFF)

#endif // NANOVG_CSS_TEST_CONFIG_H
