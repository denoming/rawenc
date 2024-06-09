# RawEnc

## Name

TBD

## Description

Tested FFMPEG supported encoders:
 * libx264
 * libx265
 * libopenh264

Tested pipelines:
 * streaming to window (H.264 by libx264):<br/>
 `$PWD/rawenc | gst-launch-1.0 -e fdsrc fd=0 ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink sync=false`
 * streaming to window (H.265 by libx265):<br/>
 `$PWD/rawenc | gst-launch-1.0 -e fdsrc fd=0 ! h265parse ! avdec_h265 ! videoconvert ! xvimagesink sync=false`

## Links

* [FFMPEG - H.264](https://trac.ffmpeg.org/wiki/Encode/H.264)
* [FFMPEG - H.265](https://trac.ffmpeg.org/wiki/Encode/H.265)
* [Encoder - x265](https://x265.readthedocs.io/en/master/index.html)
