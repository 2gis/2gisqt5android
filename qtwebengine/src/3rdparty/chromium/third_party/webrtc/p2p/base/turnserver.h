/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TURNSERVER_H_
#define WEBRTC_P2P_BASE_TURNSERVER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socketaddress.h"

namespace rtc {
class ByteBuffer;
class PacketSocketFactory;
class Thread;
}

namespace cricket {

class StunMessage;
class TurnMessage;

// The default server port for TURN, as specified in RFC5766.
const int TURN_SERVER_PORT = 3478;

// An interface through which the MD5 credential hash can be retrieved.
class TurnAuthInterface {
 public:
  // Gets HA1 for the specified user and realm.
  // HA1 = MD5(A1) = MD5(username:realm:password).
  // Return true if the given username and realm are valid, or false if not.
  virtual bool GetKey(const std::string& username, const std::string& realm,
                      std::string* key) = 0;
};

// An interface enables Turn Server to control redirection behavior.
class TurnRedirectInterface {
 public:
  virtual bool ShouldRedirect(const rtc::SocketAddress& address,
                              rtc::SocketAddress* out) = 0;
  virtual ~TurnRedirectInterface() {}
};

// The core TURN server class. Give it a socket to listen on via
// AddInternalServerSocket, and a factory to create external sockets via
// SetExternalSocketFactory, and it's ready to go.
// Not yet wired up: TCP support.
class TurnServer : public sigslot::has_slots<> {
 public:
  explicit TurnServer(rtc::Thread* thread);
  ~TurnServer();

  // Gets/sets the realm value to use for the server.
  const std::string& realm() const { return realm_; }
  void set_realm(const std::string& realm) { realm_ = realm; }

  // Gets/sets the value for the SOFTWARE attribute for TURN messages.
  const std::string& software() const { return software_; }
  void set_software(const std::string& software) { software_ = software; }

  // Sets the authentication callback; does not take ownership.
  void set_auth_hook(TurnAuthInterface* auth_hook) { auth_hook_ = auth_hook; }

  void set_redirect_hook(TurnRedirectInterface* redirect_hook) {
    redirect_hook_ = redirect_hook;
  }

  void set_enable_otu_nonce(bool enable) { enable_otu_nonce_ = enable; }

  // Starts listening for packets from internal clients.
  void AddInternalSocket(rtc::AsyncPacketSocket* socket,
                         ProtocolType proto);
  // Starts listening for the connections on this socket. When someone tries
  // to connect, the connection will be accepted and a new internal socket
  // will be added.
  void AddInternalServerSocket(rtc::AsyncSocket* socket,
                               ProtocolType proto);
  // Specifies the factory to use for creating external sockets.
  void SetExternalSocketFactory(rtc::PacketSocketFactory* factory,
                                const rtc::SocketAddress& address);

 private:
  // Encapsulates the client's connection to the server.
  class Connection {
   public:
    Connection() : proto_(PROTO_UDP), socket_(NULL) {}
    Connection(const rtc::SocketAddress& src,
               ProtocolType proto,
               rtc::AsyncPacketSocket* socket);
    const rtc::SocketAddress& src() const { return src_; }
    rtc::AsyncPacketSocket* socket() { return socket_; }
    bool operator==(const Connection& t) const;
    bool operator<(const Connection& t) const;
    std::string ToString() const;

   private:
    rtc::SocketAddress src_;
    rtc::SocketAddress dst_;
    cricket::ProtocolType proto_;
    rtc::AsyncPacketSocket* socket_;
  };
  class Allocation;
  class Permission;
  class Channel;
  typedef std::map<Connection, Allocation*> AllocationMap;

  void OnInternalPacket(rtc::AsyncPacketSocket* socket, const char* data,
                        size_t size, const rtc::SocketAddress& address,
                        const rtc::PacketTime& packet_time);

  void OnNewInternalConnection(rtc::AsyncSocket* socket);

  // Accept connections on this server socket.
  void AcceptConnection(rtc::AsyncSocket* server_socket);
  void OnInternalSocketClose(rtc::AsyncPacketSocket* socket, int err);

  void HandleStunMessage(Connection* conn, const char* data, size_t size);
  void HandleBindingRequest(Connection* conn, const StunMessage* msg);
  void HandleAllocateRequest(Connection* conn, const TurnMessage* msg,
                             const std::string& key);

  bool GetKey(const StunMessage* msg, std::string* key);
  bool CheckAuthorization(Connection* conn, const StunMessage* msg,
                          const char* data, size_t size,
                          const std::string& key);
  std::string GenerateNonce() const;
  bool ValidateNonce(const std::string& nonce) const;

  Allocation* FindAllocation(Connection* conn);
  Allocation* CreateAllocation(Connection* conn, int proto,
                               const std::string& key);

  void SendErrorResponse(Connection* conn, const StunMessage* req,
                         int code, const std::string& reason);

  void SendErrorResponseWithRealmAndNonce(Connection* conn,
                                          const StunMessage* req,
                                          int code,
                                          const std::string& reason);

  void SendErrorResponseWithAlternateServer(Connection* conn,
                                            const StunMessage* req,
                                            const rtc::SocketAddress& addr);

  void SendStun(Connection* conn, StunMessage* msg);
  void Send(Connection* conn, const rtc::ByteBuffer& buf);

  void OnAllocationDestroyed(Allocation* allocation);
  void DestroyInternalSocket(rtc::AsyncPacketSocket* socket);

  typedef std::map<rtc::AsyncPacketSocket*,
                   ProtocolType> InternalSocketMap;
  typedef std::map<rtc::AsyncSocket*,
                   ProtocolType> ServerSocketMap;

  rtc::Thread* thread_;
  std::string nonce_key_;
  std::string realm_;
  std::string software_;
  TurnAuthInterface* auth_hook_;
  TurnRedirectInterface* redirect_hook_;
  // otu - one-time-use. Server will respond with 438 if it's
  // sees the same nonce in next transaction.
  bool enable_otu_nonce_;

  InternalSocketMap server_sockets_;
  ServerSocketMap server_listen_sockets_;
  rtc::scoped_ptr<rtc::PacketSocketFactory>
      external_socket_factory_;
  rtc::SocketAddress external_addr_;

  AllocationMap allocations_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TURNSERVER_H_
