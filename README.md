# RawEnc

`RawEnc` is a simple application with command line interface. Created in order to show a simple example
of capturing raw video stream from typical camera sensor using V4L2 and encoding using FFMPEG framework.

Tested encoders:
 * libx264
 * libx265
 * libopenh264
 * h264_nvenc (optional)
 * hevc_nvenc (optional)

Note: The `h264_nvenc` and `hevc_nvenc` codecs are available only if Nvidia GPU
      present and cmake `RAWENC_ENABLE_NVCODEC` option is enabled.

## Pipelines

Streaming to window pipeline (H.264 by libx264):<br/>
```shell
$PWD/rawenc | gst-launch-1.0 -e fdsrc fd=0 ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink sync=false
```

Streaming to window (H.265 by libx265):<br/>
```shell
$PWD/rawenc | gst-launch-1.0 -e fdsrc fd=0 ! h265parse ! avdec_h265 ! videoconvert ! xvimagesink sync=false
```

Streaming to shared memory (H.264 by libx264):<br/>
```shell
# Producer pipeline
$ $PWD/rawenc | gst-launch-1.0 -e fdsrc fd=0 ! shmsink socket-path=/tmp/rawenc.socket
# Consumer pipeline
$ gst-launch-1.0 shmsrc socket-path=/tmp/rawenc.socket ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink sync=false
```

Streaming RAW video to shared memory using V4L2:<br/>
```shell
# Producer pipeline
$ gst-launch-1.0 v4l2src flags=capture io-mode=mmap ! \
 "video/x-raw, format=YUY2, width=(int)800, height=(int)600, framerate=(fraction)24/1" \
 ! shmsink socket-path=/tmp/rawenc.socket
# Consumer pipeline
$ gst-launch-1.0 shmsrc socket-path=/tmp/rawenc.socket ! \
 rawvideoparse format=GST_VIDEO_FORMAT_YUY2 framerate=24/1 width=800 height=600 ! \
 xvimagesink sync=false 
```

## Useful

* Shows available codec options:
```shell
$ ffmpeg -h encoder=libx264 --help
$ ffmpeg -h encoder=libx265 --help
$ ffmpeg -h encoder=h264_nvenc --help
$ ffmpeg -h encoder=hevc_nvenc --help
```

## Links

* [FFMPEG - H.264](https://trac.ffmpeg.org/wiki/Encode/H.264)
* [FFMPEG - H.265](https://trac.ffmpeg.org/wiki/Encode/H.265)
* [Encoder - x265](https://x265.readthedocs.io/en/master/index.html)
