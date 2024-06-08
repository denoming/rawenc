#include "Encoder.hpp"

#include "Logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

namespace jar {

class Encoder::Impl {
public:
    Impl() = default;

    ~Impl()
    {
        finalize();
        cleanup();
    }

    bool
    configure(const EncoderConfig& config)
    {
        av_log_set_level(AV_LOG_QUIET);

        _codec = avcodec_find_encoder_by_name(config.codec.data());
        if (not _codec) {
            LOGE("Unable to find encoder with <{}> given name", config.codec);
            return false;
        }

        _ctx = avcodec_alloc_context3(_codec);
        if (not _ctx) {
            cleanup();
            LOGE("Unable to allocate codec context");
            return false;
        }

        _ctx->bit_rate = config.bitrate;
        _ctx->width = config.width;
        _ctx->height = config.height;
        _ctx->time_base = {1, config.fps};
        _ctx->framerate = {config.fps, 1};
        if (_codec->id != AV_CODEC_ID_HEVC) {
            _ctx->max_b_frames = config.bFrames;
        }
        _ctx->gop_size = config.gopSize;
        _ctx->pix_fmt = AV_PIX_FMT_YUV420P;

        if (_codec->id == AV_CODEC_ID_H264) {
            av_opt_set(_ctx->priv_data, "preset", "fast", 0);
            av_opt_set(_ctx->priv_data, "tune", "zerolatency", 0);
        }
        if (_codec->id == AV_CODEC_ID_H265) {
            av_opt_set(_ctx->priv_data, "preset", "fast", 0);
            av_opt_set(_ctx->priv_data, "tune", "zerolatency", 0);
        }

        if (const int rv = avcodec_open2(_ctx, _codec, nullptr); rv < 0) {
            cleanup();
            LOGE("Unable to open encoder: {}", av_err2str(rv));
            return false;
        }

        _packet = av_packet_alloc();
        if (not _packet) {
            cleanup();
            LOGE("Unable to allocate packet");
            return false;
        }

        if (_frame = av_frame_alloc(); _frame) {
            _frame->format = _ctx->pix_fmt;
            _frame->width = _ctx->width;
            _frame->height = _ctx->height;
        } else {
            cleanup();
            LOGE("Unable to allocate frame");
            return false;
        }

        if (const int rv = av_frame_get_buffer(_frame, 0); rv < 0) {
            cleanup();
            LOGE("Unable to allocate buffer for frame: {}", av_err2str(rv));
            return false;
        }

        return true;
    }

    void
    encode(const unsigned int sequence, void* data, const unsigned int size) const
    {
        if (sendFrame(fillFrame(sequence, data, size))) {
            recvPackets();
        } else {
            LOGE("Unable to send <{}> frame to encode", sequence);
        }
    }

    void
    finalize() const
    {
        if (sendFrame(nullptr)) {
            recvPackets();
        } else {
            LOGE("Unable to finalize");
        }
    }

    void
    cleanup()
    {
        if (_packet) {
            av_packet_free(&_packet);
        }
        if (_frame) {
            av_frame_free(&_frame);
        }
        if (_ctx) {
            avcodec_free_context(&_ctx);
        }
    }

    OnPacketReadySig
    onPacketReady()
    {
        return _packetReadySig;
    }

private:
    AVFrame*
    fillFrame(const unsigned int sequence, void* data, unsigned int /*size*/) const
    {
        av_frame_make_writable(_frame);

        _frame->pts = sequence;

        int offset{0}, bytes = 640 * 480;
        memcpy(_frame->data[0], data, bytes);
        offset += bytes;
        bytes = 320 * 240;
        memcpy(_frame->data[1], static_cast<uint8_t*>(data) + offset, bytes);
        offset += bytes;
        memcpy(_frame->data[2], static_cast<uint8_t*>(data) + offset, bytes);

        return _frame;
    }

    [[nodiscard]] bool
    sendFrame(const AVFrame* frame) const
    {
        if (const int rv = avcodec_send_frame(_ctx, frame); rv < 0) {
            LOGE("Error on sending frame to encode: {}", av_err2str(rv));
            return false;
        }
        return true;
    }

    void
    recvPackets() const
    {
        int rv{};
        do {
            rv = avcodec_receive_packet(_ctx, _packet);
            if (rv == AVERROR(EAGAIN) or rv == AVERROR_EOF) {
                break;
            }
            if (rv < 0) {
                LOGE("Error during encoding: {}", av_err2str(rv));
                break;
            }

            notifyPacketReady({
                .data = _packet->data,
                .size = _packet->size,
            });

            av_packet_unref(_packet);
        }
        while (rv >= 0);
    }

    void
    notifyPacketReady(const EncodedPacket& packet) const
    {
        _packetReadySig(packet);
    }

private:
    const AVCodec* _codec{};
    AVCodecContext* _ctx{};
    AVPacket* _packet{};
    AVFrame* _frame{};
    OnPacketReadySig _packetReadySig;
};

Encoder::Encoder()
    : _impl{std::make_unique<Impl>()}
{
}

Encoder::~Encoder() = default;

bool
Encoder::configure(const EncoderConfig& config) const
{
    assert(_impl);
    return _impl->configure(config);
}

void
Encoder::encode(unsigned int sequence, void* data, unsigned int size) const
{
    assert(_impl);
    _impl->encode(sequence, data, size);
}

Encoder::OnPacketReadySig
Encoder::onPacketReady() const
{
    assert(_impl);
    return _impl->onPacketReady();
}

} // namespace jar
