// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview usbGnubby methods related to U2F support.
 */
'use strict';

/** Enroll */
usbGnubby.U2F_ENROLL = 0x01;
/** Request signature */
usbGnubby.U2F_SIGN = 0x02;
/** Request protocol version */
usbGnubby.U2F_VERSION = 0x03;

/** Request applet version */
usbGnubby.APPLET_VERSION = 0x11;  // First 3 bytes are applet version.

// APDU.P1 flags
/** Test of User Presence required */
usbGnubby.P1_TUP_REQUIRED = 0x01;
/** Consume a Test of User Presence */
usbGnubby.P1_TUP_CONSUME = 0x02;
/** Test signature only, no TUP. E.g. to check for existing enrollments. */
usbGnubby.P1_TUP_TESTONLY = 0x04;
/** Attest with device key */
usbGnubby.P1_INDIVIDUAL_KEY = 0x80;

// Version values
/** V1 of the applet. */
usbGnubby.U2F_V1 = 'U2F_V1';
/** V2 of the applet. */
usbGnubby.U2F_V2 = 'U2F_V2';

/** Perform enrollment
 * @param {ArrayBuffer|Uint8Array} challenge Enrollment challenge
 * @param {ArrayBuffer|Uint8Array} appIdHash Hashed application id
 * @param {function(...)} cb Result callback
 */
usbGnubby.prototype.enroll = function(challenge, appIdHash, cb) {
  var apdu = new Uint8Array(
      [0x00,
       usbGnubby.U2F_ENROLL,
       usbGnubby.P1_TUP_REQUIRED | usbGnubby.P1_TUP_CONSUME |
         usbGnubby.P1_INDIVIDUAL_KEY,
       0x00, 0x00, 0x00,
       challenge.length + appIdHash.length]);
  // TODO: only use P1_INDIVIDUAL_KEY for corp appIdHashes.
  var u8 = new Uint8Array(apdu.length + challenge.length +
      appIdHash.length + 2);
  for (var i = 0; i < apdu.length; ++i) u8[i] = apdu[i];
  for (var i = 0; i < challenge.length; ++i) u8[i + apdu.length] =
    challenge[i];
  for (var i = 0; i < appIdHash.length; ++i) {
    u8[i + apdu.length + challenge.length] = appIdHash[i];
  }
  this.apduReply_(u8.buffer, cb);
};

/** Request signature
 * @param {ArrayBuffer|Uint8Array} challengeHash Hashed signature challenge
 * @param {ArrayBuffer|Uint8Array} appIdHash Hashed application id
 * @param {ArrayBuffer|Uint8Array} keyHandle Key handle to use
 * @param {function(...)} cb Result callback
 * @param {boolean=} opt_nowink Request signature without winking
 *     (e.g. during enroll)
 */
usbGnubby.prototype.sign = function(challengeHash, appIdHash, keyHandle, cb,
                                    opt_nowink) {
  var self = this;
  // The sign command's format is ever-so-slightly different between V1 and V2,
  // so get this gnubby's version prior to sending it.
  this.version(function(rc, opt_data) {
    if (rc) {
      cb(rc);
      return;
    }
    var version = UTIL_BytesToString(new Uint8Array(opt_data || []));
    var apduDataLen =
      challengeHash.length + appIdHash.length + keyHandle.length;
    if (version != usbGnubby.U2F_V1) {
      // The V2 sign command includes a length byte for the key handle.
      apduDataLen++;
    }
    var apdu = new Uint8Array(
        [0x00,
         usbGnubby.U2F_SIGN,
         usbGnubby.P1_TUP_REQUIRED | usbGnubby.P1_TUP_CONSUME,
         0x00, 0x00, 0x00,
         apduDataLen]);
    if (opt_nowink) {
      // A signature request that does not want winking.
      // These are used during enroll to figure out whether a gnubby was already
      // enrolled.
      // Tell applet to not actually produce a signature, even
      // if already touched.
      apdu[2] |= usbGnubby.P1_TUP_TESTONLY;
    }
    var u8 = new Uint8Array(apdu.length + apduDataLen + 2);
    for (var i = 0; i < apdu.length; ++i) u8[i] = apdu[i];
    for (var i = 0; i < challengeHash.length; ++i) u8[i + apdu.length] =
      challengeHash[i];
    for (var i = 0; i < appIdHash.length; ++i) {
      u8[i + apdu.length + challengeHash.length] = appIdHash[i];
    }
    var keyHandleOffset = apdu.length + challengeHash.length + appIdHash.length;
    if (version != usbGnubby.U2F_V1) {
      u8[keyHandleOffset++] = keyHandle.length;
    }
    for (var i = 0; i < keyHandle.length; ++i) {
      u8[i + keyHandleOffset] = keyHandle[i];
    }
    self.apduReply_(u8.buffer, cb, opt_nowink);
  });
};

/** Request version information
 * @param {function(...)} cb Callback
 */
usbGnubby.prototype.version = function(cb) {
  if (!cb) cb = usbGnubby.defaultCallback;
  if (this.version_) {
    cb(-llGnubby.OK, this.version_);
    return;
  }
  var self = this;
  var apdu = new Uint8Array([0x00, usbGnubby.U2F_VERSION, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00]);
  this.apduReply_(apdu.buffer, function(rc, data) {
    if (rc == 0x6d00) {
      // Command not implemented. Pretend this is v1.
      var v1 = new Uint8Array(UTIL_StringToBytes(usbGnubby.U2F_V1));
      self.version_ = v1.buffer;
      cb(-llGnubby.OK, v1.buffer);
    } else {
      if (!rc) {
        self.version_ = data;
      }
      cb(rc, data);
    }
  });
};
