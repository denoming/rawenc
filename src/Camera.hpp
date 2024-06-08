#pragma once

#include <sigc++/signal.h>

#include <optional>
#include <string>
#include <vector>
#include <thread>

namespace jar {

struct CameraConfig {
    unsigned width{};
    unsigned height{};
    unsigned bufferCount{};
};

struct CapturedFrame {
    unsigned sequence{};
    void* data{};
    unsigned int size{};
};

class Camera {
public:
    static constexpr int kInvalidFd = -1;

    using OnFrameReadySig = sigc::signal<void(const CapturedFrame& frame)>;

    explicit Camera(std::string deviceName = "/dev/video0");

    ~Camera();

    [[nodiscard]] bool
    configure(const CameraConfig& config);

    [[nodiscard]] bool
    start();

    void
    stop();

    OnFrameReadySig
    onFrameReady();

private:
    [[nodiscard]] bool
    deviceOpened() const;

    [[nodiscard]] bool
    openDevice();

    void
    closeDevice();

    [[nodiscard]] bool
    configureDevice(const CameraConfig& config);

    [[nodiscard]] bool
    requestBuffers(unsigned int bufferCount);

    [[nodiscard]] bool
    enqueueBuffers() const;

    void
    releaseBuffers();

    [[nodiscard]] bool
    activateStream();

    void
    deactivateStream();

    void
    readFrame() const;

    void
    runWorker();

    void
    stopWorker();

    void
    handleWorker(const std::stop_token& token) const;

    void
    notifyFrameReady(const CapturedFrame& frame) const;

private:
    struct FrameBuffer {
        void* ptr{};
        unsigned int length{};
    };

private:
    std::string _deviceName;
    int _fd{kInvalidFd};
    std::vector<FrameBuffer> _buffers;
    std::optional<CameraConfig> _config;
    std::jthread _worker;
    OnFrameReadySig _frameReadySig;
};

} // namespace jar
