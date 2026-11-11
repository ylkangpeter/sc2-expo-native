#pragma once

#include <iostream>

inline int g_tests_run = 0;
inline int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " << msg << std::endl; \
    } else { \
        g_tests_passed++; \
    } \
} while(0)
