// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FLOW_CONTROLLER_H_
#define NET_QUIC_QUIC_FLOW_CONTROLLER_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicFlowControllerPeer;
}  // namespace test

class QuicConnection;

const QuicStreamId kConnectionLevelId = 0;

// QuicFlowController allows a QUIC stream or connection to perform flow
// control. The stream/connection owns a QuicFlowController which keeps track of
// bytes sent/received, can tell the owner if it is flow control blocked, and
// can send WINDOW_UPDATE or BLOCKED frames when needed.
class NET_EXPORT_PRIVATE QuicFlowController {
 public:
  QuicFlowController(QuicConnection* connection,
                     QuicStreamId id,
                     bool is_server,
                     uint64 send_window_offset,
                     uint64 receive_window_offset,
                     uint64 max_receive_window);
  ~QuicFlowController() {}

  // Called when we see a new highest received byte offset from the peer, either
  // via a data frame or a RST.
  // Returns true if this call changes highest_received_byte_offset_, and false
  // in the case where |new_offset| is <= highest_received_byte_offset_.
  bool UpdateHighestReceivedOffset(uint64 new_offset);

  // Called when bytes received from the peer are consumed locally. This may
  // trigger the sending of a WINDOW_UPDATE frame using |connection|.
  void AddBytesConsumed(uint64 bytes_consumed);

  // Called when bytes are sent to the peer.
  void AddBytesSent(uint64 bytes_sent);

  // Set a new send window offset.
  // Returns true if this changes send_window_offset_, and false in the case
  // where |new_send_window| is <= send_window_offset_.
  bool UpdateSendWindowOffset(uint64 new_send_window_offset);

  // Returns the current available send window.
  uint64 SendWindowSize() const;

  // Send a BLOCKED frame if appropriate.
  void MaybeSendBlocked();

  // Disable flow control.
  void Disable();

  // Returns true if flow control is enabled.
  bool IsEnabled() const;

  // Returns true if flow control send limits have been reached.
  bool IsBlocked() const;

  // Returns true if flow control receive limits have been violated by the peer.
  bool FlowControlViolation();

  uint64 bytes_consumed() const { return bytes_consumed_; }

  uint64 highest_received_byte_offset() const {
    return highest_received_byte_offset_;
  }

 private:
  friend class test::QuicFlowControllerPeer;

  // Send a WINDOW_UPDATE frame if appropriate.
  void MaybeSendWindowUpdate();

  // The parent connection, used to send connection close on flow control
  // violation, and WINDOW_UPDATE and BLOCKED frames when appropriate.
  // Not owned.
  QuicConnection* connection_;

  // ID of stream this flow controller belongs to. This can be 0 if this is a
  // connection level flow controller.
  QuicStreamId id_;

  // True if flow control is enabled.
  bool is_enabled_;

  // True if this is owned by a server.
  bool is_server_;

  // Track number of bytes received from the peer, which have been consumed
  // locally.
  uint64 bytes_consumed_;

  // The highest byte offset we have seen from the peer. This could be the
  // highest offset in a data frame, or a final value in a RST.
  uint64 highest_received_byte_offset_;

  // Tracks number of bytes sent to the peer.
  uint64 bytes_sent_;

  // The absolute offset in the outgoing byte stream. If this offset is reached
  // then we become flow control blocked until we receive a WINDOW_UPDATE.
  uint64 send_window_offset_;

  // The absolute offset in the incoming byte stream. The peer should never send
  // us bytes which are beyond this offset.
  uint64 receive_window_offset_;

  // Largest size the receive window can grow to.
  uint64 max_receive_window_;

  // Keep track of the last time we sent a BLOCKED frame. We should only send
  // another when the number of bytes we have sent has changed.
  uint64 last_blocked_send_window_offset_;

  DISALLOW_COPY_AND_ASSIGN(QuicFlowController);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FLOW_CONTROLLER_H_
