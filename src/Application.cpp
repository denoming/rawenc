#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "Camera.hpp"
#include "Encoder.hpp"
#include "Logger.hpp"
#include "LoggerInitializer.hpp"

#include <iostream>

namespace asio = boost::asio;
namespace po = boost::program_options;

static const char* kDefaultCodec{"libx264"};
static unsigned int kDefaultWidth = 640;
static unsigned int kDefaultHeight = 480;

namespace jar {

class Application {
public:
    [[nodiscard]] bool
    parseArgs(const int argc, char* argv[])
    {
        po::options_description d{"RawEnc CLI"};

        // clang-format off
        d.add_options()
            ("help,h", "Display help")
            ("codec", po::value<std::string>(&_codec), "Set codec (e.g. 'libx264')")
            ("width", po::value<unsigned int>(&_width), "Set width")
            ("height", po::value<unsigned int>(&_height), "Set height")
        ;
        // clang-format on

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, d), vm);
        po::notify(vm);

        if (vm.contains("help")) {
            std::cout << d << std::endl;
            return false;
        }

        return true;
    }

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
            .codec = _codec,
            .bitrate = 400000,
            .width = static_cast<int>(_width),
            .height = static_cast<int>(_height),
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
            .width = _width,
            .height = _height,
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
    std::string _codec{kDefaultCodec};
    unsigned int _width{kDefaultWidth};
    unsigned int _height{kDefaultHeight};
    Camera _camera;
    Encoder _encoder;
};

} // namespace jar

int
main(int argc, char* argv[])
{
    jar::LoggerInitializer::instance().initialize("rawenc.log");
    jar::Application app;
    if (not app.parseArgs(argc, argv)) {
        /* Show help menu and exit */
        return EXIT_SUCCESS;
    }
    return app.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
