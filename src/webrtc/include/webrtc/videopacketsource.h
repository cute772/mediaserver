
#ifndef WebRTC_VideoPacketSource_H
#define WebRTC_VideoPacketSource_H


#include "base/base.h"
//#include "base/packetsignal.h"

#ifdef HAVE_FFMPEG

#include "ff/packet.h"

#include "media/base/videocapturer.h"


namespace base {
namespace wrtc {


/// VideoPacketSource implements a simple `cricket::VideoCapturer` that
/// gets decoded remote video frames from a local media channel.
/// It's used as the remote video source's `VideoCapturer` so that the remote
/// video can be used as a `cricket::VideoCapturer` and in that way a remote
/// video stream can implement the `MediaStreamSourceInterface`.
class VideoPacketSource : public cricket::VideoCapturer
{
public:
    VideoPacketSource(int width, int height, int fps, uint32_t fourcc);
    VideoPacketSource(const cricket::VideoFormat& captureFormat);
    virtual ~VideoPacketSource();

    /// Set the source `av::VideoPacket` emitter.
    /// The pointer is not managed by this class.
    //arvind
//    void setPacketSource(PacketSignal* source);

    /// Callback that fired when an `av::PlanarVideoPacket`
    /// is ready for processing.
    void onVideoCaptured(IPacket& pac);

    /// cricket::VideoCapturer implementation.
    virtual cricket::CaptureState Start(const cricket::VideoFormat& capture_format) override;
    virtual void Stop() override;
    virtual bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;
    virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                      cricket::VideoFormat* best_format) override;
    virtual bool IsRunning() override;
    virtual bool IsScreencast() const override;

protected:
    cricket::VideoFormat _captureFormat;
    webrtc::VideoRotation _rotation;
    int64_t _timestampOffset;
    int64_t _nextTimestamp;
   // std::function<void(ff::PlanarVideoPacket& packet)>   _source;
};


// class VideoPacketSourceFactory : public cricket::VideoDeviceCapturerFactory {
// public:
//     VideoPacketSourceFactory() {}
//     virtual ~VideoPacketSourceFactory() {}
//
//     virtual cricket::VideoCapturer* Create(const cricket::Device& device) {
//         return new VideoPacketSource(device.name);
//     }
// };


} } // namespace :wrtc


#endif // HAVE_FFMPEG
#endif


