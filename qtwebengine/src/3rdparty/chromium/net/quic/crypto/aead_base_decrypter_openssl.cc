// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aead_base_decrypter.h"

#include <openssl/err.h>
#include <openssl/evp.h>

#include "base/memory/scoped_ptr.h"

using base::StringPiece;

namespace net {

namespace {

// Clear OpenSSL error stack.
void ClearOpenSslErrors() {
  while (ERR_get_error()) {}
}

// In debug builds only, log OpenSSL error stack. Then clear OpenSSL error
// stack.
void DLogOpenSslErrors() {
#ifdef NDEBUG
  ClearOpenSslErrors();
#else
  while (unsigned long error = ERR_get_error()) {
    char buf[120];
    ERR_error_string_n(error, buf, arraysize(buf));
    DLOG(ERROR) << "OpenSSL error: " << buf;
  }
#endif
}

}  // namespace

AeadBaseDecrypter::AeadBaseDecrypter(const EVP_AEAD* aead_alg,
                                     size_t key_size,
                                     size_t auth_tag_size,
                                     size_t nonce_prefix_size)
    : aead_alg_(aead_alg),
      key_size_(key_size),
      auth_tag_size_(auth_tag_size),
      nonce_prefix_size_(nonce_prefix_size) {
  DCHECK_LE(key_size_, sizeof(key_));
  DCHECK_LE(nonce_prefix_size_, sizeof(nonce_prefix_));
}

AeadBaseDecrypter::~AeadBaseDecrypter() {}

bool AeadBaseDecrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), key_size_);
  if (key.size() != key_size_) {
    return false;
  }
  memcpy(key_, key.data(), key.size());

  EVP_AEAD_CTX_cleanup(ctx_.get());
  if (!EVP_AEAD_CTX_init(ctx_.get(), aead_alg_, key_, key_size_,
                         auth_tag_size_, NULL)) {
    DLogOpenSslErrors();
    return false;
  }

  return true;
}

bool AeadBaseDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), nonce_prefix_size_);
  if (nonce_prefix.size() != nonce_prefix_size_) {
    return false;
  }
  memcpy(nonce_prefix_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool AeadBaseDecrypter::Decrypt(StringPiece nonce,
                                StringPiece associated_data,
                                StringPiece ciphertext,
                                uint8* output,
                                size_t* output_length) {
  if (ciphertext.length() < auth_tag_size_ ||
      nonce.size() != nonce_prefix_size_ + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }

  ssize_t len = EVP_AEAD_CTX_open(
      ctx_.get(), output, ciphertext.size(),
      reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size(),
      reinterpret_cast<const uint8_t*>(ciphertext.data()), ciphertext.size(),
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.size());

  if (len < 0) {
    // Because QuicFramer does trial decryption, decryption errors are expected
    // when encryption level changes. So we don't log decryption errors.
    ClearOpenSslErrors();
    return false;
  }

  *output_length = len;
  return true;
}

QuicData* AeadBaseDecrypter::DecryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece ciphertext) {
  if (ciphertext.length() < auth_tag_size_) {
    return NULL;
  }
  size_t plaintext_size = ciphertext.length();
  scoped_ptr<char[]> plaintext(new char[plaintext_size]);

  uint8 nonce[sizeof(nonce_prefix_) + sizeof(sequence_number)];
  const size_t nonce_size = nonce_prefix_size_ + sizeof(sequence_number);
  DCHECK_LE(nonce_size, sizeof(nonce));
  memcpy(nonce, nonce_prefix_, nonce_prefix_size_);
  memcpy(nonce + nonce_prefix_size_, &sequence_number, sizeof(sequence_number));
  if (!Decrypt(StringPiece(reinterpret_cast<char*>(nonce), nonce_size),
               associated_data, ciphertext,
               reinterpret_cast<uint8*>(plaintext.get()),
               &plaintext_size)) {
    return NULL;
  }
  return new QuicData(plaintext.release(), plaintext_size, true);
}

StringPiece AeadBaseDecrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), key_size_);
}

StringPiece AeadBaseDecrypter::GetNoncePrefix() const {
  if (nonce_prefix_size_ == 0) {
    return StringPiece();
  }
  return StringPiece(reinterpret_cast<const char*>(nonce_prefix_),
                     nonce_prefix_size_);
}

}  // namespace net
