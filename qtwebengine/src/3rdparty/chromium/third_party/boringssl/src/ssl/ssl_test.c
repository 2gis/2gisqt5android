/* Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>
#include <string.h>

#include <openssl/base64.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

typedef struct {
  int id;
  int in_group_flag;
} EXPECTED_CIPHER;

typedef struct {
  /* The rule string to apply. */
  const char *rule;
  /* The list of expected ciphers, in order, terminated with -1. */
  const EXPECTED_CIPHER *expected;
} CIPHER_TEST;

/* Selecting individual ciphers should work. */
static const char kRule1[] =
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES128-GCM-SHA256";

static const EXPECTED_CIPHER kExpected1[] = {
  { TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* + reorders selected ciphers to the end, keeping their relative
 * order. */
static const char kRule2[] =
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES128-GCM-SHA256:"
    "+aRSA";

static const EXPECTED_CIPHER kExpected2[] = {
  { TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* ! banishes ciphers from future selections. */
static const char kRule3[] =
    "!aRSA:"
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES128-GCM-SHA256";

static const EXPECTED_CIPHER kExpected3[] = {
  { TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* Multiple masks can be ANDed in a single rule. */
static const char kRule4[] = "kRSA+AESGCM+AES128";

static const EXPECTED_CIPHER kExpected4[] = {
  { TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* - removes selected ciphers, but preserves their order for future
 * selections. Select AES_128_GCM, but order the key exchanges RSA,
 * DHE_RSA, ECDHE_RSA. */
static const char kRule5[] =
    "ALL:-kEECDH:-kEDH:-kRSA:-ALL:"
    "AESGCM+AES128+aRSA";

static const EXPECTED_CIPHER kExpected5[] = {
  { TLS1_CK_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_DHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* Unknown selectors are no-ops. */
static const char kRule6[] =
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES128-GCM-SHA256:"
    "BOGUS1:-BOGUS2:+BOGUS3:!BOGUS4";

static const EXPECTED_CIPHER kExpected6[] = {
  { TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* Square brackets specify equi-preference groups. */
static const char kRule7[] =
    "[ECDHE-ECDSA-CHACHA20-POLY1305|ECDHE-ECDSA-AES128-GCM-SHA256]:"
    "[ECDHE-RSA-CHACHA20-POLY1305]:"
    "ECDHE-RSA-AES128-GCM-SHA256";

static const EXPECTED_CIPHER kExpected7[] = {
  { TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305, 1 },
  { TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0 },
  { TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0 },
  { -1, -1 },
};

/* @STRENGTH performs a stable strength-sort of the selected
 * ciphers and only the selected ciphers. */
static const char kRule8[] =
    /* To simplify things, banish all but {ECDHE_RSA,RSA} x
     * {CHACHA20,AES_256_CBC,AES_128_CBC,RC4} x SHA1. */
    "!kEDH:!AESGCM:!3DES:!SHA256:!MD5:!SHA384:"
    /* Order some ciphers backwards by strength. */
    "ALL:-CHACHA20:-AES256:-AES128:-RC4:-ALL:"
    /* Select ECDHE ones and sort them by strength. Ties should resolve
     * based on the order above. */
    "kEECDH:@STRENGTH:-ALL:"
    /* Now bring back everything uses RSA. ECDHE_RSA should be first,
     * sorted by strength. Then RSA, backwards by strength. */
    "aRSA";

static const EXPECTED_CIPHER kExpected8[] = {
  { TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA, 0 },
  { TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_RC4_128_SHA, 0 },
  { TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0 },
  { SSL3_CK_RSA_RC4_128_SHA, 0 },
  { TLS1_CK_RSA_WITH_AES_128_SHA, 0 },
  { TLS1_CK_RSA_WITH_AES_256_SHA, 0 },
  { -1, -1 },
};

static CIPHER_TEST kCipherTests[] = {
  { kRule1, kExpected1 },
  { kRule2, kExpected2 },
  { kRule3, kExpected3 },
  { kRule4, kExpected4 },
  { kRule5, kExpected5 },
  { kRule6, kExpected6 },
  { kRule7, kExpected7 },
  { kRule8, kExpected8 },
  { NULL, NULL },
};

static const char *kBadRules[] = {
  /* Invalid brackets. */
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256",
  "RSA]",
  "[[RSA]]",
  /* Operators inside brackets */
  "[+RSA]",
  /* Unknown directive. */
  "@BOGUS",
  /* Empty cipher lists error at SSL_CTX_set_cipher_list. */
  "",
  "BOGUS",
  /* Invalid command. */
  "?BAR",
  /* Special operators are not allowed if groups are used. */
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:+FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:!FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:-FOO",
  "[ECDHE-RSA-CHACHA20-POLY1305|ECDHE-RSA-AES128-GCM-SHA256]:@STRENGTH",
  NULL,
};

static void print_cipher_preference_list(
    struct ssl_cipher_preference_list_st *list) {
  size_t i;
  int in_group = 0;
  for (i = 0; i < sk_SSL_CIPHER_num(list->ciphers); i++) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(list->ciphers, i);
    if (!in_group && list->in_group_flags[i]) {
      fprintf(stderr, "\t[\n");
      in_group = 1;
    }
    fprintf(stderr, "\t");
    if (in_group) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "%s\n", SSL_CIPHER_get_name(cipher));
    if (in_group && !list->in_group_flags[i]) {
      fprintf(stderr, "\t]\n");
      in_group = 0;
    }
  }
}

static int test_cipher_rule(CIPHER_TEST *t) {
  int ret = 0;
  SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
  size_t i;

  if (!SSL_CTX_set_cipher_list(ctx, t->rule)) {
    fprintf(stderr, "Error testing cipher rule '%s'\n", t->rule);
    BIO_print_errors_fp(stderr);
    goto done;
  }

  /* Compare the two lists. */
  for (i = 0; i < sk_SSL_CIPHER_num(ctx->cipher_list->ciphers); i++) {
    const SSL_CIPHER *cipher =
        sk_SSL_CIPHER_value(ctx->cipher_list->ciphers, i);
    if (t->expected[i].id != SSL_CIPHER_get_id(cipher) ||
        t->expected[i].in_group_flag != ctx->cipher_list->in_group_flags[i]) {
      fprintf(stderr, "Error: cipher rule '%s' evaluted to:\n", t->rule);
      print_cipher_preference_list(ctx->cipher_list);
      goto done;
    }
  }

  if (t->expected[i].id != -1) {
    fprintf(stderr, "Error: cipher rule '%s' evaluted to:\n", t->rule);
    print_cipher_preference_list(ctx->cipher_list);
    goto done;
  }

  ret = 1;
done:
  SSL_CTX_free(ctx);
  return ret;
}

static int test_cipher_rules(void) {
  size_t i;
  for (i = 0; kCipherTests[i].rule != NULL; i++) {
    if (!test_cipher_rule(&kCipherTests[i])) {
      return 0;
    }
  }

  for (i = 0; kBadRules[i] != NULL; i++) {
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if (SSL_CTX_set_cipher_list(ctx, kBadRules[i])) {
      fprintf(stderr, "Cipher rule '%s' unexpectedly succeeded\n", kBadRules[i]);
      return 0;
    }
    ERR_clear_error();
    SSL_CTX_free(ctx);
  }

  return 1;
}

/* kOpenSSLSession is a serialized SSL_SESSION generated from openssl
 * s_client -sess_out. */
static const char kOpenSSLSession[] =
  "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"
  "i4gv7Y5oliyn";

/* kCustomSession is a custom serialized SSL_SESSION generated by
 * filling in missing fields from |kOpenSSLSession|. This includes
 * providing |peer_sha256|, so |peer| is not serialized. */
static const char kCustomSession[] =
  "MIIBfwIBAQICAwMEAsAvBCAG5Q1ndq4Yfmbeo1zwLkNRKmCXGdNgWvGT3cskV0yQ"
  "kAQwJlrlzkAWBOWiLj/jJ76D7l+UXoizP2KI2C7I2FccqMmIfFmmkUy32nIJ0mZH"
  "IWoJoQYCBFRDO46iBAICASykAwQBAqUDAgEUphAEDnd3dy5nb29nbGUuY29tpwcE"
  "BWhlbGxvqAcEBXdvcmxkqQUCAwGJwKqBpwSBpBwUQvoeOk0Kg36SYTcLEkXqKwOB"
  "fF9vE4KX0NxeLwjcDTpsuh3qXEaZ992r1N38VDcyS6P7I6HBYN9BsNHM362zZnY2"
  "7GpTw+Kwd751CLoXFPoaMOe57dbBpXoro6Pd3BTbf/Tzr88K06yEOTDKPNj3+inb"
  "MaVigtK4PLyPq+Topyzvx9USFgRvyuoxn0Hgb+R0A3j6SLRuyOdAi4gv7Y5oliyn"
  "rSIEIAYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGrgMEAQevAwQBBLAD"
  "BAEF";

static int decode_base64(uint8_t **out, size_t *out_len, const char *in) {
  size_t len;

  if (!EVP_DecodedLength(&len, strlen(in))) {
    fprintf(stderr, "EVP_DecodedLength failed\n");
    return 0;
  }

  *out = OPENSSL_malloc(len);
  if (*out == NULL) {
    fprintf(stderr, "malloc failed\n");
    return 0;
  }

  if (!EVP_DecodeBase64(*out, out_len, len, (const uint8_t *)in,
                        strlen(in))) {
    fprintf(stderr, "EVP_DecodeBase64 failed\n");
    OPENSSL_free(*out);
    *out = NULL;
    return 0;
  }
  return 1;
}

static int test_ssl_session_asn1(const char *input_b64) {
  int ret = 0, len;
  size_t input_len, encoded_len;
  uint8_t *input = NULL, *encoded = NULL;
  const uint8_t *cptr;
  uint8_t *ptr;
  SSL_SESSION *session = NULL;

  /* Decode the input. */
  if (!decode_base64(&input, &input_len, input_b64)) {
    goto done;
  }

  /* Verify the SSL_SESSION decodes. */
  cptr = input;
  session = d2i_SSL_SESSION(NULL, &cptr, input_len);
  if (session == NULL || cptr != input + input_len) {
    fprintf(stderr, "d2i_SSL_SESSION failed\n");
    goto done;
  }

  /* Verify the SSL_SESSION encoding round-trips. */
  if (!SSL_SESSION_to_bytes(session, &encoded, &encoded_len)) {
    fprintf(stderr, "SSL_SESSION_to_bytes failed\n");
    goto done;
  }
  if (encoded_len != input_len ||
      memcmp(input, encoded, input_len) != 0) {
    fprintf(stderr, "SSL_SESSION_to_bytes did not round-trip\n");
    goto done;
  }
  OPENSSL_free(encoded);
  encoded = NULL;

  /* Verify the SSL_SESSION encoding round-trips via the legacy API. */
  len = i2d_SSL_SESSION(session, NULL);
  if (len < 0 || (size_t)len != input_len) {
    fprintf(stderr, "i2d_SSL_SESSION(NULL) returned invalid length\n");
    goto done;
  }

  encoded = OPENSSL_malloc(input_len);
  if (encoded == NULL) {
    fprintf(stderr, "malloc failed\n");
    goto done;
  }
  ptr = encoded;
  len = i2d_SSL_SESSION(session, &ptr);
  if (len < 0 || (size_t)len != input_len) {
    fprintf(stderr, "i2d_SSL_SESSION returned invalid length\n");
    goto done;
  }
  if (ptr != encoded + input_len) {
    fprintf(stderr, "i2d_SSL_SESSION did not advance ptr correctly\n");
    goto done;
  }
  if (memcmp(input, encoded, input_len) != 0) {
    fprintf(stderr, "i2d_SSL_SESSION did not round-trip\n");
    goto done;
  }

  ret = 1;

 done:
  if (!ret) {
    BIO_print_errors_fp(stderr);
  }

  if (session) {
    SSL_SESSION_free(session);
  }
  if (input) {
    OPENSSL_free(input);
  }
  if (encoded) {
    OPENSSL_free(encoded);
  }
  return ret;
}

int main(void) {
  SSL_library_init();

  if (!test_cipher_rules() ||
      !test_ssl_session_asn1(kOpenSSLSession) ||
      !test_ssl_session_asn1(kCustomSession)) {
    return 1;
  }

  printf("PASS\n");
  return 0;
}
