/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include "webrtc/p2p/base/stunserver.h"

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/logging.h"

namespace cricket {

StunServer::StunServer(rtc::AsyncUDPSocket* socket) : socket_(socket) {
  socket_->SignalReadPacket.connect(this, &StunServer::OnPacket);
}

StunServer::~StunServer() {
  socket_->SignalReadPacket.disconnect(this);
}

void StunServer::OnPacket(
    rtc::AsyncPacketSocket* socket, const char* buf, size_t size,
    const rtc::SocketAddress& remote_addr,
    const rtc::PacketTime& packet_time) {
  // Parse the STUN message; eat any messages that fail to parse.
  rtc::ByteBuffer bbuf(buf, size);
  StunMessage msg;
  if (!msg.Read(&bbuf)) {
    return;
  }

  // TODO: If unknown non-optional (<= 0x7fff) attributes are found, send a
  //       420 "Unknown Attribute" response.

  // Send the message to the appropriate handler function.
  switch (msg.type()) {
    case STUN_BINDING_REQUEST:
      OnBindingRequest(&msg, remote_addr);
      break;

    default:
      SendErrorResponse(msg, remote_addr, 600, "Operation Not Supported");
  }
}

void StunServer::OnBindingRequest(
    StunMessage* msg, const rtc::SocketAddress& remote_addr) {
  StunMessage response;
  GetStunBindReqponse(msg, remote_addr, &response);
  SendResponse(response, remote_addr);
}

void StunServer::SendErrorResponse(
    const StunMessage& msg, const rtc::SocketAddress& addr,
    int error_code, const char* error_desc) {
  StunMessage err_msg;
  err_msg.SetType(GetStunErrorResponseType(msg.type()));
  err_msg.SetTransactionID(msg.transaction_id());

  StunErrorCodeAttribute* err_code = StunAttribute::CreateErrorCode();
  err_code->SetCode(error_code);
  err_code->SetReason(error_desc);
  err_msg.AddAttribute(err_code);

  SendResponse(err_msg, addr);
}

void StunServer::SendResponse(
    const StunMessage& msg, const rtc::SocketAddress& addr) {
  rtc::ByteBuffer buf;
  msg.Write(&buf);
  rtc::PacketOptions options;
  if (socket_->SendTo(buf.Data(), buf.Length(), addr, options) < 0)
    LOG_ERR(LS_ERROR) << "sendto";
}

void StunServer::GetStunBindReqponse(StunMessage* request,
                                     const rtc::SocketAddress& remote_addr,
                                     StunMessage* response) const {
  response->SetType(STUN_BINDING_RESPONSE);
  response->SetTransactionID(request->transaction_id());

  // Tell the user the address that we received their request from.
  StunAddressAttribute* mapped_addr;
  if (!request->IsLegacy()) {
    mapped_addr = StunAttribute::CreateAddress(STUN_ATTR_MAPPED_ADDRESS);
  } else {
    mapped_addr = StunAttribute::CreateXorAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  }
  mapped_addr->SetAddress(remote_addr);
  response->AddAttribute(mapped_addr);
}

}  // namespace cricket
