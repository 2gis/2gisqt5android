// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleSession.h"

class GURL;

namespace media {
class CdmFactory;
}

namespace content {

class WebContentDecryptionModuleSessionImpl;

// Owns the CDM instance and makes calls from session objects to the CDM.
// Forwards the web session ID-based callbacks of the MediaKeys interface to the
// appropriate session object. Callers should hold references to this class
// as long as they need the CDM instance.
class CdmSessionAdapter : public base::RefCounted<CdmSessionAdapter> {
 public:
  CdmSessionAdapter();

  // Returns true on success.
  bool Initialize(media::CdmFactory* cdm_factory,
                  const std::string& key_system,
                  const GURL& security_origin);

  // Provides a server certificate to be used to encrypt messages to the
  // license server.
  void SetServerCertificate(const uint8* server_certificate,
                            int server_certificate_length,
                            scoped_ptr<media::SimpleCdmPromise> promise);

  // Creates a new session and adds it to the internal map. The caller owns the
  // created session. RemoveSession() must be called when destroying it, if
  // RegisterSession() was called.
  WebContentDecryptionModuleSessionImpl* CreateSession();

  // Adds a session to the internal map. Called once the session is successfully
  // initialized. Returns true if the session was registered, false if there is
  // already an existing session with the same |web_session_id|.
  bool RegisterSession(
      const std::string& web_session_id,
      base::WeakPtr<WebContentDecryptionModuleSessionImpl> session);

  // Removes a session from the internal map.
  void UnregisterSession(const std::string& web_session_id);

  // Initializes a session with the |init_data_type|, |init_data| and
  // |session_type| provided.
  void InitializeNewSession(const std::string& init_data_type,
                            const uint8* init_data,
                            int init_data_length,
                            media::MediaKeys::SessionType session_type,
                            scoped_ptr<media::NewSessionCdmPromise> promise);

  // Loads the session specified by |web_session_id|.
  void LoadSession(const std::string& web_session_id,
                   scoped_ptr<media::NewSessionCdmPromise> promise);

  // Updates the session specified by |web_session_id| with |response|.
  void UpdateSession(const std::string& web_session_id,
                     const uint8* response,
                     int response_length,
                     scoped_ptr<media::SimpleCdmPromise> promise);

  // Closes the session specified by |web_session_id|.
  void CloseSession(const std::string& web_session_id,
                    scoped_ptr<media::SimpleCdmPromise> promise);

  // Removes stored session data associated with the session specified by
  // |web_session_id|.
  void RemoveSession(const std::string& web_session_id,
                     scoped_ptr<media::SimpleCdmPromise> promise);

  // Retrieves the key IDs for keys in the session that the CDM knows are
  // currently usable to decrypt media data.
  void GetUsableKeyIds(const std::string& web_session_id,
                       scoped_ptr<media::KeyIdsPromise> promise);

  // Returns the Decryptor associated with this CDM. May be NULL if no
  // Decryptor is associated with the MediaKeys object.
  // TODO(jrummell): Figure out lifetimes, as WMPI may still use the decryptor
  // after WebContentDecryptionModule is freed. http://crbug.com/330324
  media::Decryptor* GetDecryptor();

  // Returns a prefix to use for UMAs.
  const std::string& GetKeySystemUMAPrefix() const;

#if defined(ENABLE_BROWSER_CDMS)
  // Returns the CDM ID associated with the |media_keys_|. May be kInvalidCdmId
  // if no CDM ID is associated.
  int GetCdmId() const;
#endif

 private:
  friend class base::RefCounted<CdmSessionAdapter>;
  typedef base::hash_map<std::string,
                         base::WeakPtr<WebContentDecryptionModuleSessionImpl> >
      SessionMap;

  ~CdmSessionAdapter();

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& web_session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionKeysChange(const std::string& web_session_id,
                           bool has_additional_usable_key);
  void OnSessionExpirationUpdate(const std::string& web_session_id,
                                 const base::Time& new_expiry_time);
  void OnSessionReady(const std::string& web_session_id);
  void OnSessionClosed(const std::string& web_session_id);
  void OnSessionError(const std::string& web_session_id,
                      media::MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message);

  // Helper function of the callbacks.
  WebContentDecryptionModuleSessionImpl* GetSession(
      const std::string& web_session_id);

  scoped_ptr<media::MediaKeys> media_keys_;

  SessionMap sessions_;

  std::string key_system_uma_prefix_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CdmSessionAdapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmSessionAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_
