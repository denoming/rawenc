#include <boost/asio.hpp>

#include "Camera.hpp"
#include "Encoder.hpp"
#include "Logger.hpp"
#include "LoggerInitializer.hpp"

namespace asio = boost::asio;

using namespace jar;

static unsigned int kDefaultWidth = 640;
static unsigned int kDefaultHeight = 480;

namespace {

bool
waitForTermination()
{
    asio::io_context context;
    asio::signal_set signals(context, SIGINT, SIGTERM);
    signals.async_wait([&context](const auto& /*error*/, int signal) {
        if (not context.stopped()) {
            context.stop();
        }
    });
    return (context.run() > 0);
}

bool
setupEncoder(const Encoder& encoder)
{
    const EncoderConfig encoderConfig{
        .codec = "libx264",
        .bitrate = 400000,
        .width = static_cast<int>(kDefaultWidth),
        .height = static_cast<int>(kDefaultHeight),
        .fps = 30,
        .gopSize = 10,
        .bFrames = 0,
    };

    if (not encoder.configure(encoderConfig)) {
        LOGE("Unable to configure encoder");
        return false;
    }

    encoder.onPacketReady().connect([](const EncodedPacket& packet) {
        fwrite(packet.data, 1, packet.size, stdout);
        fflush(stdout);
    });

    return true;
}

bool
setupCamera(Camera& camera, const Encoder& encoder)
{
    const CameraConfig cameraConfig{
        .width = kDefaultWidth,
        .height = kDefaultHeight,
        .bufferCount = 8,
    };

    if (not camera.configure(cameraConfig)) {
        LOGE("Unable to configure camera");
        return false;
    }

    camera.onFrameReady().connect([&](const CapturedFrame& frame) {
        encoder.encode(frame.sequence, frame.data, frame.size);
    });

    return true;
}

bool
captureFrames(Camera& camera)
{
    if (not camera.start()) {
        LOGE("Unable to start camera");
        return false;
    }
    waitForTermination();
    camera.stop();
    return true;
}

} // namespace

int
main()
{
    LoggerInitializer::instance().initialize("rawenc.log");

    const Encoder encoder;
    if (not setupEncoder(encoder)) {
        LOGE("Unable to setup encoder");
        return EXIT_FAILURE;
    }

    Camera camera;
    if (not setupCamera(camera, encoder)) {
        LOGE("Unable to setup camera");
        return EXIT_FAILURE;
    }

    return captureFrames(camera) ? EXIT_SUCCESS : EXIT_FAILURE;
}
