#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "Camera.hpp"
#include "Encoder.hpp"
#include "Logger.hpp"
#include "LoggerInitializer.hpp"

#include <iostream>

namespace asio = boost::asio;
namespace po = boost::program_options;

/* General defaults */
static unsigned kDefaultWidth = 640;
static unsigned kDefaultHeight = 480;

/* Encoder specific defaults */
static const char* kDefaultCodec{"libx264"};
static const char* kDefaultPreset{"fast"};
static const char* kDefaultTune{"zerolatency"};
static unsigned kDefaultBitrate = 400000;
static unsigned kDefaultFps = 30;
static unsigned kDefaultGopSize = 10;
static unsigned kDefaultBFrames = 0;

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
            ("width", po::value<unsigned>(&_width)
                ->default_value(kDefaultWidth), "Set width")
            ("height", po::value<unsigned>(&_height)
                ->default_value(kDefaultHeight), "Set height")
            ("codec", po::value<std::string>(&_codec)
                ->default_value(kDefaultCodec), "Set encoder codec")
            ("preset", po::value<std::string>(&_preset)
                ->default_value(kDefaultPreset), "Set encoder preset")
            ("tune", po::value<std::string>(&_tune)
                ->default_value(kDefaultTune), "Set encoder tune")
            ("bitrate", po::value<unsigned>(&_bitrate)
                ->default_value(kDefaultBitrate), "Set encoder bitrate")
            ("fps", po::value<unsigned>(&_fps)
                ->default_value(kDefaultFps), "Set encoder FPS")
            ("gop-size", po::value<unsigned>(&_gopSize)
                ->default_value(kDefaultGopSize), "Set encoder GOP size")
            ("b-frames", po::value<unsigned>(&_bFrames)
                ->default_value(kDefaultBFrames), "Set encoder b-frames count")
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
            .preset = _preset,
            .tune = _tune,
            .bitrate = _bitrate,
            .width = _width,
            .height = _height,
            .fps = _fps,
            .gopSize = _gopSize,
            .bFrames = _bFrames,
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
    unsigned _width{kDefaultWidth};
    unsigned _height{kDefaultHeight};
    std::string _codec{kDefaultCodec};
    std::string _preset{kDefaultPreset};
    std::string _tune{kDefaultTune};
    unsigned _bitrate{kDefaultBitrate};
    unsigned _fps{kDefaultFps};
    unsigned _gopSize{kDefaultGopSize};
    unsigned _bFrames{kDefaultBFrames};
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
