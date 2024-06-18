#pragma once

#include <sigc++/signal.h>

#include <memory>
#include <optional>

namespace jar {

struct EncoderConfig {
    /* The name of encoder to pass to FFMPEG (H.264 - libx264, H.265 - libx265) */
    std::string codec{"libx264"};
    /* The width of the frame (must be a multiple of two) */
    unsigned width{640};
    /* The height of the frame (must be a multiple of two) */
    unsigned height{480};
    /* The frame rate of incoming stream */
    unsigned fps{30};
    /* The preset to use while encoding */
    std::optional<std::string> preset;
    /* The tune to apply after applying preset  */
    std::optional<std::string> tune;
    /* The CRF (Constant Rate Factor) */
    std::optional<unsigned> crf;
    /* The average bitrate value */
    std::optional<unsigned> bitrate;
    /* The number of pictures in a group of pictures, or 0 for intra_only */
    std::optional<unsigned> gopSize;
    /* The maximum number of B-frames (the output will be delayed by bFrame+1 relative to input) */
    std::optional<unsigned> bFrames;
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
