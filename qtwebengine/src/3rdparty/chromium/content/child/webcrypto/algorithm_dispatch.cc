// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/algorithm_dispatch.h"

#include "base/logging.h"
#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/algorithm_registry.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

Status DecryptDontCheckKeyUsage(const blink::WebCryptoAlgorithm& algorithm,
                                const blink::WebCryptoKey& key,
                                const CryptoData& data,
                                std::vector<uint8_t>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Decrypt(algorithm, key, data, buffer);
}

Status EncryptDontCheckUsage(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& data,
                             std::vector<uint8_t>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Encrypt(algorithm, key, data, buffer);
}

Status ExportKeyDontCheckExtractability(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(key.algorithm().id(), &impl);
  if (status.IsError())
    return status;

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return impl->ExportKeyRaw(key, buffer);
    case blink::WebCryptoKeyFormatSpki:
      return impl->ExportKeySpki(key, buffer);
    case blink::WebCryptoKeyFormatPkcs8:
      return impl->ExportKeyPkcs8(key, buffer);
    case blink::WebCryptoKeyFormatJwk:
      return impl->ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

}  // namespace

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageEncrypt))
    return Status::ErrorUnexpected();
  return EncryptDontCheckUsage(algorithm, key, data, buffer);
}

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageDecrypt))
    return Status::ErrorUnexpected();
  return DecryptDontCheckKeyUsage(algorithm, key, data, buffer);
}

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Digest(algorithm, data, buffer);
}

Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   GenerateKeyResult* result) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->GenerateKey(algorithm, extractable, usages, result);
}

// Note that this function may be called from the target Blink thread.
Status ImportKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  status = impl->VerifyKeyUsagesBeforeImportKey(format, usages);
  if (status.IsError())
    return status;

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return impl->ImportKeyRaw(key_data, algorithm, extractable, usages, key);
    case blink::WebCryptoKeyFormatSpki:
      return impl->ImportKeySpki(key_data, algorithm, extractable, usages, key);
    case blink::WebCryptoKeyFormatPkcs8:
      return impl->ImportKeyPkcs8(
          key_data, algorithm, extractable, usages, key);
    case blink::WebCryptoKeyFormatJwk:
      return impl->ImportKeyJwk(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupported();
  }
}

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8_t>* buffer) {
  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();
  return ExportKeyDontCheckExtractability(format, key, buffer);
}

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const CryptoData& data,
            std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageSign))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Sign(algorithm, key, data, buffer);
}

Status Verify(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& signature,
              const CryptoData& data,
              bool* signature_match) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageVerify))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  const AlgorithmImplementation* impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &impl);
  if (status.IsError())
    return status;

  return impl->Verify(algorithm, key, signature, data, signature_match);
}

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8_t>* buffer) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageWrapKey))
    return Status::ErrorUnexpected();

  std::vector<uint8_t> exported_data;
  Status status = ExportKey(format, key_to_wrap, &exported_data);
  if (status.IsError())
    return status;
  return EncryptDontCheckUsage(
      wrapping_algorithm, wrapping_key, CryptoData(exported_data), buffer);
}

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageUnwrapKey))
    return Status::ErrorUnexpected();
  if (wrapping_algorithm.id() != wrapping_key.algorithm().id())
    return Status::ErrorUnexpected();

  // Fail fast if the import is doomed to fail.
  const AlgorithmImplementation* import_impl = NULL;
  Status status = GetAlgorithmImplementation(algorithm.id(), &import_impl);
  if (status.IsError())
    return status;

  status = import_impl->VerifyKeyUsagesBeforeImportKey(format, usages);
  if (status.IsError())
    return status;

  std::vector<uint8_t> buffer;
  status = DecryptDontCheckKeyUsage(
      wrapping_algorithm, wrapping_key, wrapped_key_data, &buffer);
  if (status.IsError())
    return status;

  // NOTE that returning the details of ImportKey() failures may leak
  // information about the plaintext of the encrypted key (for instance the JWK
  // key_ops). As long as the ImportKey error messages don't describe actual
  // key bytes however this should be OK. For more discussion see
  // http://crubg.com/372040
  return ImportKey(
      format, CryptoData(buffer), algorithm, extractable, usages, key);
}

scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm) {
  PlatformInit();
  return CreatePlatformDigestor(algorithm);
}

}  // namespace webcrypto

}  // namespace content
