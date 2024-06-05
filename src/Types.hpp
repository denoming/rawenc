#pragma once

struct CameraConfig {
    unsigned int width{};
    unsigned int height{};
    unsigned int bufferCount{};
};

struct FrameBuffer {
    void* data{};
    unsigned int size{};
};
