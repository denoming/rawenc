#pragma once

#include <spdlog/spdlog.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>

// clang-format off
#define LOGT(...) \
    SPDLOG_TRACE(__VA_ARGS__)
#define LOGD(...) \
    SPDLOG_DEBUG(__VA_ARGS__)
#define LOGI(...) \
    SPDLOG_INFO(__VA_ARGS__)
#define LOGW(...) \
    SPDLOG_WARN(__VA_ARGS__)
#define LOGE(...) \
    SPDLOG_ERROR(__VA_ARGS__)
// clang-format on
