// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_KEYS_H_
#define MEDIA_BASE_MEDIA_KEYS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace media {

class Decryptor;

template <typename... T>
class CdmPromiseTemplate;

typedef CdmPromiseTemplate<std::string> NewSessionCdmPromise;
typedef CdmPromiseTemplate<> SimpleCdmPromise;
typedef std::vector<std::vector<uint8> > KeyIdsVector;
typedef CdmPromiseTemplate<KeyIdsVector> KeyIdsPromise;

// Performs media key operations.
//
// All key operations are called on the renderer thread. Therefore, these calls
// should be fast and nonblocking; key events should be fired asynchronously.
class MEDIA_EXPORT MediaKeys {
 public:
  // Reported to UMA, so never reuse a value!
  // Must be kept in sync with blink::WebMediaPlayerClient::MediaKeyErrorCode
  // (enforced in webmediaplayer_impl.cc).
  // TODO(jrummell): Can this be moved to proxy_decryptor as it should only be
  // used by the prefixed EME code?
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    // The commented v0.1b values below have never been used.
    // kServiceError,
    kOutputError = 4,
    // kHardwareChangeError,
    // kDomainError,
    kMaxKeyError  // Must be last and greater than any legit value.
  };

  // Must be a superset of cdm::MediaKeyException.
  enum Exception {
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    INVALID_ACCESS_ERROR,
    QUOTA_EXCEEDED_ERROR,
    UNKNOWN_ERROR,
    CLIENT_ERROR,
    OUTPUT_ERROR
  };

  // Type of license required when creating/loading a session.
  // Must be consistent with the values specified in the spec:
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#extensions
  enum SessionType {
    TEMPORARY_SESSION,
    PERSISTENT_SESSION
  };

  static const uint32 kInvalidSessionId = 0;
#if defined(ENABLE_BROWSER_CDMS)
  static const int kInvalidCdmId = 0;
#endif

  MediaKeys();
  virtual ~MediaKeys();

  // Provides a server certificate to be used to encrypt messages to the
  // license server.
  virtual void SetServerCertificate(const uint8* certificate_data,
                                    int certificate_data_length,
                                    scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Creates a session with the |init_data_type|, |init_data| and |session_type|
  // provided.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // |promise| is resolved.
  virtual void CreateSession(const std::string& init_data_type,
                             const uint8* init_data,
                             int init_data_length,
                             SessionType session_type,
                             scoped_ptr<NewSessionCdmPromise> promise) = 0;

  // Loads a session with the |web_session_id| provided.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // |promise| is resolved.
  virtual void LoadSession(const std::string& web_session_id,
                           scoped_ptr<NewSessionCdmPromise> promise) = 0;

  // Updates a session specified by |web_session_id| with |response|.
  virtual void UpdateSession(const std::string& web_session_id,
                             const uint8* response,
                             int response_length,
                             scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Closes the session specified by |web_session_id|.
  virtual void CloseSession(const std::string& web_session_id,
                            scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Removes stored session data associated with the session specified by
  // |web_session_id|.
  virtual void RemoveSession(const std::string& web_session_id,
                             scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Retrieves the key IDs for keys in the session that the CDM knows are
  // currently usable to decrypt media data.
  virtual void GetUsableKeyIds(const std::string& web_session_id,
                               scoped_ptr<KeyIdsPromise> promise) = 0;

  // Gets the Decryptor object associated with the MediaKeys. Returns NULL if
  // no Decryptor object is associated. The returned object is only guaranteed
  // to be valid during the MediaKeys' lifetime.
  virtual Decryptor* GetDecryptor();

#if defined(ENABLE_BROWSER_CDMS)
  // Returns the CDM ID associated with |this|. May be kInvalidCdmId if no CDM
  // ID is associated.
  virtual int GetCdmId() const = 0;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

// Key event callbacks. See the spec for details:
// https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#event-summary
typedef base::Callback<void(const std::string& web_session_id,
                            const std::vector<uint8>& message,
                            const GURL& destination_url)> SessionMessageCB;

typedef base::Callback<void(const std::string& web_session_id)> SessionReadyCB;

typedef base::Callback<void(const std::string& web_session_id)> SessionClosedCB;

typedef base::Callback<void(const std::string& web_session_id,
                            MediaKeys::Exception exception_code,
                            uint32 system_code,
                            const std::string& error_message)> SessionErrorCB;

typedef base::Callback<void(const std::string& web_session_id,
                            bool has_additional_usable_key)>
    SessionKeysChangeCB;

typedef base::Callback<void(const std::string& web_session_id,
                            const base::Time& new_expiry_time)>
    SessionExpirationUpdateCB;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_KEYS_H_
