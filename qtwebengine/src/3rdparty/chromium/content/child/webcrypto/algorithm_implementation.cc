// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/algorithm_implementation.h"

#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

AlgorithmImplementation::~AlgorithmImplementation() {
}

Status AlgorithmImplementation::Encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const CryptoData& data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const CryptoData& data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Sign(const blink::WebCryptoAlgorithm& algorithm,
                                     const blink::WebCryptoKey& key,
                                     const CryptoData& data,
                                     std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Verify(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const CryptoData& signature,
    const CryptoData& data,
    bool* signature_match) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Digest(
    const blink::WebCryptoAlgorithm& algorithm,
    const CryptoData& data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    GenerateKeyResult* result) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  return Status::ErrorUnsupportedImportKeyFormat();
}

Status AlgorithmImplementation::ImportKeyRaw(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupportedImportKeyFormat();
}

Status AlgorithmImplementation::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupportedImportKeyFormat();
}

Status AlgorithmImplementation::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupportedImportKeyFormat();
}

Status AlgorithmImplementation::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupportedImportKeyFormat();
}

Status AlgorithmImplementation::ExportKeyRaw(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupportedExportKeyFormat();
}

Status AlgorithmImplementation::ExportKeyPkcs8(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupportedExportKeyFormat();
}

Status AlgorithmImplementation::ExportKeySpki(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupportedExportKeyFormat();
}

Status AlgorithmImplementation::ExportKeyJwk(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupportedExportKeyFormat();
}

}  // namespace webcrypto

}  // namespace content
