#include <boost/asio.hpp>

#include "Camera.hpp"
#include "Encoder.hpp"
#include "Logger.hpp"
#include "LoggerInitializer.hpp"

namespace asio = boost::asio;

static unsigned int kDefaultWidth = 640;
static unsigned int kDefaultHeight = 480;

namespace jar {

class Application {
public:
    [[nodiscard]] bool
    run()
    {
        if (not setupEncoder()) {
            LOGE("Unable to setup encoder");
            return false;
        }
        if (not setupCamera()) {
            LOGE("Unable to setup camera");
            return false;
        }

        _encoder.start();
        if (not _camera.start()) {
            LOGE("Unable to start camera");
            return false;
        }

        waitForTermination();

        _camera.stop();
        _encoder.stop();
        _encoder.finalize();

        return true;
    }

private:
    [[maybe_unused]] bool
    waitForTermination()
    {
        asio::signal_set signals(_context, SIGINT, SIGTERM);
        signals.async_wait([this](const auto& /*error*/, int signal) {
            if (not _context.stopped()) {
                _context.stop();
            }
        });
        return (_context.run() > 0);
    }

    [[nodiscard]] bool
    setupEncoder() const
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

        if (not _encoder.configure(encoderConfig)) {
            LOGE("Unable to configure encoder");
            return false;
        }

        _encoder.onPacketReady().connect([](const EncodedPacket& packet) {
            fwrite(packet.data, 1, packet.size, stdout);
            fflush(stdout);
        });

        return true;
    }

    [[nodiscard]] bool
    setupCamera()
    {
        const CameraConfig cameraConfig{
            .width = kDefaultWidth,
            .height = kDefaultHeight,
            .bufferCount = 8,
        };

        if (not _camera.configure(cameraConfig)) {
            LOGE("Unable to configure camera");
            return false;
        }

        _camera.onFrameReady().connect([this](const CapturedFrame& frame) {
            _encoder.encode(frame.sequence, frame.data, frame.size);
        });

        return true;
    }

private:
    asio::io_context _context;
    Camera _camera;
    Encoder _encoder;
};

} // namespace jar

int
main()
{
    jar::LoggerInitializer::instance().initialize("rawenc.log");
    jar::Application app;
    return app.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
