/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBRTC_P2P_BASE_TESTTURNSERVER_H_
#define WEBRTC_P2P_BASE_TESTTURNSERVER_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/p2p/base/turnserver.h"
#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/thread.h"

namespace cricket {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

class TestTurnRedirector : public TurnRedirectInterface {
 public:
  explicit TestTurnRedirector(const std::vector<rtc::SocketAddress>& addresses)
      : alternate_server_addresses_(addresses),
        iter_(alternate_server_addresses_.begin()) {
  }

  virtual bool ShouldRedirect(const rtc::SocketAddress&,
                              rtc::SocketAddress* out) {
    if (!out || iter_ == alternate_server_addresses_.end()) {
      return false;
    }
    *out = *iter_++;
    return true;
  }

 private:
  const std::vector<rtc::SocketAddress>& alternate_server_addresses_;
  std::vector<rtc::SocketAddress>::const_iterator iter_;
};

class TestTurnServer : public TurnAuthInterface {
 public:
  TestTurnServer(rtc::Thread* thread,
                 const rtc::SocketAddress& udp_int_addr,
                 const rtc::SocketAddress& udp_ext_addr)
      : server_(thread) {
    AddInternalSocket(udp_int_addr, cricket::PROTO_UDP);
    server_.SetExternalSocketFactory(new rtc::BasicPacketSocketFactory(),
        udp_ext_addr);
    server_.set_realm(kTestRealm);
    server_.set_software(kTestSoftware);
    server_.set_auth_hook(this);
  }

  void set_enable_otu_nonce(bool enable) {
    server_.set_enable_otu_nonce(enable);
  }

  TurnServer* server() { return &server_; }

  void set_redirect_hook(TurnRedirectInterface* redirect_hook) {
    server_.set_redirect_hook(redirect_hook);
  }

  void AddInternalSocket(const rtc::SocketAddress& int_addr,
                         ProtocolType proto) {
    rtc::Thread* thread = rtc::Thread::Current();
    if (proto == cricket::PROTO_UDP) {
      server_.AddInternalSocket(rtc::AsyncUDPSocket::Create(
          thread->socketserver(), int_addr), proto);
    } else if (proto == cricket::PROTO_TCP) {
      // For TCP we need to create a server socket which can listen for incoming
      // new connections.
      rtc::AsyncSocket* socket =
          thread->socketserver()->CreateAsyncSocket(SOCK_STREAM);
      socket->Bind(int_addr);
      socket->Listen(5);
      server_.AddInternalServerSocket(socket, proto);
    }
  }

 private:
  // For this test server, succeed if the password is the same as the username.
  // Obviously, do not use this in a production environment.
  virtual bool GetKey(const std::string& username, const std::string& realm,
                      std::string* key) {
    return ComputeStunCredentialHash(username, realm, username, key);
  }

  TurnServer server_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TESTTURNSERVER_H_
