#pragma once

#include <spdlog/common.h>

namespace jar {

class LoggerInitializer {
public:
    static LoggerInitializer&
    instance();

    void
    initialize(const spdlog::filename_t& fileName);

private:
    LoggerInitializer() = default;

private:
    bool _initialized{false};
};

} // namespace jar
