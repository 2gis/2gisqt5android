// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

#include <vector>

#include "net/base/net_export.h"

namespace net {

// Next Protocol Negotiation (NPN), if successful, results in agreement on an
// application-level string that specifies the application level protocol to
// use over the TLS connection. NextProto enumerates the application level
// protocols that we recognize.  Do not change or reuse values, because they
// are used to collect statistics on UMA.  Also, values must be in [0,499),
// because of the way TLS protocol negotiation extension information is added to
// UMA histogram.
enum NextProto {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoMinimumVersion = kProtoHTTP11,

  kProtoDeprecatedSPDY2 = 100,
  kProtoSPDYMinimumVersion = kProtoDeprecatedSPDY2,
  kProtoSPDY3 = 101,
  kProtoSPDY31 = 102,
  kProtoSPDY4 = 103,  // SPDY4 is HTTP/2.
  kProtoSPDYMaximumVersion = kProtoSPDY4,

  kProtoQUIC1SPDY3 = 200,

  kProtoMaximumVersion = kProtoQUIC1SPDY3,
};

// List of protocols to use for NPN, used for configuring HttpNetworkSessions.
typedef std::vector<NextProto> NextProtoVector;

// Convenience functions to create NextProtoVector.

NET_EXPORT NextProtoVector NextProtosHttpOnly();

// Default values, which are subject to change over time.  Currently just
// SPDY 3 and 3.1.
NET_EXPORT NextProtoVector NextProtosDefaults();

NET_EXPORT NextProtoVector NextProtosWithSpdyAndQuic(bool spdy_enabled,
                                                     bool quic_enabled);

// All of these also enable QUIC.
NET_EXPORT NextProtoVector NextProtosSpdy31();
NET_EXPORT NextProtoVector NextProtosSpdy4Http2();

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_
