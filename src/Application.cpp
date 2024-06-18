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
            ("width", po::value<unsigned>()->notifier([this](const unsigned v) {
                _cameraConfig.width = _encoderConfig.width = v;
            })->default_value(kDefaultWidth), "Set width")
            ("height", po::value<unsigned>()->notifier([this](const unsigned v) {
                _cameraConfig.height = _encoderConfig.height = v;
            })->default_value(kDefaultHeight), "Set height")
            ("codec", po::value<std::string>()->notifier([this](const std::string& v) {
                _encoderConfig.codec = v;
            })->default_value(kDefaultCodec), "Set encoder codec")
            ("fps", po::value<unsigned>()->notifier([this](const unsigned fps) {
                _encoderConfig.fps = fps;
            })->default_value(kDefaultFps), "Set encoder FPS")
            ("preset", po::value<std::string>()->notifier([this](const std::string& v) {
                _encoderConfig.preset = v;
            }), "Choose encoder preset")
            ("tune", po::value<std::string>()->notifier([this](const std::string& v) {
                _encoderConfig.tune = v;
            }), "Choose encoder tune")
            ("bitrate", po::value<unsigned>()->notifier([this](const unsigned v) {
                _encoderConfig.bitrate = v;
            }), "Set encoder bitrate")
            ("crf", po::value<unsigned>()->notifier([this](const unsigned v) {
                _encoderConfig.crf = v;
            }), "Set CRF (Constant Rate Factor) value")
            ("gop-size", po::value<unsigned>()->notifier([this](const unsigned v) {
                _encoderConfig.gopSize = v;
            }), "Set encoder GOP size")
            ("b-frames", po::value<unsigned>()->notifier([this](const unsigned v) {
               _encoderConfig.bFrames = v;
            }), "Set encoder b-frames count")
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
        if (not _encoder.configure(_encoderConfig)) {
            LOGE("Unable to configure encoder");
            return false;
        }

        _encoder.onPacketReady().connect([](const EncodedPacket& packet) {
            LOGT("Packet: data<{}>, size<{}>", fmt::ptr(packet.data), packet.size);
            fwrite(packet.data, 1, packet.size, stdout);
            fflush(stdout);
        });

        return true;
    }

    [[nodiscard]] bool
    setupCamera()
    {
        if (not _camera.configure(_cameraConfig)) {
            LOGE("Unable to configure camera");
            return false;
        }

        _camera.onFrameReady().connect([this](const CapturedFrame& frame) {
            LOGT("Frame: index<{}>, data<{}>, size<{}>",
                 frame.sequence,
                 fmt::ptr(frame.data),
                 frame.size);
            _encoder.encode(frame.sequence, frame.data, frame.size);
        });

        return true;
    }

private:
    asio::io_context _context;
    Camera _camera;
    CameraConfig _cameraConfig;
    Encoder _encoder;
    EncoderConfig _encoderConfig;
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
