#include "Camera.hpp"

#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
extern "C" {
#include <linux/videodev2.h>
}
#include <libv4l2.h>

#include "Logger.hpp"

namespace {

int
xioctl(int fd, int request, void* arg)
{
    int r;
    do {
        r = v4l2_ioctl(fd, request, arg);
    }
    while (-1 == r && EINTR == errno);
    return r;
}

} // namespace

Camera::Camera(std::string deviceName)
    : _deviceName{std::move(deviceName)}
{
}

Camera::~Camera()
{
    if (deviceOpened()) {
        closeDevice();
    }
}

bool
Camera::configure(const CameraConfig& config)
{
    if (not openDevice()) {
        LOGE("Unable to open <{}> device", _deviceName);
        return false;
    }

    if (not configureDevice(config)) {
        LOGE("Unable to configure <{}> deivce", _deviceName);
        closeDevice();
        return false;
    }

    return true;
}

bool
Camera::start()
{
    if (not _config) {
        LOGE("Configure camera properly first");
        return false;
    }

    if (not requestBuffers(_config->bufferCount)) {
        LOGE("Unable to request <{}> buffers", _config->bufferCount);
        return false;
    }

    if (not enqueueBuffers()) {
        LOGE("Unable to enqueue <{}> buffers", _config->bufferCount);
        return false;
    }

    if (not activateStream()) {
        return false;
    }

    return true;
}

void
Camera::stop()
{
    deactivateStream();
    releaseBuffers();
}

Camera::OnFrameReadySig
Camera::onFrameReady()
{
    return _frameReadySig;
}

bool
Camera::deviceOpened() const
{
    return (_fd != kInvalidFd);
}

bool
Camera::openDevice()
{
    struct stat st = {};
    if (stat(_deviceName.data(), &st) == -1) {
        LOGE("Cannot identify {}: {}, {}", _deviceName, errno, strerror(errno));
        return false;
    }

    if (not S_ISCHR(st.st_mode)) {
        LOGE("{} is no device", _deviceName);
        return false;
    }

    _fd = v4l2_open(_deviceName.data(), O_RDWR /* required */ | O_NONBLOCK, 0);
    if (_fd == kInvalidFd) {
        LOGE("Cannot open {}: {}, {}", _deviceName, errno, strerror(errno));
        return false;
    }

    return true;
}

void
Camera::closeDevice()
{
    std::ignore = v4l2_close(_fd);
    _fd = kInvalidFd;
}

bool
Camera::configureDevice(const CameraConfig& config)
{
    v4l2_capability cap = {};
    if (xioctl(_fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (errno == EINVAL) {
            LOGE("Device <{}> is not V4L2 device", _deviceName);
        } else {
            LOGE("Unable to query <{}> device caps", _deviceName);
        }
        return false;
    }

    if (not(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOGE("Device <{}> is not video capture device", _deviceName);
        return false;
    }

    if (not(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOGE("Device <{}> doesn't support streaming I/O", _deviceName);
        return false;
    }

    v4l2_cropcap cropcap = {};
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_crop crop = {};
    if (xioctl(_fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        std::ignore = xioctl(_fd, VIDIOC_S_CROP, &crop);
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config.width;
    fmt.fmt.pix.height = config.height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (xioctl(_fd, VIDIOC_S_FMT, &fmt) == 0) {
        _config = CameraConfig{
            .width = fmt.fmt.pix.width,
            .height = fmt.fmt.pix.height,
            .bufferCount = config.bufferCount,
        };
    } else {
        LOGE("Unable to set format for <{}> device", _deviceName);
        return false;
    }

    return true;
}

bool
Camera::requestBuffers(const unsigned int bufferCount)
{
    v4l2_requestbuffers req{};
    req.count = bufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(_fd, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            LOGE("Device <{}> doesn't support memory mapping");
        } else {
            LOGE("Unable to request <{}> buffers", bufferCount);
        }
        return false;
    }

    if (req.count < 2) {
        LOGE("Too few buffer memory");
        return false;
    }

    _buffers.resize(bufferCount);

    for (size_t n = 0; n < req.count; ++n) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = n;

        if (xioctl(_fd, VIDIOC_QUERYBUF, &buffer) == -1) {
            LOGE("Unable to query status of <{}> buffer", n);
            return false;
        }

        constexpr int prots = PROT_READ | PROT_WRITE; /* Requered */
        constexpr int flags = MAP_SHARED;             /* Recommended */
        void* const ptr = v4l2_mmap(nullptr, buffer.length, prots, flags, _fd, buffer.m.offset);
        if (ptr == MAP_FAILED) {
            LOGE("Unable to map memory of <{}> buffer", n);
            return false;
        }

        _buffers[n].size = buffer.length;
        _buffers[n].data = ptr;
    }

    return true;
}

void
Camera::releaseBuffers()
{
    for (const auto [data, size] : _buffers) {
        v4l2_munmap(data, size);
    }
    _buffers.clear();
}

bool
Camera::enqueueBuffers() const
{
    for (unsigned n = 0; n < _buffers.size(); ++n) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = n;
        if (xioctl(_fd, VIDIOC_QBUF, &buffer) == -1) {
            LOGE("Unable to enqueue <{}> out of <{}>", n + 1, _buffers.size());
            return false;
        }
    }
    return true;
}

bool
Camera::activateStream()
{
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(_fd, VIDIOC_STREAMON, &type) == -1) {
        LOGE("Unable to set stream to on");
        return false;
    }

    _worker = std::jthread{&Camera::handleWorker, this};

    return true;
}

void
Camera::deactivateStream()
{
    if (_worker.request_stop()) {
        if (_worker.joinable()) {
            _worker.join();
        }
    } else {
        LOGE("Unable to stop worker");
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(_fd, VIDIOC_STREAMOFF, &type) == -1) {
        LOGE("Unable to set stream to off");
    }
}

void
Camera::readFrame() const
{
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (xioctl(_fd, VIDIOC_DQBUF, &buffer) == -1) {
        LOGE("Unable to dequeue buffer: {}, {}", errno, strerror(errno));
        return;
    }

    notifyFrameReady(_buffers[buffer.index]);

    if (xioctl(_fd, VIDIOC_QBUF, &buffer) == -1) {
        LOGE("Unable to enqueue buffer: {}, {}", errno, strerror(errno));
    }
}

void
Camera::handleWorker(const std::stop_token& token) const
{
    while (not token.stop_requested()) {
        pollfd p = {_fd, POLLIN, 0};
        if (const int rv = ::poll(&p, 1, 200); rv == -1) {
            if (errno == EINTR) {
                continue;
            }
        }
        if (p.revents & POLLIN) {
            readFrame();
        }
    }
}

void
Camera::notifyFrameReady(const FrameBuffer& buffer) const
{
    _frameReadySig(buffer);
}
