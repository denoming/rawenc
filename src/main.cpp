#include <boost/asio.hpp>

#include "Camera.hpp"
#include "Logger.hpp"

namespace asio = boost::asio;

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

} // namespace

int
main()
{
    const CameraConfig config{
        .width = 640,
        .height = 480,
        .bufferCount = 8,
    };

    Camera camera;
    if (not camera.configure(config)) {
        LOGE("Unable to configure camera");
    }

    camera.onFrameReady().connect([](const FrameBuffer& buffer) {
        LOGI("Frame is ready: addr<{}>, size<{}>", buffer.data, buffer.size);
    });

    if (not camera.start()) {
        LOGE("Unable to start camera");
    }

    if (not waitForTermination()) {
        LOGE("Unable to wait for termination");
        camera.stop();
        return EXIT_FAILURE;
    }

    camera.stop();

    return EXIT_SUCCESS;
}
