// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Various string utility functions */
'use strict';

/**
 * Converts a string to an array of bytes.
 * @param {string} s The string to convert.
 * @param {(Array|Uint8Array)=} bytes The Array-like object into which to store
 *     the bytes. A new Array will be created if not provided.
 * @return {(Array|Uint8Array)} An array of bytes representing the string.
 */
function UTIL_StringToBytes(s, bytes) {
  bytes = bytes || new Array(s.length);
  for (var i = 0; i < s.length; ++i)
    bytes[i] = s.charCodeAt(i);
  return bytes;
}

function UTIL_BytesToString(b) {
  return String.fromCharCode.apply(null, b);
}

function UTIL_BytesToHex(b) {
  if (!b) return '(null)';
  var hexchars = '0123456789ABCDEF';
  var hexrep = new Array(b.length * 2);

  for (var i = 0; i < b.length; ++i) {
    hexrep[i * 2 + 0] = hexchars.charAt((b[i] >> 4) & 15);
    hexrep[i * 2 + 1] = hexchars.charAt(b[i] & 15);
  }
  return hexrep.join('');
}

function UTIL_BytesToHexWithSeparator(b, sep) {
  var hexchars = '0123456789ABCDEF';
  var stride = 2 + (sep ? 1 : 0);
  var hexrep = new Array(b.length * stride);

  for (var i = 0; i < b.length; ++i) {
    if (sep) hexrep[i * stride + 0] = sep;
    hexrep[i * stride + stride - 2] = hexchars.charAt((b[i] >> 4) & 15);
    hexrep[i * stride + stride - 1] = hexchars.charAt(b[i] & 15);
  }
  return (sep ? hexrep.slice(1) : hexrep).join('');
}

function UTIL_HexToBytes(h) {
  var hexchars = '0123456789ABCDEFabcdef';
  var res = new Uint8Array(h.length / 2);
  for (var i = 0; i < h.length; i += 2) {
    if (hexchars.indexOf(h.substring(i, i + 1)) == -1) break;
    res[i / 2] = parseInt(h.substring(i, i + 2), 16);
  }
  return res;
}

function UTIL_equalArrays(a, b) {
  if (!a || !b) return false;
  if (a.length != b.length) return false;
  var accu = 0;
  for (var i = 0; i < a.length; ++i)
    accu |= a[i] ^ b[i];
  return accu === 0;
}

function UTIL_ltArrays(a, b) {
  if (a.length < b.length) return true;
  if (a.length > b.length) return false;
  for (var i = 0; i < a.length; ++i) {
    if (a[i] < b[i]) return true;
    if (a[i] > b[i]) return false;
  }
  return false;
}

function UTIL_gtArrays(a, b) {
  return UTIL_ltArrays(b, a);
}

function UTIL_geArrays(a, b) {
  return !UTIL_ltArrays(a, b);
}

function UTIL_unionArrays(a, b) {
  var obj = {};
  for (var i = 0; i < a.length; i++) {
    obj[a[i]] = a[i];
  }
  for (var i = 0; i < b.length; i++) {
    obj[b[i]] = b[i];
  }
  var union = [];
  for (var k in obj) {
    union.push(obj[k]);
  }
  return union;
}

function UTIL_getRandom(a) {
  var tmp = new Array(a);
  var rnd = new Uint8Array(a);
  window.crypto.getRandomValues(rnd);  // Yay!
  for (var i = 0; i < a; ++i) tmp[i] = rnd[i] & 255;
  return tmp;
}

function UTIL_setFavicon(icon) {
  // Construct a new favion link tag
  var faviconLink = document.createElement('link');
  faviconLink.rel = 'Shortcut Icon';
  faviconLink.type = 'image/x-icon';
  faviconLink.href = icon;

  // Remove the old favion, if it exists
  var head = document.getElementsByTagName('head')[0];
  var links = head.getElementsByTagName('link');
  for (var i = 0; i < links.length; i++) {
    var link = links[i];
    if (link.type == faviconLink.type && link.rel == faviconLink.rel) {
      head.removeChild(link);
    }
  }

  // Add in the new one
  head.appendChild(faviconLink);
}

// Erase all entries in array
function UTIL_clear(a) {
  if (a instanceof Array) {
    for (var i = 0; i < a.length; ++i)
      a[i] = 0;
  }
}

// hr:min:sec.milli string
function UTIL_time() {
  var d = new Date();
  var m = '000' + d.getMilliseconds();
  var s = d.toTimeString().substring(0, 8) + '.' + m.substring(m.length - 3);
  return s;
}
var UTIL_events = [];
var UTIL_max_events = 500;

function UTIL_fmt(s) {
  var line = UTIL_time() + ' ' + s;
  if (UTIL_events.push(line) > UTIL_max_events) {
    // Drop from head.
    UTIL_events.splice(0, UTIL_events.length - UTIL_max_events);
  }
  return line;
}
