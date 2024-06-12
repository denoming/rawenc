#pragma once

#include <sigc++/signal.h>

#include <memory>

namespace jar {

struct EncoderConfig {
    /* The name of encoder to pass to FFMPEG (H.264 - libx264, H.265 - libx265) */
    std::string codec{"libx264"};
    /* The average bitrate value */
    unsigned bitrate{400000};
    /* The width of the frame (must be a multiple of two) */
    unsigned width{640};
    /* The height of the frame (must be a multiple of two) */
    unsigned height{480};
    /* The frame rate of incoming stream */
    unsigned fps{30};
    /* The number of pictures in a group of pictures, or 0 for intra_only */
    unsigned gopSize{10};
    /* The maximum number of B-frames (the output will be delayed by bFrame+1 relative to input) */
    unsigned bFrames{1};
};

struct EncodedPacket {
    /* The payload fo encoded packet */
    uint8_t* data{};
    /* The size of payload */
    int size{};
};

class Encoder {
public:
    using OnPacketReadySig = sigc::signal<void(const EncodedPacket& packet)>;

    Encoder();

    ~Encoder();

    [[nodiscard]] bool
    configure(const EncoderConfig& config) const;

    void
    start() const;

    void
    stop() const;

    void
    encode(unsigned int sequence, void* data, unsigned int size) const;

    void
    finalize() const;

    [[nodiscard]] OnPacketReadySig
    onPacketReady() const;

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace jar
