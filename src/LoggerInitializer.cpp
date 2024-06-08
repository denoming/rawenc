#include "LoggerInitializer.hpp"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace fs = std::filesystem;

class ShortFilenameAndLine : public spdlog::custom_flag_formatter {
public:
    void
    format(const spdlog::details::log_msg& msg,
           const std::tm&,
           spdlog::memory_buf_t& buffer) override
    {
        static constexpr const char* kFullFormat{"{:.<30}"};
        static constexpr const char* kNullFormat{"{:^5}"};

        if (msg.source.empty()) {
            fmt::format_to(std::back_inserter(buffer), kNullFormat, std::string{"?:?"});
        } else {
            auto fileNameAndLine{fs::path{msg.source.filename}.filename().string()};
            fileNameAndLine += ':';
            fileNameAndLine += std::to_string(msg.source.line);
            fmt::format_to(std::back_inserter(buffer), kFullFormat, fileNameAndLine);
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter>
    clone() const override
    {
        return spdlog::details::make_unique<ShortFilenameAndLine>();
    }
};

namespace jar {

namespace {

spdlog::level::level_enum
getLogLevel()
{
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_OFF
    return spdlog::level::off;
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
    return spdlog::level::debug;
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO
    return spdlog::level::info;
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_WARN
    return spdlog::level::warn;
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_ERROR
    return spdlog::level::err;
#else
    return spdlog::level::info;
#endif
}

void
setupDefaultLogger()
{
    auto logger = std::make_shared<spdlog::logger>("rawenc");
    logger->set_level(getLogLevel());
    set_default_logger(std::move(logger));
}

void
addFileSink(const spdlog::filename_t& fileName)
{
    using Sink = spdlog::sinks::basic_file_sink_mt;

    static constexpr const char* kFormat{"[%L] [%P:%t] [%*] %v"};
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ShortFilenameAndLine>('*').set_pattern(kFormat);

    if (const fs::path filePath{fileName}; filePath.has_parent_path()) {
        std::error_code ec;
        create_directories(filePath.parent_path(), ec);
    }

    const auto sink = std::make_shared<Sink>(fileName, true);
    sink->set_level(getLogLevel());
    sink->set_formatter(std::move(formatter));
    spdlog::default_logger()->sinks().push_back(sink);
}

} // namespace

LoggerInitializer&
LoggerInitializer::instance()
{
    static LoggerInitializer instance;
    return instance;
}

void
LoggerInitializer::initialize(const spdlog::filename_t& fileName)
{
    if (not _initialized) {
        setupDefaultLogger();
        addFileSink(fileName);
        _initialized = true;
    }
}

} // namespace jar
