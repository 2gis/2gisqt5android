// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureDevice is the abstract base class for realizing video capture
// device support in Chromium. It provides the interface for OS dependent
// implementations.
// The class is created and functions are invoked on a thread owned by
// VideoCaptureManager. Capturing is done on other threads, depending on the OS
// specific implementation.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_

#include <list>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class MEDIA_EXPORT VideoCaptureDevice {
 public:
  // Represents a capture device name and ID.
  // You should not create an instance of this class directly by e.g. setting
  // various properties directly.  Instead use
  // VideoCaptureDevice::GetDeviceNames to do this for you and if you need to
  // cache your own copy of a name, you can do so via the copy constructor.
  // The reason for this is that a device name might contain platform specific
  // settings that are relevant only to the platform specific implementation of
  // VideoCaptureDevice::Create.
  class MEDIA_EXPORT Name {
   public:
    Name();
    Name(const std::string& name, const std::string& id);

#if defined(OS_WIN)
    // Windows targets Capture Api type: it can only be set on construction.
    enum CaptureApiType {
      MEDIA_FOUNDATION,
      DIRECT_SHOW,
      DIRECT_SHOW_WDM_CROSSBAR,
      API_TYPE_UNKNOWN
    };
#endif
#if defined(OS_MACOSX)
    // Mac targets Capture Api type: it can only be set on construction.
    enum CaptureApiType {
      AVFOUNDATION,
      QTKIT,
      DECKLINK,
      API_TYPE_UNKNOWN
    };
    // For AVFoundation Api, identify devices that are built-in or USB.
    enum TransportType {
      USB_OR_BUILT_IN,
      OTHER_TRANSPORT
    };
#endif
#if defined(OS_WIN) || defined(OS_MACOSX)
    Name(const std::string& name,
         const std::string& id,
         const CaptureApiType api_type);
#endif
#if defined(OS_MACOSX)
    Name(const std::string& name,
         const std::string& id,
         const CaptureApiType api_type,
         const TransportType transport_type);
#endif
    ~Name();

    // Friendly name of a device
    const std::string& name() const { return device_name_; }

    // Unique name of a device. Even if there are multiple devices with the same
    // friendly name connected to the computer this will be unique.
    const std::string& id() const { return unique_id_; }

    // The unique hardware model identifier of the capture device. Returns
    // "[vid]:[pid]" when a USB device is detected, otherwise "".
    // The implementation of this method is platform-dependent.
    const std::string GetModel() const;

    // Friendly name of a device, plus the model identifier in parentheses.
    const std::string GetNameAndModel() const;

    // These operators are needed due to storing the name in an STL container.
    // In the shared build, all methods from the STL container will be exported
    // so even though they're not used, they're still depended upon.
    bool operator==(const Name& other) const {
      return other.id() == unique_id_;
    }
    bool operator<(const Name& other) const {
      return unique_id_ < other.id();
    }

#if defined(OS_WIN) || defined(OS_MACOSX)
    CaptureApiType capture_api_type() const {
      return capture_api_class_.capture_api_type();
    }
    const char* GetCaptureApiTypeString() const;
#endif
#if defined(OS_WIN)
    // Certain devices need an ID different from the |unique_id_| for
    // capabilities retrieval.
    const std::string& capabilities_id() const {
      return capabilities_id_;
    }
    void set_capabilities_id(const std::string& id) {
      capabilities_id_ = id;
    }
#endif
#if defined(OS_MACOSX)
    TransportType transport_type() const {
      return transport_type_;
    }
    bool is_blacklisted() const {
      return is_blacklisted_;
    }
    void set_is_blacklisted(bool is_blacklisted) {
      is_blacklisted_ = is_blacklisted;
    }
#endif  // if defined(OS_WIN)

   private:
    std::string device_name_;
    std::string unique_id_;
#if defined(OS_WIN) || defined(OS_MACOSX)
    // This class wraps the CaptureApiType to give it a by default value if not
    // initialized.
    class CaptureApiClass {
     public:
      CaptureApiClass(): capture_api_type_(API_TYPE_UNKNOWN) {}
      CaptureApiClass(const CaptureApiType api_type)
          : capture_api_type_(api_type) {}
      CaptureApiType capture_api_type() const {
        DCHECK_NE(capture_api_type_, API_TYPE_UNKNOWN);
        return capture_api_type_;
      }
     private:
      CaptureApiType capture_api_type_;
    };

    CaptureApiClass capture_api_class_;
#endif
#if defined(OS_WIN)
    // ID used for capabilities retrieval. By default is equal to |unique_id|.
    std::string capabilities_id_;
#endif
#if defined(OS_MACOSX)
    TransportType transport_type_;
    // Flag used to mark blacklisted devices for QTKit Api.
    bool is_blacklisted_;
#endif
    // Allow generated copy constructor and assignment.
  };

  // Manages a list of Name entries.
  typedef std::list<Name> Names;

  class MEDIA_EXPORT Client {
   public:
    // Memory buffer returned by Client::ReserveOutputBuffer().
    class Buffer : public base::RefCountedThreadSafe<Buffer> {
     public:
      int id() const { return id_; }
      void* data() const { return data_; }
      size_t size() const { return size_; }

     protected:
      friend class base::RefCountedThreadSafe<Buffer>;

      Buffer(int id, void* data, size_t size)
          : id_(id), data_(data), size_(size) {}
      virtual ~Buffer() {}

      const int id_;
      void* const data_;
      const size_t size_;
    };

    virtual ~Client() {}

    // Reserve an output buffer into which contents can be captured directly.
    // The returned Buffer will always be allocated with a memory size suitable
    // for holding a packed video frame with pixels of |format| format, of
    // |dimensions| frame dimensions. It is permissible for |dimensions| to be
    // zero; in which case the returned Buffer does not guarantee memory
    // backing, but functions as a reservation for external input for the
    // purposes of buffer throttling.
    //
    // The output buffer stays reserved for use until the Buffer object is
    // destroyed.
    virtual scoped_refptr<Buffer> ReserveOutputBuffer(
        media::VideoFrame::Format format,
        const gfx::Size& dimensions) = 0;

    // Captured a new video frame, data for which is pointed to by |data|.
    //
    // The format of the frame is described by |frame_format|, and is assumed to
    // be tightly packed. This method will try to reserve an output buffer and
    // copy from |data| into the output buffer. If no output buffer is
    // available, the frame will be silently dropped.
    virtual void OnIncomingCapturedData(const uint8* data,
                                        int length,
                                        const VideoCaptureFormat& frame_format,
                                        int rotation,  // Clockwise.
                                        base::TimeTicks timestamp) = 0;

    // Captured a new video frame, held in |frame|.
    //
    // As the frame is backed by a reservation returned by
    // ReserveOutputBuffer(), delivery is guaranteed and will require no
    // additional copies in the browser process.
    virtual void OnIncomingCapturedVideoFrame(
        const scoped_refptr<Buffer>& buffer,
        const VideoCaptureFormat& buffer_format,
        const scoped_refptr<media::VideoFrame>& frame,
        base::TimeTicks timestamp) = 0;

    // An error has occurred that cannot be handled and VideoCaptureDevice must
    // be StopAndDeAllocate()-ed. |reason| is a text description of the error.
    virtual void OnError(const std::string& reason) = 0;

    // VideoCaptureDevice requests the |message| to be logged.
    virtual void OnLog(const std::string& message) {}

    // The video stream has been muted. After this callback, no more
    // OnIncomingCapturedData() will be called. This may happen when
    // CaptureImage() has called. After the still image captured, the client
    // will get notified by OnUnmute() and the video stream will be resumed.
    virtual void OnMute() {}

    // The video stream has resumed.
    virtual void OnUnmute() {}
  };

  // Interface for clients that use VideoCaptureDevice for taking still images.
  class MEDIA_EXPORT ImageClient {
   public:
    virtual ~ImageClient() {}

    // Callback function to notify the client a captured image is available.
    //
    // The captured still image is stored at address |data| and is of |length|
    // bytes. The format of the frame is described by |format|, and is assumed
    // to be tightly packed. The still image should be rotated |rotation|
    // degrees clockwise for viewing.
    //
    // Note that the content in |data| will not be valid after this callback
    // returns. Copy the content to use it later.
    virtual void OnIncomingCapturedData(const uint8* data,
                                        size_t length,
                                        const ImageCaptureFormat& format,
                                        int rotation,
                                        base::TimeTicks timestamp) = 0;

    // Callback function to notify the client about a failure of the image
    // capture. The VideoCaptureDevice must be StopAndDeAllocate()-ed.
    // |reason| contains a text description of the error.
    virtual void OnError(const std::string& reason) = 0;
  };

  virtual ~VideoCaptureDevice();

  // Prepares the camera for use. After this function has been called no other
  // applications can use the camera. StopAndDeAllocate() must be called before
  // the object is deleted.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                scoped_ptr<Client> client) = 0;

  // Deallocates the camera, possibly asynchronously.
  //
  // This call requires the device to do the following things, eventually: put
  // camera hardware into a state where other applications could use it, free
  // the memory associated with capture, and delete the |client| pointer passed
  // into AllocateAndStart.
  //
  // If deallocation is done asynchronously, then the device implementation must
  // ensure that a subsequent AllocateAndStart() operation targeting the same ID
  // would be sequenced through the same task runner, so that deallocation
  // happens first.
  virtual void StopAndDeAllocate() = 0;

  // Gets the power line frequency from the current system time zone if this is
  // defined, otherwise returns 0.
  int GetPowerLineFrequencyForLocation() const;

  // Initializes the device for still image capture for the given image format.
  // This call is synchronous and returns true iff the initialization is
  // successful.
  //
  // This function must be called between AllocateAndStart() and
  // StopAndDeAllocate().
  virtual bool InitializeImageCapture(const ImageCaptureFormat& image_format,
                                      scoped_ptr<ImageClient> client);

  // Releases resources for image capture.
  //
  // The ImageClient passed from InitializeImageCapture will be freed. This
  // method must be called between InitializeImageCapture() and
  // StopAndDeAllocate().
  virtual void ReleaseImageCapture() {}

  // Requests one image from the device.
  //
  // The image will be returned via the ImageClient::OnIncomingCapturedData()
  // callback. If the video stream has to be stopped to capture the still image,
  // the Client::OnMute() and Client::OnUnmute() will be called.
  //
  // This function must be called between InitializeImageCapture() and
  // ReleaseImageCapture().
  virtual void CaptureImage() {}

 protected:
  static const int kPowerLine50Hz = 50;
  static const int kPowerLine60Hz = 60;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
