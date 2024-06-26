#include "Encoder.hpp"

#include "Logger.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace jar {

class Encoder::Impl {
public:
    Impl() = default;

    ~Impl()
    {
        cleanup();
    }

    bool
    configure(const EncoderConfig& config)
    {
        LOGI("Encoder config: codec<{}>, width<{}>, height<{}>, fps<{}>, preset<{}>, tune<{}>, "
             "bitrate<{}>, bFrames<{}>, gopSize<{}>",
             config.codec,
             config.width,
             config.height,
             config.fps,
             config.preset,
             config.tune,
             config.bitrate,
             config.bFrames,
             config.gopSize);

        av_log_set_level(AV_LOG_QUIET);

        _codec = avcodec_find_encoder_by_name(config.codec.data());
        if (not _codec) {
            LOGE("Unable to find encoder with <{}> given name", config.codec);
            return false;
        }

        _packet = av_packet_alloc();
        if (not _packet) {
            LOGE("Unable to allocate packet");
            return false;
        }

        _ctx = avcodec_alloc_context3(_codec);
        if (not _ctx) {
            cleanup();
            LOGE("Unable to allocate codec context");
            return false;
        }
        _ctx->width = static_cast<int>(config.width);
        _ctx->height = static_cast<int>(config.height);
        _ctx->time_base = {1, static_cast<int>(config.fps)};
        _ctx->framerate = {static_cast<int>(config.fps), 1};
        _ctx->pix_fmt = AV_PIX_FMT_YUV420P;

        if (config.bitrate) {
            _ctx->bit_rate = static_cast<int>(*config.bitrate);
        }
        if (config.gopSize) {
            _ctx->gop_size = static_cast<int>(*config.gopSize);
        }
        if (config.bFrames) {
            _ctx->max_b_frames = static_cast<int>(*config.bFrames);
        }
        if (_codec->id == AV_CODEC_ID_H264 or _codec->id == AV_CODEC_ID_H265) {
            if (config.preset) {
                av_opt_set(_ctx->priv_data, "preset", config.preset->data(), 0);
            }
            if (config.tune) {
                av_opt_set(_ctx->priv_data, "tune", config.tune->data(), 0);
            }
            if (config.crf) {
                av_opt_set_int(_ctx->priv_data, "crf", *config.crf, 0);
            }
        }

        if (const int rv = avcodec_open2(_ctx, _codec, nullptr); rv < 0) {
            cleanup();
            LOGE("Unable to open encoder: {}", av_err2str(rv));
            return false;
        }

        return true;
    }

    void
    start()
    {
        _worker = std::jthread{&Impl::handleWorker, this};
    }

    void
    stop()
    {
        _worker.request_stop();
        if (_worker.joinable()) {
            _worker.join();
        }
    }

    void
    encode(const unsigned int sequence, void* data, const unsigned int size)
    {
        if (auto frame = createFrame(sequence, data, size); frame) {
            enqueueFrame(std::move(frame));
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

    OnPacketReadySig
    onPacketReady()
    {
        return _packetReadySig;
    }

private:
    using FramePtr = std::shared_ptr<AVFrame>;

    void
    cleanup()
    {
        if (_packet) {
            av_packet_free(&_packet);
        }
        if (_ctx) {
            avcodec_free_context(&_ctx);
        }
    }

    [[nodiscard]] FramePtr
    createFrame(const unsigned int sequence, void* data, const unsigned int /*size*/) const
    {
        auto frame = std::shared_ptr<AVFrame>(av_frame_alloc(),
                                              [](AVFrame* self) { av_frame_free(&self); });
        if (not frame) {
            LOGE("Unable to allocate frame");
            return {};
        }

        frame->format = _ctx->pix_fmt;
        const int width{_ctx->width};
        frame->width = width;
        const int height{_ctx->height};
        frame->height = height;
        frame->pts = sequence;

        if (const int rv = av_frame_get_buffer(frame.get(), 0); rv < 0) {
            LOGE("Unable to allocate frame buffer: {}", av_err2str(rv));
            return {};
        }

        av_frame_make_writable(frame.get());

        int offset{0}, bytes = width * height;
        memcpy(frame->data[0], data, bytes);
        offset += bytes, bytes /= 4;
        memcpy(frame->data[1], static_cast<uint8_t*>(data) + offset, bytes);
        offset += bytes;
        memcpy(frame->data[2], static_cast<uint8_t*>(data) + offset, bytes);

        return frame;
    }

    void
    enqueueFrame(FramePtr frame)
    {
        {
            std::scoped_lock lock{_guard};
            _queue.push(std::move(frame));
        }
        _whenNotEmpty.notify_one();
    }

    FramePtr
    dequeueFrame()
    {
        std::unique_lock lock{_guard};
        const bool ok = _whenNotEmpty.wait(
            lock, _worker.get_stop_token(), [this] { return not _queue.empty(); });
        FramePtr output;
        if (ok) {
            output = _queue.front();
            _queue.pop();
        }
        return output;
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
            if (rv >= 0) {
                notifyPacketReady({
                    .data = _packet->data,
                    .size = _packet->size,
                });
            } else {
                LOGE("Error during encoding: {}", av_err2str(rv));
            }
            av_packet_unref(_packet);
        }
        while (rv >= 0);
    }

    void
    notifyPacketReady(const EncodedPacket& packet) const
    {
        _packetReadySig(packet);
    }

    void
    handleWorker(const std::stop_token& token)
    {
        while (not token.stop_requested()) {
            if (auto frame = dequeueFrame(); frame) {
                if (sendFrame(frame.get())) {
                    recvPackets();
                } else {
                    LOGE("Unable to send frame");
                }
            }
        }
    }

private:
    const AVCodec* _codec{};
    AVPacket* _packet{};
    AVCodecContext* _ctx{};

    std::queue<FramePtr> _queue;
    std::mutex _guard;
    std::condition_variable_any _whenNotEmpty;
    std::jthread _worker;

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
Encoder::start() const
{
    assert(_impl);
    _impl->start();
}

void
Encoder::stop() const
{
    assert(_impl);
    _impl->stop();
}

void
Encoder::encode(unsigned int sequence, void* data, unsigned int size) const
{
    assert(_impl);
    _impl->encode(sequence, data, size);
}

void
Encoder::finalize() const
{
    assert(_impl);
    _impl->finalize();
}

Encoder::OnPacketReadySig
Encoder::onPacketReady() const
{
    assert(_impl);
    return _impl->onPacketReady();
}

} // namespace jar
