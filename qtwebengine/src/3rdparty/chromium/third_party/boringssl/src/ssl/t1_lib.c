/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <openssl/bytestring.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/rand.h>

#include "ssl_locl.h"
static int tls_decrypt_ticket(SSL *s, const unsigned char *tick, int ticklen,
				const unsigned char *sess_id, int sesslen,
				SSL_SESSION **psess);
static int ssl_check_clienthello_tlsext(SSL *s);
static int ssl_check_serverhello_tlsext(SSL *s);

SSL3_ENC_METHOD TLSv1_enc_data={
	tls1_enc,
	tls1_mac,
	tls1_setup_key_block,
	tls1_generate_master_secret,
	tls1_change_cipher_state,
	tls1_final_finish_mac,
	TLS1_FINISH_MAC_LENGTH,
	tls1_cert_verify_mac,
	TLS_MD_CLIENT_FINISH_CONST,TLS_MD_CLIENT_FINISH_CONST_SIZE,
	TLS_MD_SERVER_FINISH_CONST,TLS_MD_SERVER_FINISH_CONST_SIZE,
	tls1_alert_code,
	tls1_export_keying_material,
	0,
	SSL3_HM_HEADER_LENGTH,
	ssl3_set_handshake_header,
	ssl3_handshake_write,
	ssl3_add_to_finished_hash,
	};

SSL3_ENC_METHOD TLSv1_1_enc_data={
	tls1_enc,
	tls1_mac,
	tls1_setup_key_block,
	tls1_generate_master_secret,
	tls1_change_cipher_state,
	tls1_final_finish_mac,
	TLS1_FINISH_MAC_LENGTH,
	tls1_cert_verify_mac,
	TLS_MD_CLIENT_FINISH_CONST,TLS_MD_CLIENT_FINISH_CONST_SIZE,
	TLS_MD_SERVER_FINISH_CONST,TLS_MD_SERVER_FINISH_CONST_SIZE,
	tls1_alert_code,
	tls1_export_keying_material,
	SSL_ENC_FLAG_EXPLICIT_IV,
	SSL3_HM_HEADER_LENGTH,
	ssl3_set_handshake_header,
	ssl3_handshake_write,
	ssl3_add_to_finished_hash,
	};

SSL3_ENC_METHOD TLSv1_2_enc_data={
	tls1_enc,
	tls1_mac,
	tls1_setup_key_block,
	tls1_generate_master_secret,
	tls1_change_cipher_state,
	tls1_final_finish_mac,
	TLS1_FINISH_MAC_LENGTH,
	tls1_cert_verify_mac,
	TLS_MD_CLIENT_FINISH_CONST,TLS_MD_CLIENT_FINISH_CONST_SIZE,
	TLS_MD_SERVER_FINISH_CONST,TLS_MD_SERVER_FINISH_CONST_SIZE,
	tls1_alert_code,
	tls1_export_keying_material,
	SSL_ENC_FLAG_EXPLICIT_IV|SSL_ENC_FLAG_SIGALGS|SSL_ENC_FLAG_SHA256_PRF
		|SSL_ENC_FLAG_TLS1_2_CIPHERS,
	SSL3_HM_HEADER_LENGTH,
	ssl3_set_handshake_header,
	ssl3_handshake_write,
	ssl3_add_to_finished_hash,
	};

static int compare_uint16_t(const void *p1, const void *p2)
	{
	uint16_t u1 = *((const uint16_t*)p1);
	uint16_t u2 = *((const uint16_t*)p2);
	if (u1 < u2)
		{
		return -1;
		}
	else if (u1 > u2)
		{
		return 1;
		}
	else
		{
		return 0;
		}
	}

/* Per http://tools.ietf.org/html/rfc5246#section-7.4.1.4, there may not be more
 * than one extension of the same type in a ClientHello or ServerHello. This
 * function does an initial scan over the extensions block to filter those
 * out. */
static int tls1_check_duplicate_extensions(const CBS *cbs)
	{
	CBS extensions = *cbs;
	size_t num_extensions = 0, i = 0;
	uint16_t *extension_types = NULL;
	int ret = 0;

	/* First pass: count the extensions. */
	while (CBS_len(&extensions) > 0)
		{
		uint16_t type;
		CBS extension;

		if (!CBS_get_u16(&extensions, &type) ||
			!CBS_get_u16_length_prefixed(&extensions, &extension))
			{
			goto done;
			}

		num_extensions++;
		}

	if (num_extensions == 0)
		{
		return 1;
		}

	extension_types = (uint16_t*)OPENSSL_malloc(sizeof(uint16_t) * num_extensions);
	if (extension_types == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, tls1_check_duplicate_extensions, ERR_R_MALLOC_FAILURE);
		goto done;
		}

	/* Second pass: gather the extension types. */
	extensions = *cbs;
	for (i = 0; i < num_extensions; i++)
		{
		CBS extension;

		if (!CBS_get_u16(&extensions, &extension_types[i]) ||
			!CBS_get_u16_length_prefixed(&extensions, &extension))
			{
			/* This should not happen. */
			goto done;
			}
		}
	assert(CBS_len(&extensions) == 0);

	/* Sort the extensions and make sure there are no duplicates. */
	qsort(extension_types, num_extensions, sizeof(uint16_t), compare_uint16_t);
	for (i = 1; i < num_extensions; i++)
		{
		if (extension_types[i-1] == extension_types[i])
			{
			goto done;
			}
		}

	ret = 1;
done:
	if (extension_types)
		OPENSSL_free(extension_types);
	return ret;
	}

char ssl_early_callback_init(struct ssl_early_callback_ctx *ctx)
	{
	CBS client_hello, session_id, cipher_suites, compression_methods, extensions;

	CBS_init(&client_hello, ctx->client_hello, ctx->client_hello_len);

	/* Skip client version. */
	if (!CBS_skip(&client_hello, 2))
		return 0;

	/* Skip client nonce. */
	if (!CBS_skip(&client_hello, 32))
		return 0;

	/* Extract session_id. */
	if (!CBS_get_u8_length_prefixed(&client_hello, &session_id))
		return 0;
	ctx->session_id = CBS_data(&session_id);
	ctx->session_id_len = CBS_len(&session_id);

	/* Skip past DTLS cookie */
	if (SSL_IS_DTLS(ctx->ssl))
		{
		CBS cookie;

		if (!CBS_get_u8_length_prefixed(&client_hello, &cookie))
			return 0;
		}

	/* Extract cipher_suites. */
	if (!CBS_get_u16_length_prefixed(&client_hello, &cipher_suites) ||
		CBS_len(&cipher_suites) < 2 ||
		(CBS_len(&cipher_suites) & 1) != 0)
		return 0;
	ctx->cipher_suites = CBS_data(&cipher_suites);
	ctx->cipher_suites_len = CBS_len(&cipher_suites);

	/* Extract compression_methods. */
	if (!CBS_get_u8_length_prefixed(&client_hello, &compression_methods) ||
		CBS_len(&compression_methods) < 1)
		return 0;
	ctx->compression_methods = CBS_data(&compression_methods);
	ctx->compression_methods_len = CBS_len(&compression_methods);

	/* If the ClientHello ends here then it's valid, but doesn't have any
	 * extensions. (E.g. SSLv3.) */
	if (CBS_len(&client_hello) == 0)
		{
		ctx->extensions = NULL;
		ctx->extensions_len = 0;
		return 1;
		}

	/* Extract extensions and check it is valid. */
	if (!CBS_get_u16_length_prefixed(&client_hello, &extensions) ||
		!tls1_check_duplicate_extensions(&extensions) ||
		CBS_len(&client_hello) != 0)
		return 0;
	ctx->extensions = CBS_data(&extensions);
	ctx->extensions_len = CBS_len(&extensions);

	return 1;
	}

char
SSL_early_callback_ctx_extension_get(const struct ssl_early_callback_ctx *ctx,
				     uint16_t extension_type,
				     const unsigned char **out_data,
				     size_t *out_len)
	{
	CBS extensions;

	CBS_init(&extensions, ctx->extensions, ctx->extensions_len);

	while (CBS_len(&extensions) != 0)
		{
		uint16_t type;
		CBS extension;

		/* Decode the next extension. */
		if (!CBS_get_u16(&extensions, &type) ||
			!CBS_get_u16_length_prefixed(&extensions, &extension))
			return 0;

		if (type == extension_type)
			{
			*out_data = CBS_data(&extension);
			*out_len = CBS_len(&extension);
			return 1;
			}
		}

	return 0;
	}


static const int nid_list[] =
	{
		NID_sect163k1, /* sect163k1 (1) */
		NID_sect163r1, /* sect163r1 (2) */
		NID_sect163r2, /* sect163r2 (3) */
		NID_sect193r1, /* sect193r1 (4) */ 
		NID_sect193r2, /* sect193r2 (5) */ 
		NID_sect233k1, /* sect233k1 (6) */
		NID_sect233r1, /* sect233r1 (7) */ 
		NID_sect239k1, /* sect239k1 (8) */ 
		NID_sect283k1, /* sect283k1 (9) */
		NID_sect283r1, /* sect283r1 (10) */ 
		NID_sect409k1, /* sect409k1 (11) */ 
		NID_sect409r1, /* sect409r1 (12) */
		NID_sect571k1, /* sect571k1 (13) */ 
		NID_sect571r1, /* sect571r1 (14) */ 
		NID_secp160k1, /* secp160k1 (15) */
		NID_secp160r1, /* secp160r1 (16) */ 
		NID_secp160r2, /* secp160r2 (17) */ 
		NID_secp192k1, /* secp192k1 (18) */
		NID_X9_62_prime192v1, /* secp192r1 (19) */ 
		NID_secp224k1, /* secp224k1 (20) */ 
		NID_secp224r1, /* secp224r1 (21) */
		NID_secp256k1, /* secp256k1 (22) */ 
		NID_X9_62_prime256v1, /* secp256r1 (23) */ 
		NID_secp384r1, /* secp384r1 (24) */
		NID_secp521r1,  /* secp521r1 (25) */	
		NID_brainpoolP256r1,  /* brainpoolP256r1 (26) */	
		NID_brainpoolP384r1,  /* brainpoolP384r1 (27) */	
		NID_brainpoolP512r1  /* brainpool512r1 (28) */	
	};

static const uint8_t ecformats_default[] =
	{
	TLSEXT_ECPOINTFORMAT_uncompressed,
	};

static const uint16_t eccurves_default[] =
	{
		23, /* secp256r1 (23) */
		24, /* secp384r1 (24) */
		25, /* secp521r1 (25) */
	};

int tls1_ec_curve_id2nid(uint16_t curve_id)
	{
	/* ECC curves from draft-ietf-tls-ecc-12.txt (Oct. 17, 2005) */
	if (curve_id < 1 || curve_id > sizeof(nid_list)/sizeof(nid_list[0]))
		return OBJ_undef;
	return nid_list[curve_id-1];
	}

uint16_t tls1_ec_nid2curve_id(int nid)
	{
	size_t i;
	for (i = 0; i < sizeof(nid_list)/sizeof(nid_list[0]); i++)
		{
		/* nid_list[i] stores the NID corresponding to curve ID i+1. */
		if (nid == nid_list[i])
			return i + 1;
		}
	/* Use 0 for non-existent curve ID. Note: this assumes that curve ID 0
	 * will never be allocated. */
	return 0;
	}

/* tls1_get_curvelist sets |*out_curve_ids| and |*out_curve_ids_len|
 * to the list of allowed curve IDs. If |get_peer_curves| is non-zero,
 * return the peer's curve list. Otherwise, return the preferred
 * list. */
static void tls1_get_curvelist(SSL *s, int get_peer_curves,
	const uint16_t **out_curve_ids, size_t *out_curve_ids_len)
	{
	if (get_peer_curves)
		{
		*out_curve_ids = s->s3->tmp.peer_ellipticcurvelist;
		*out_curve_ids_len = s->s3->tmp.peer_ellipticcurvelist_length;
		return;
		}

	*out_curve_ids = s->tlsext_ellipticcurvelist;
	*out_curve_ids_len = s->tlsext_ellipticcurvelist_length;
	if (!*out_curve_ids)
		{
		*out_curve_ids = eccurves_default;
		*out_curve_ids_len = sizeof(eccurves_default) / sizeof(eccurves_default[0]);
		}
	}

int tls1_check_curve(SSL *s, CBS *cbs, uint16_t *out_curve_id)
	{
	uint8_t curve_type;
	uint16_t curve_id;
	const uint16_t *curves;
	size_t curves_len, i;

	/* Only support named curves. */
	if (!CBS_get_u8(cbs, &curve_type) ||
		curve_type != NAMED_CURVE_TYPE ||
		!CBS_get_u16(cbs, &curve_id))
		return 0;

	tls1_get_curvelist(s, 0, &curves, &curves_len);
	for (i = 0; i < curves_len; i++)
		{
		if (curve_id == curves[i])
			{
			*out_curve_id = curve_id;
			return 1;
			}
		}
	return 0;
	}

int tls1_get_shared_curve(SSL *s)
	{
	const uint16_t *pref, *supp;
	size_t preflen, supplen, i, j;

	/* Can't do anything on client side */
	if (s->server == 0)
		return NID_undef;

	/* Return first preference shared curve */
	tls1_get_curvelist(s, !!(s->options & SSL_OP_CIPHER_SERVER_PREFERENCE),
				&supp, &supplen);
	tls1_get_curvelist(s, !(s->options & SSL_OP_CIPHER_SERVER_PREFERENCE),
				&pref, &preflen);
	for (i = 0; i < preflen; i++)
		{
		for (j = 0; j < supplen; j++)
			{
			if (pref[i] == supp[j])
				return tls1_ec_curve_id2nid(pref[i]);
			}
		}
	return NID_undef;
	}

/* NOTE: tls1_ec_curve_id2nid and tls1_set_curves assume that
 *
 * (a) 0 is not a valid curve ID.
 *
 * (b) The largest curve ID is 31.
 *
 * Those implementations must be revised before adding support for curve IDs
 * that break these assumptions. */
OPENSSL_COMPILE_ASSERT(
	(sizeof(nid_list) / sizeof(nid_list[0])) < 32, small_curve_ids);

int tls1_set_curves(uint16_t **out_curve_ids, size_t *out_curve_ids_len,
	const int *curves, size_t ncurves)
	{
	uint16_t *curve_ids;
	size_t i;
	/* Bitmap of curves included to detect duplicates: only works
	 * while curve ids < 32 
	 */
	uint32_t dup_list = 0;
	curve_ids = (uint16_t*)OPENSSL_malloc(ncurves * sizeof(uint16_t));
	if (!curve_ids)
		return 0;
	for (i = 0; i < ncurves; i++)
		{
		uint32_t idmask;
		uint16_t id;
		id = tls1_ec_nid2curve_id(curves[i]);
		idmask = ((uint32_t)1) << id;
		if (!id || (dup_list & idmask))
			{
			OPENSSL_free(curve_ids);
			return 0;
			}
		dup_list |= idmask;
		curve_ids[i] = id;
		}
	if (*out_curve_ids)
		OPENSSL_free(*out_curve_ids);
	*out_curve_ids = curve_ids;
	*out_curve_ids_len = ncurves;
	return 1;
	}

/* tls1_curve_params_from_ec_key sets |*out_curve_id| and |*out_comp_id| to the
 * TLS curve ID and point format, respectively, for |ec|. It returns one on
 * success and zero on failure. */
static int tls1_curve_params_from_ec_key(uint16_t *out_curve_id, uint8_t *out_comp_id, EC_KEY *ec)
	{
	int nid;
	uint16_t id;
	const EC_GROUP *grp;
	if (!ec)
		return 0;

	grp = EC_KEY_get0_group(ec);
	if (!grp)
		return 0;

	/* Determine curve ID */
	nid = EC_GROUP_get_curve_name(grp);
	id = tls1_ec_nid2curve_id(nid);
	if (!id)
		return 0;

	/* Set the named curve ID. Arbitrary explicit curves are not
	 * supported. */
	*out_curve_id = id;

	if (out_comp_id)
		{
        	if (EC_KEY_get0_public_key(ec) == NULL)
			return 0;
		if (EC_KEY_get_conv_form(ec) == POINT_CONVERSION_COMPRESSED)
			*out_comp_id = TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime;
		else
			*out_comp_id = TLSEXT_ECPOINTFORMAT_uncompressed;
		}
	return 1;
	}

/* tls1_check_point_format returns one if |comp_id| is consistent with the
 * peer's point format preferences. */
static int tls1_check_point_format(SSL *s, uint8_t comp_id)
	{
	uint8_t *p = s->s3->tmp.peer_ecpointformatlist;
	size_t plen = s->s3->tmp.peer_ecpointformatlist_length;
	size_t i;

	/* If point formats extension present check it, otherwise everything
	 * is supported (see RFC4492). */
	if (p == NULL)
		return 1;

	for (i = 0; i < plen; i++)
		{
		if (comp_id == p[i])
			return 1;
		}
	return 0;
	}

/* tls1_check_curve_id returns one if |curve_id| is consistent with both our and
 * the peer's curve preferences. Note: if called as the client, only our
 * preferences are checked; the peer (the server) does not send preferences. */
static int tls1_check_curve_id(SSL *s, uint16_t curve_id)
	{
	const uint16_t *curves;
	size_t curves_len, i, j;

	/* Check against our list, then the peer's list. */
	for (j = 0; j <= 1; j++)
		{
		tls1_get_curvelist(s, j, &curves, &curves_len);
		for (i = 0; i < curves_len; i++)
			{
			if (curves[i] == curve_id)
				break;
			}
		if (i == curves_len)
			return 0;
		/* Servers do not present a preference list so, if we are a
		 * client, only check our list. */
		if (!s->server)
			return 1;
		}
	return 1;
	}

static void tls1_get_formatlist(SSL *s, const unsigned char **pformats,
					size_t *pformatslen)
	{
	/* If we have a custom point format list use it otherwise
	 * use default */
	if (s->tlsext_ecpointformatlist)
		{
		*pformats = s->tlsext_ecpointformatlist;
		*pformatslen = s->tlsext_ecpointformatlist_length;
		}
	else
		{
		*pformats = ecformats_default;
		*pformatslen = sizeof(ecformats_default);
		}
	}

/* Check cert parameters compatible with extensions: currently just checks
 * EC certificates have compatible curves and compression.
 */
static int tls1_check_cert_param(SSL *s, X509 *x, int set_ee_md)
	{
	uint8_t comp_id;
	uint16_t curve_id;
	EVP_PKEY *pkey;
	int rv;
	pkey = X509_get_pubkey(x);
	if (!pkey)
		return 0;
	/* If not EC nothing to do */
	if (pkey->type != EVP_PKEY_EC)
		{
		EVP_PKEY_free(pkey);
		return 1;
		}
	rv = tls1_curve_params_from_ec_key(&curve_id, &comp_id, pkey->pkey.ec);
	EVP_PKEY_free(pkey);
	if (!rv)
		return 0;
	/* Can't check curve_id for client certs as we don't have a
	 * supported curves extension. */
	if (s->server && !tls1_check_curve_id(s, curve_id))
		return 0;
	return tls1_check_point_format(s, comp_id);
	}

/* Check EC temporary key is compatible with client extensions */
int tls1_check_ec_tmp_key(SSL *s)
	{
	uint16_t curve_id;
	EC_KEY *ec = s->cert->ecdh_tmp;
	if (s->cert->ecdh_tmp_auto)
		{
		/* Need a shared curve */
		return tls1_get_shared_curve(s) != NID_undef;
		}
	if (!ec)
		{
		if (s->cert->ecdh_tmp_cb)
			return 1;
		else
			return 0;
		}
	return tls1_curve_params_from_ec_key(&curve_id, NULL, ec) &&
		tls1_check_curve_id(s, curve_id);
	}



/* List of supported signature algorithms and hashes. Should make this
 * customisable at some point, for now include everything we support.
 */

#define tlsext_sigalg_rsa(md) md, TLSEXT_signature_rsa,

#define tlsext_sigalg_ecdsa(md) md, TLSEXT_signature_ecdsa,

#define tlsext_sigalg(md) \
		tlsext_sigalg_rsa(md) \
		tlsext_sigalg_ecdsa(md)

static const uint8_t tls12_sigalgs[] = {
	tlsext_sigalg(TLSEXT_hash_sha512)
	tlsext_sigalg(TLSEXT_hash_sha384)
	tlsext_sigalg(TLSEXT_hash_sha256)
	tlsext_sigalg(TLSEXT_hash_sha224)
	tlsext_sigalg(TLSEXT_hash_sha1)
};
size_t tls12_get_psigalgs(SSL *s, const unsigned char **psigs)
	{
	/* If server use client authentication sigalgs if not NULL */
	if (s->server && s->cert->client_sigalgs)
		{
		*psigs = s->cert->client_sigalgs;
		return s->cert->client_sigalgslen;
		}
	else if (s->cert->conf_sigalgs)
		{
		*psigs = s->cert->conf_sigalgs;
		return s->cert->conf_sigalgslen;
		}
	else
		{
		*psigs = tls12_sigalgs;
		return sizeof(tls12_sigalgs);
		}
	}

/* tls12_check_peer_sigalg parses a SignatureAndHashAlgorithm out of
 * |cbs|. It checks it is consistent with |s|'s sent supported
 * signature algorithms and, if so, writes the relevant digest into
 * |*out_md| and returns 1. Otherwise it returns 0 and writes an alert
 * into |*out_alert|.
 */
int tls12_check_peer_sigalg(const EVP_MD **out_md, int *out_alert,
	SSL *s, CBS *cbs, EVP_PKEY *pkey)
	{
	const unsigned char *sent_sigs;
	size_t sent_sigslen, i;
	int sigalg = tls12_get_sigid(pkey);
	uint8_t hash, signature;
	/* Should never happen */
	if (sigalg == -1)
		{
		OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, ERR_R_INTERNAL_ERROR);
		*out_alert = SSL_AD_INTERNAL_ERROR;
		return 0;
		}
	if (!CBS_get_u8(cbs, &hash) ||
		!CBS_get_u8(cbs, &signature))
		{
		OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, SSL_R_DECODE_ERROR);
		*out_alert = SSL_AD_DECODE_ERROR;
		return 0;
		}
	/* Check key type is consistent with signature */
	if (sigalg != signature)
		{
		OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, SSL_R_WRONG_SIGNATURE_TYPE);
		*out_alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
		}
	if (pkey->type == EVP_PKEY_EC)
		{
		uint16_t curve_id;
		uint8_t comp_id;
		/* Check compression and curve matches extensions */
		if (!tls1_curve_params_from_ec_key(&curve_id, &comp_id, pkey->pkey.ec))
			{
			*out_alert = SSL_AD_INTERNAL_ERROR;
			return 0;
			}
		if (s->server)
			{
			if (!tls1_check_curve_id(s, curve_id) ||
				!tls1_check_point_format(s, comp_id))
				{
				OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, SSL_R_WRONG_CURVE);
				*out_alert = SSL_AD_ILLEGAL_PARAMETER;
				return 0;
				}
			}
		}

	/* Check signature matches a type we sent */
	sent_sigslen = tls12_get_psigalgs(s, &sent_sigs);
	for (i = 0; i < sent_sigslen; i+=2, sent_sigs+=2)
		{
		if (hash == sent_sigs[0] && signature == sent_sigs[1])
			break;
		}
	/* Allow fallback to SHA1 if not strict mode */
	if (i == sent_sigslen && (hash != TLSEXT_hash_sha1 || s->cert->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT))
		{
		OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, SSL_R_WRONG_SIGNATURE_TYPE);
		*out_alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
		}
	*out_md = tls12_get_hash(hash);
	if (*out_md == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, tls12_check_peer_sigalg, SSL_R_UNKNOWN_DIGEST);
		*out_alert = SSL_AD_ILLEGAL_PARAMETER;
		return 0;
		}
	/* Store the digest used so applications can retrieve it if they
	 * wish.
	 */
	if (s->session && s->session->sess_cert)
		s->session->sess_cert->peer_key->digest = *out_md;
	return 1;
	}
/* Get a mask of disabled algorithms: an algorithm is disabled
 * if it isn't supported or doesn't appear in supported signature
 * algorithms. Unlike ssl_cipher_get_disabled this applies to a specific
 * session and not global settings.
 * 
 */
void ssl_set_client_disabled(SSL *s)
	{
	CERT *c = s->cert;
	const unsigned char *sigalgs;
	size_t i, sigalgslen;
	int have_rsa = 0, have_ecdsa = 0;
	c->mask_a = 0;
	c->mask_k = 0;
	/* Don't allow TLS 1.2 only ciphers if we don't suppport them */
	if (!SSL_CLIENT_USE_TLS1_2_CIPHERS(s))
		c->mask_ssl = SSL_TLSV1_2;
	else
		c->mask_ssl = 0;
	/* Now go through all signature algorithms seeing if we support
	 * any for RSA, DSA, ECDSA. Do this for all versions not just
	 * TLS 1.2.
	 */
	sigalgslen = tls12_get_psigalgs(s, &sigalgs);
	for (i = 0; i < sigalgslen; i += 2, sigalgs += 2)
		{
		switch(sigalgs[1])
			{
		case TLSEXT_signature_rsa:
			have_rsa = 1;
			break;
		case TLSEXT_signature_ecdsa:
			have_ecdsa = 1;
			break;
			}
		}
	/* Disable auth if we don't include any appropriate signature
	 * algorithms.
	 */
	if (!have_rsa)
		{
		c->mask_a |= SSL_aRSA;
		}
	if (!have_ecdsa)
		{
		c->mask_a |= SSL_aECDSA;
		}
	/* with PSK there must be client callback set */
	if (!s->psk_client_callback)
		{
		c->mask_a |= SSL_aPSK;
		c->mask_k |= SSL_kPSK;
		}
	c->valid = 1;
	}

/* header_len is the length of the ClientHello header written so far, used to
 * compute padding. It does not include the record header. Pass 0 if no padding
 * is to be done. */
unsigned char *ssl_add_clienthello_tlsext(SSL *s, unsigned char *buf, unsigned char *limit, size_t header_len)
	{
	int extdatalen=0;
	unsigned char *ret = buf;
	unsigned char *orig = buf;
	/* See if we support any ECC ciphersuites */
	int using_ecc = 0;
	if (s->version >= TLS1_VERSION || SSL_IS_DTLS(s))
		{
		size_t i;
		unsigned long alg_k, alg_a;
		STACK_OF(SSL_CIPHER) *cipher_stack = SSL_get_ciphers(s);

		for (i = 0; i < sk_SSL_CIPHER_num(cipher_stack); i++)
			{
			const SSL_CIPHER *c = sk_SSL_CIPHER_value(cipher_stack, i);

			alg_k = c->algorithm_mkey;
			alg_a = c->algorithm_auth;
			if ((alg_k & SSL_kEECDH) || (alg_a & SSL_aECDSA))
				{
				using_ecc = 1;
				break;
				}
			}
		}

	/* don't add extensions for SSLv3 unless doing secure renegotiation */
	if (s->client_version == SSL3_VERSION
					&& !s->s3->send_connection_binding)
		return orig;

	ret+=2;

	if (ret>=limit) return NULL; /* this really never occurs, but ... */

 	if (s->tlsext_hostname != NULL)
		{ 
		/* Add TLS extension servername to the Client Hello message */
		unsigned long size_str;
		long lenmax; 

		/* check for enough space.
		   4 for the servername type and entension length
		   2 for servernamelist length
		   1 for the hostname type
		   2 for hostname length
		   + hostname length 
		*/
		   
		if ((lenmax = limit - ret - 9) < 0 
		    || (size_str = strlen(s->tlsext_hostname)) > (unsigned long)lenmax) 
			return NULL;
			
		/* extension type and length */
		s2n(TLSEXT_TYPE_server_name,ret); 
		s2n(size_str+5,ret);
		
		/* length of servername list */
		s2n(size_str+3,ret);
	
		/* hostname type, length and hostname */
		*(ret++) = (unsigned char) TLSEXT_NAMETYPE_host_name;
		s2n(size_str,ret);
		memcpy(ret, s->tlsext_hostname, size_str);
		ret+=size_str;
		}

        /* Add RI if renegotiating */
        if (s->renegotiate)
          {
          int el;
          
          if(!ssl_add_clienthello_renegotiate_ext(s, 0, &el, 0))
              {
              OPENSSL_PUT_ERROR(SSL, ssl_add_clienthello_tlsext, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          if((limit - ret - 4 - el) < 0) return NULL;
          
          s2n(TLSEXT_TYPE_renegotiate,ret);
          s2n(el,ret);

          if(!ssl_add_clienthello_renegotiate_ext(s, ret, &el, el))
              {
              OPENSSL_PUT_ERROR(SSL, ssl_add_clienthello_tlsext, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          ret += el;
        }

	/* Add extended master secret. */
	if (s->version != SSL3_VERSION)
		{
		if (limit - ret - 4 < 0)
			return NULL;
		s2n(TLSEXT_TYPE_extended_master_secret,ret);
		s2n(0,ret);
		}

	if (!(SSL_get_options(s) & SSL_OP_NO_TICKET))
		{
		int ticklen;
		if (!s->new_session && s->session && s->session->tlsext_tick)
			ticklen = s->session->tlsext_ticklen;
		else if (s->session && s->tlsext_session_ticket &&
			 s->tlsext_session_ticket->data)
			{
			s->session->tlsext_tick = BUF_memdup(
			       s->tlsext_session_ticket->data,
			       s->tlsext_session_ticket->length);
			if (!s->session->tlsext_tick)
				return NULL;
			ticklen = s->tlsext_session_ticket->length;
			s->session->tlsext_ticklen = ticklen;
			}
		else
			ticklen = 0;
		if (ticklen == 0 && s->tlsext_session_ticket &&
		    s->tlsext_session_ticket->data == NULL)
			goto skip_ext;
		/* Check for enough room 2 for extension type, 2 for len
 		 * rest for ticket
  		 */
		if ((long)(limit - ret - 4 - ticklen) < 0) return NULL;
		s2n(TLSEXT_TYPE_session_ticket,ret); 
		s2n(ticklen,ret);
		if (ticklen)
			{
			memcpy(ret, s->session->tlsext_tick, ticklen);
			ret += ticklen;
			}
		}
		skip_ext:

	if (SSL_USE_SIGALGS(s))
		{
		size_t salglen;
		const unsigned char *salg;
		salglen = tls12_get_psigalgs(s, &salg);
		if ((size_t)(limit - ret) < salglen + 6)
			return NULL; 
		s2n(TLSEXT_TYPE_signature_algorithms,ret);
		s2n(salglen + 2, ret);
		s2n(salglen, ret);
		memcpy(ret, salg, salglen);
		ret += salglen;
		}

	if (s->ocsp_stapling_enabled)
		{
		/* The status_request extension is excessively extensible at
		 * every layer. On the client, only support requesting OCSP
		 * responses with an empty responder_id_list and no
		 * extensions. */
		if (limit - ret - 4 - 1 - 2 - 2 < 0) return NULL;

		s2n(TLSEXT_TYPE_status_request, ret);
		s2n(1 + 2 + 2, ret);
		/* status_type */
		*(ret++) = TLSEXT_STATUSTYPE_ocsp;
		/* responder_id_list - empty */
		s2n(0, ret);
		/* request_extensions - empty */
		s2n(0, ret);
		}

	if (s->ctx->next_proto_select_cb && !s->s3->tmp.finish_md_len)
		{
		/* The client advertises an emtpy extension to indicate its
		 * support for Next Protocol Negotiation */
		if (limit - ret - 4 < 0)
			return NULL;
		s2n(TLSEXT_TYPE_next_proto_neg,ret);
		s2n(0,ret);
		}

	if (s->signed_cert_timestamps_enabled && !s->s3->tmp.finish_md_len)
		{
		/* The client advertises an empty extension to indicate its support for
		 * certificate timestamps. */
		if (limit - ret - 4 < 0)
			return NULL;
		s2n(TLSEXT_TYPE_certificate_timestamp,ret);
		s2n(0,ret);
		}

	if (s->alpn_client_proto_list && !s->s3->tmp.finish_md_len)
		{
		if ((size_t)(limit - ret) < 6 + s->alpn_client_proto_list_len)
			return NULL;
		s2n(TLSEXT_TYPE_application_layer_protocol_negotiation,ret);
		s2n(2 + s->alpn_client_proto_list_len,ret);
		s2n(s->alpn_client_proto_list_len,ret);
		memcpy(ret, s->alpn_client_proto_list,
		       s->alpn_client_proto_list_len);
		ret += s->alpn_client_proto_list_len;
		}

	if (s->tlsext_channel_id_enabled)
		{
		/* The client advertises an emtpy extension to indicate its
		 * support for Channel ID. */
		if (limit - ret - 4 < 0)
			return NULL;
		if (s->ctx->tlsext_channel_id_enabled_new)
			s2n(TLSEXT_TYPE_channel_id_new,ret);
		else
			s2n(TLSEXT_TYPE_channel_id,ret);
		s2n(0,ret);
		}

        if(SSL_get_srtp_profiles(s))
                {
                int el;

                ssl_add_clienthello_use_srtp_ext(s, 0, &el, 0);
                
                if((limit - ret - 4 - el) < 0) return NULL;

                s2n(TLSEXT_TYPE_use_srtp,ret);
                s2n(el,ret);

                if(!ssl_add_clienthello_use_srtp_ext(s, ret, &el, el))
			{
			OPENSSL_PUT_ERROR(SSL, ssl_add_clienthello_tlsext, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
                ret += el;
                }

	if (using_ecc)
		{
		/* Add TLS extension ECPointFormats to the ClientHello message */
		long lenmax; 
		const uint8_t *formats;
		const uint16_t *curves;
		size_t formats_len, curves_len, i;

		tls1_get_formatlist(s, &formats, &formats_len);

		if ((lenmax = limit - ret - 5) < 0) return NULL; 
		if (formats_len > (size_t)lenmax) return NULL;
		if (formats_len > 255)
			{
			OPENSSL_PUT_ERROR(SSL, ssl_add_clienthello_tlsext, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_ec_point_formats,ret);
		s2n(formats_len + 1,ret);
		*(ret++) = (unsigned char)formats_len;
		memcpy(ret, formats, formats_len);
		ret+=formats_len;

		/* Add TLS extension EllipticCurves to the ClientHello message */
		tls1_get_curvelist(s, 0, &curves, &curves_len);

		if ((lenmax = limit - ret - 6) < 0) return NULL; 
		if ((curves_len * 2) > (size_t)lenmax) return NULL;
		if ((curves_len * 2) > 65532)
			{
			OPENSSL_PUT_ERROR(SSL, ssl_add_clienthello_tlsext, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_elliptic_curves,ret);
		s2n((curves_len * 2) + 2, ret);

		/* NB: draft-ietf-tls-ecc-12.txt uses a one-byte prefix for
		 * elliptic_curve_list, but the examples use two bytes.
		 * http://www1.ietf.org/mail-archive/web/tls/current/msg00538.html
		 * resolves this to two bytes.
		 */
		s2n(curves_len * 2, ret);
		for (i = 0; i < curves_len; i++)
			{
			s2n(curves[i], ret);
			}
		}

#ifdef TLSEXT_TYPE_padding
	/* Add padding to workaround bugs in F5 terminators.
	 * See https://tools.ietf.org/html/draft-agl-tls-padding-03
	 *
	 * NB: because this code works out the length of all existing
	 * extensions it MUST always appear last. */
	if (header_len > 0)
		{
		header_len += ret - orig;
		if (header_len > 0xff && header_len < 0x200)
			{
			size_t padding_len = 0x200 - header_len;
			/* Extensions take at least four bytes to encode. Always
			 * include least one byte of data if including the
			 * extension. WebSphere Application Server 7.0 is
			 * intolerant to the last extension being zero-length. */
			if (padding_len >= 4 + 1)
				padding_len -= 4;
			else
				padding_len = 1;
			if (limit - ret - 4 - (long)padding_len < 0)
				return NULL;

			s2n(TLSEXT_TYPE_padding, ret);
			s2n(padding_len, ret);
			memset(ret, 0, padding_len);
			ret += padding_len;
			}
		}
#endif

	if ((extdatalen = ret-orig-2)== 0)
		return orig;

	s2n(extdatalen, orig);
	return ret;
	}

unsigned char *ssl_add_serverhello_tlsext(SSL *s, unsigned char *buf, unsigned char *limit)
	{
	int extdatalen=0;
	unsigned char *orig = buf;
	unsigned char *ret = buf;
	int next_proto_neg_seen;
	unsigned long alg_k = s->s3->tmp.new_cipher->algorithm_mkey;
	unsigned long alg_a = s->s3->tmp.new_cipher->algorithm_auth;
	int using_ecc = (alg_k & SSL_kEECDH) || (alg_a & SSL_aECDSA);
	using_ecc = using_ecc && (s->s3->tmp.peer_ecpointformatlist != NULL);
	/* don't add extensions for SSLv3, unless doing secure renegotiation */
	if (s->version == SSL3_VERSION && !s->s3->send_connection_binding)
		return orig;
	
	ret+=2;
	if (ret>=limit) return NULL; /* this really never occurs, but ... */

	if (!s->hit && s->should_ack_sni && s->session->tlsext_hostname != NULL)
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 

		s2n(TLSEXT_TYPE_server_name,ret);
		s2n(0,ret);
		}

	if(s->s3->send_connection_binding)
        {
          int el;
          
          if(!ssl_add_serverhello_renegotiate_ext(s, 0, &el, 0))
              {
              OPENSSL_PUT_ERROR(SSL, ssl_add_serverhello_tlsext, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          if((limit - ret - 4 - el) < 0) return NULL;
          
          s2n(TLSEXT_TYPE_renegotiate,ret);
          s2n(el,ret);

          if(!ssl_add_serverhello_renegotiate_ext(s, ret, &el, el))
              {
              OPENSSL_PUT_ERROR(SSL, ssl_add_serverhello_tlsext, ERR_R_INTERNAL_ERROR);
              return NULL;
              }

          ret += el;
        }

	if (s->s3->tmp.extended_master_secret)
		{
		if ((long)(limit - ret - 4) < 0) return NULL;

		s2n(TLSEXT_TYPE_extended_master_secret,ret);
		s2n(0,ret);
		}

	if (using_ecc)
		{
		const unsigned char *plist;
		size_t plistlen;
		/* Add TLS extension ECPointFormats to the ServerHello message */
		long lenmax; 

		tls1_get_formatlist(s, &plist, &plistlen);

		if ((lenmax = limit - ret - 5) < 0) return NULL; 
		if (plistlen > (size_t)lenmax) return NULL;
		if (plistlen > 255)
			{
			OPENSSL_PUT_ERROR(SSL, ssl_add_serverhello_tlsext, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
		
		s2n(TLSEXT_TYPE_ec_point_formats,ret);
		s2n(plistlen + 1,ret);
		*(ret++) = (unsigned char) plistlen;
		memcpy(ret, plist, plistlen);
		ret+=plistlen;

		}
	/* Currently the server should not respond with a SupportedCurves extension */

	if (s->tlsext_ticket_expected
		&& !(SSL_get_options(s) & SSL_OP_NO_TICKET)) 
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 
		s2n(TLSEXT_TYPE_session_ticket,ret);
		s2n(0,ret);
		}

	if (s->s3->tmp.certificate_status_expected)
		{ 
		if ((long)(limit - ret - 4) < 0) return NULL; 
		s2n(TLSEXT_TYPE_status_request,ret);
		s2n(0,ret);
		}

        if(s->srtp_profile)
                {
                int el;

                ssl_add_serverhello_use_srtp_ext(s, 0, &el, 0);
                
                if((limit - ret - 4 - el) < 0) return NULL;

                s2n(TLSEXT_TYPE_use_srtp,ret);
                s2n(el,ret);

                if(!ssl_add_serverhello_use_srtp_ext(s, ret, &el, el))
			{
			OPENSSL_PUT_ERROR(SSL, ssl_add_serverhello_tlsext, ERR_R_INTERNAL_ERROR);
			return NULL;
			}
                ret+=el;
                }

	next_proto_neg_seen = s->s3->next_proto_neg_seen;
	s->s3->next_proto_neg_seen = 0;
	if (next_proto_neg_seen && s->ctx->next_protos_advertised_cb)
		{
		const unsigned char *npa;
		unsigned int npalen;
		int r;

		r = s->ctx->next_protos_advertised_cb(s, &npa, &npalen, s->ctx->next_protos_advertised_cb_arg);
		if (r == SSL_TLSEXT_ERR_OK)
			{
			if ((long)(limit - ret - 4 - npalen) < 0) return NULL;
			s2n(TLSEXT_TYPE_next_proto_neg,ret);
			s2n(npalen,ret);
			memcpy(ret, npa, npalen);
			ret += npalen;
			s->s3->next_proto_neg_seen = 1;
			}
		}

	if (s->s3->alpn_selected)
		{
		const uint8_t *selected = s->s3->alpn_selected;
		size_t len = s->s3->alpn_selected_len;

		if ((long)(limit - ret - 4 - 2 - 1 - len) < 0)
			return NULL;
		s2n(TLSEXT_TYPE_application_layer_protocol_negotiation,ret);
		s2n(3 + len,ret);
		s2n(1 + len,ret);
		*ret++ = len;
		memcpy(ret, selected, len);
		ret += len;
		}

	/* If the client advertised support for Channel ID, and we have it
	 * enabled, then we want to echo it back. */
	if (s->s3->tlsext_channel_id_valid)
		{
		if (limit - ret - 4 < 0)
			return NULL;
		if (s->s3->tlsext_channel_id_new)
			s2n(TLSEXT_TYPE_channel_id_new,ret);
		else
			s2n(TLSEXT_TYPE_channel_id,ret);
		s2n(0,ret);
		}

	if ((extdatalen = ret-orig-2) == 0)
		return orig;

	s2n(extdatalen, orig);
	return ret;
	}

/* tls1_alpn_handle_client_hello is called to process the ALPN extension in a
 * ClientHello.
 *   cbs: the contents of the extension, not including the type and length.
 *   out_alert: a pointer to the alert value to send in the event of a zero
 *       return.
 *
 *   returns: 1 on success. */
static int tls1_alpn_handle_client_hello(SSL *s, CBS *cbs, int *out_alert)
	{
	CBS protocol_name_list, protocol_name_list_copy;
	const unsigned char *selected;
	unsigned char selected_len;
	int r;

	if (s->ctx->alpn_select_cb == NULL)
		return 1;

	if (!CBS_get_u16_length_prefixed(cbs, &protocol_name_list) ||
		CBS_len(cbs) != 0 ||
		CBS_len(&protocol_name_list) < 2)
		goto parse_error;

	/* Validate the protocol list. */
	protocol_name_list_copy = protocol_name_list;
	while (CBS_len(&protocol_name_list_copy) > 0)
		{
		CBS protocol_name;

		if (!CBS_get_u8_length_prefixed(&protocol_name_list_copy, &protocol_name))
			goto parse_error;
		}

	r = s->ctx->alpn_select_cb(s, &selected, &selected_len,
		CBS_data(&protocol_name_list), CBS_len(&protocol_name_list),
		s->ctx->alpn_select_cb_arg);
	if (r == SSL_TLSEXT_ERR_OK) {
		if (s->s3->alpn_selected)
			OPENSSL_free(s->s3->alpn_selected);
		s->s3->alpn_selected = BUF_memdup(selected, selected_len);
		if (!s->s3->alpn_selected)
			{
			*out_alert = SSL_AD_INTERNAL_ERROR;
			return 0;
			}
		s->s3->alpn_selected_len = selected_len;
	}
	return 1;

parse_error:
	*out_alert = SSL_AD_DECODE_ERROR;
	return 0;
	}

static int ssl_scan_clienthello_tlsext(SSL *s, CBS *cbs, int *out_alert)
	{	
	int renegotiate_seen = 0;
	CBS extensions;
	size_t i;

	s->should_ack_sni = 0;
	s->s3->next_proto_neg_seen = 0;
	s->s3->tmp.certificate_status_expected = 0;
	s->s3->tmp.extended_master_secret = 0;

	if (s->s3->alpn_selected)
		{
		OPENSSL_free(s->s3->alpn_selected);
		s->s3->alpn_selected = NULL;
		}

	/* Clear any signature algorithms extension received */
	if (s->cert->peer_sigalgs)
		{
		OPENSSL_free(s->cert->peer_sigalgs);
		s->cert->peer_sigalgs = NULL;
		}
	/* Clear any shared signature algorithms */
	if (s->cert->shared_sigalgs)
		{
		OPENSSL_free(s->cert->shared_sigalgs);
		s->cert->shared_sigalgs = NULL;
		}
	/* Clear certificate digests and validity flags */
	for (i = 0; i < SSL_PKEY_NUM; i++)
		{
		s->cert->pkeys[i].digest = NULL;
		s->cert->pkeys[i].valid_flags = 0;
		}
	/* Clear ECC extensions */
	if (s->s3->tmp.peer_ecpointformatlist != 0)
		{
		OPENSSL_free(s->s3->tmp.peer_ecpointformatlist);
		s->s3->tmp.peer_ecpointformatlist = NULL;
		s->s3->tmp.peer_ecpointformatlist_length = 0;
		}
	if (s->s3->tmp.peer_ellipticcurvelist != 0)
		{
		OPENSSL_free(s->s3->tmp.peer_ellipticcurvelist);
		s->s3->tmp.peer_ellipticcurvelist = NULL;
		s->s3->tmp.peer_ellipticcurvelist_length = 0;
		}

	/* There may be no extensions. */
	if (CBS_len(cbs) == 0)
		{
		goto ri_check;
		}

	/* Decode the extensions block and check it is valid. */
	if (!CBS_get_u16_length_prefixed(cbs, &extensions) ||
		!tls1_check_duplicate_extensions(&extensions))
		{
		*out_alert = SSL_AD_DECODE_ERROR;
		return 0;
		}

	while (CBS_len(&extensions) != 0)
		{
		uint16_t type;
		CBS extension;

		/* Decode the next extension. */
		if (!CBS_get_u16(&extensions, &type) ||
			!CBS_get_u16_length_prefixed(&extensions, &extension))
			{
			*out_alert = SSL_AD_DECODE_ERROR;
			return 0;
			}

		if (s->tlsext_debug_cb)
			{
			s->tlsext_debug_cb(s, 0, type, (unsigned char*)CBS_data(&extension),
				CBS_len(&extension), s->tlsext_debug_arg);
			}

/* The servername extension is treated as follows:

   - Only the hostname type is supported with a maximum length of 255.
   - The servername is rejected if too long or if it contains zeros,
     in which case an fatal alert is generated.
   - The servername field is maintained together with the session cache.
   - When a session is resumed, the servername call back invoked in order
     to allow the application to position itself to the right context. 
   - The servername is acknowledged if it is new for a session or when 
     it is identical to a previously used for the same session. 
     Applications can control the behaviour.  They can at any time
     set a 'desirable' servername for a new SSL object. This can be the
     case for example with HTTPS when a Host: header field is received and
     a renegotiation is requested. In this case, a possible servername
     presented in the new client hello is only acknowledged if it matches
     the value of the Host: field. 
   - Applications must  use SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
     if they provide for changing an explicit servername context for the session,
     i.e. when the session has been established with a servername extension. 
   - On session reconnect, the servername extension may be absent. 

*/      

		if (type == TLSEXT_TYPE_server_name)
			{
			CBS server_name_list;
			char have_seen_host_name = 0;

			if (!CBS_get_u16_length_prefixed(&extension, &server_name_list) ||
				CBS_len(&server_name_list) < 1 ||
				CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			/* Decode each ServerName in the extension. */
			while (CBS_len(&server_name_list) > 0)
				{
				uint8_t name_type;
				CBS host_name;

				/* Decode the NameType. */
				if (!CBS_get_u8(&server_name_list, &name_type))
					{
					*out_alert = SSL_AD_DECODE_ERROR;
					return 0;
					}

				/* Only host_name is supported. */
				if (name_type != TLSEXT_NAMETYPE_host_name)
					continue;

				if (have_seen_host_name)
					{
					/* The ServerNameList MUST NOT contain
					 * more than one name of the same
					 * name_type. */
					*out_alert = SSL_AD_DECODE_ERROR;
					return 0;
					}

				have_seen_host_name = 1;

				if (!CBS_get_u16_length_prefixed(&server_name_list, &host_name) ||
					CBS_len(&host_name) < 1)
					{
					*out_alert = SSL_AD_DECODE_ERROR;
					return 0;
					}

				if (CBS_len(&host_name) > TLSEXT_MAXLEN_host_name ||
					CBS_contains_zero_byte(&host_name))
					{
					*out_alert = SSL_AD_UNRECOGNIZED_NAME;
					return 0;
					}

				if (!s->hit)
					{
					assert(s->session->tlsext_hostname == NULL);
					if (s->session->tlsext_hostname)
						{
						/* This should be impossible. */
						*out_alert = SSL_AD_DECODE_ERROR;
						return 0;
						}

					/* Copy the hostname as a string. */
					if (!CBS_strdup(&host_name, &s->session->tlsext_hostname))
						{
						*out_alert = SSL_AD_INTERNAL_ERROR;
						return 0;
						}

					s->should_ack_sni = 1;
					}
				}
			}

		else if (type == TLSEXT_TYPE_ec_point_formats)
			{
			CBS ec_point_format_list;

			if (!CBS_get_u8_length_prefixed(&extension, &ec_point_format_list) ||
				CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (!CBS_stow(&ec_point_format_list,
					&s->s3->tmp.peer_ecpointformatlist,
					&s->s3->tmp.peer_ecpointformatlist_length))
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}
			}
		else if (type == TLSEXT_TYPE_elliptic_curves)
			{
			CBS elliptic_curve_list;
			size_t i, num_curves;

			if (!CBS_get_u16_length_prefixed(&extension, &elliptic_curve_list) ||
				CBS_len(&elliptic_curve_list) == 0 ||
				(CBS_len(&elliptic_curve_list) & 1) != 0 ||
				CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (s->s3->tmp.peer_ellipticcurvelist)
				{
				OPENSSL_free(s->s3->tmp.peer_ellipticcurvelist);
				s->s3->tmp.peer_ellipticcurvelist_length = 0;
				}
			s->s3->tmp.peer_ellipticcurvelist =
				(uint16_t*)OPENSSL_malloc(CBS_len(&elliptic_curve_list));
			if (s->s3->tmp.peer_ellipticcurvelist == NULL)
					{
					*out_alert = SSL_AD_INTERNAL_ERROR;
					return 0;
					}
			num_curves = CBS_len(&elliptic_curve_list) / 2;
			for (i = 0; i < num_curves; i++)
				{
				if (!CBS_get_u16(&elliptic_curve_list,
						&s->s3->tmp.peer_ellipticcurvelist[i]))
					{
					*out_alert = SSL_AD_INTERNAL_ERROR;
					return 0;
					}
				}
			if (CBS_len(&elliptic_curve_list) != 0)
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}
			s->s3->tmp.peer_ellipticcurvelist_length = num_curves;
			}
		else if (type == TLSEXT_TYPE_session_ticket)
			{
			if (s->tls_session_ticket_ext_cb &&
				!s->tls_session_ticket_ext_cb(s, CBS_data(&extension), CBS_len(&extension), s->tls_session_ticket_ext_cb_arg))
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}
			}
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if (!ssl_parse_clienthello_renegotiate_ext(s, &extension, out_alert))
				return 0;
			renegotiate_seen = 1;
			}
		else if (type == TLSEXT_TYPE_signature_algorithms)
			{
			CBS supported_signature_algorithms;

			if (!CBS_get_u16_length_prefixed(&extension, &supported_signature_algorithms) ||
				CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			/* Ensure the signature algorithms are non-empty. It
			 * contains a list of SignatureAndHashAlgorithms
			 * which are two bytes each. */
			if (CBS_len(&supported_signature_algorithms) == 0 ||
				(CBS_len(&supported_signature_algorithms) % 2) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (!tls1_process_sigalgs(s, &supported_signature_algorithms))
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			/* If sigalgs received and no shared algorithms fatal
			 * error.
			 */
			if (s->cert->peer_sigalgs && !s->cert->shared_sigalgs)
				{
				OPENSSL_PUT_ERROR(SSL, ssl_add_serverhello_tlsext, SSL_R_NO_SHARED_SIGATURE_ALGORITHMS);
				*out_alert = SSL_AD_ILLEGAL_PARAMETER;
				return 0;
				}
			}

		else if (type == TLSEXT_TYPE_next_proto_neg &&
			 s->s3->tmp.finish_md_len == 0 &&
			 s->s3->alpn_selected == NULL)
			{
			/* The extension must be empty. */
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			/* We shouldn't accept this extension on a
			 * renegotiation.
			 *
			 * s->new_session will be set on renegotiation, but we
			 * probably shouldn't rely that it couldn't be set on
			 * the initial renegotation too in certain cases (when
			 * there's some other reason to disallow resuming an
			 * earlier session -- the current code won't be doing
			 * anything like that, but this might change).

			 * A valid sign that there's been a previous handshake
			 * in this connection is if s->s3->tmp.finish_md_len >
			 * 0.  (We are talking about a check that will happen
			 * in the Hello protocol round, well before a new
			 * Finished message could have been computed.) */
			s->s3->next_proto_neg_seen = 1;
			}

		else if (type == TLSEXT_TYPE_application_layer_protocol_negotiation &&
			 s->ctx->alpn_select_cb &&
			 s->s3->tmp.finish_md_len == 0)
			{
			if (!tls1_alpn_handle_client_hello(s, &extension, out_alert))
				return 0;
			/* ALPN takes precedence over NPN. */
			s->s3->next_proto_neg_seen = 0;
			}

		else if (type == TLSEXT_TYPE_channel_id &&
			 s->tlsext_channel_id_enabled)
			{
			/* The extension must be empty. */
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			s->s3->tlsext_channel_id_valid = 1;
			}

		else if (type == TLSEXT_TYPE_channel_id_new &&
			 s->tlsext_channel_id_enabled)
			{
			/* The extension must be empty. */
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			s->s3->tlsext_channel_id_valid = 1;
			s->s3->tlsext_channel_id_new = 1;
			}


		/* session ticket processed earlier */
		else if (type == TLSEXT_TYPE_use_srtp)
                        {
			if (!ssl_parse_clienthello_use_srtp_ext(s, &extension, out_alert))
				return 0;
                        }

		else if (type == TLSEXT_TYPE_extended_master_secret &&
			 s->version != SSL3_VERSION)
			{
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			s->s3->tmp.extended_master_secret = 1;
			}
		}

	ri_check:

	/* Need RI if renegotiating */

	if (!renegotiate_seen && s->renegotiate &&
		!(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION))
		{
		*out_alert = SSL_AD_HANDSHAKE_FAILURE;
		OPENSSL_PUT_ERROR(SSL, ssl_scan_clienthello_tlsext, SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		return 0;
		}
	/* If no signature algorithms extension set default values */
	if (!s->cert->peer_sigalgs)
		ssl_cert_set_default_md(s->cert);

	return 1;
	}

int ssl_parse_clienthello_tlsext(SSL *s, CBS *cbs)
	{
	int alert = -1;
	if (ssl_scan_clienthello_tlsext(s, cbs, &alert) <= 0)
		{
		ssl3_send_alert(s, SSL3_AL_FATAL, alert);
		return 0;
		}

	if (ssl_check_clienthello_tlsext(s) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_parse_clienthello_tlsext, SSL_R_CLIENTHELLO_TLSEXT);
		return 0;
		}
	return 1;
	}

/* ssl_next_proto_validate validates a Next Protocol Negotiation block. No
 * elements of zero length are allowed and the set of elements must exactly fill
 * the length of the block. */
static char ssl_next_proto_validate(const CBS *cbs)
	{
	CBS copy = *cbs;

	while (CBS_len(&copy) != 0)
		{
		CBS proto;
		if (!CBS_get_u8_length_prefixed(&copy, &proto) ||
			CBS_len(&proto) == 0)
			{
			return 0;
			}
		}
	return 1;
	}

static int ssl_scan_serverhello_tlsext(SSL *s, CBS *cbs, int *out_alert)
	{
	int tlsext_servername = 0;
	int renegotiate_seen = 0;
	CBS extensions;

	/* TODO(davidben): Move all of these to some per-handshake state that
	 * gets systematically reset on a new handshake; perhaps allocate it
	 * fresh each time so it's not even kept around post-handshake. */
	s->s3->next_proto_neg_seen = 0;

	s->tlsext_ticket_expected = 0;
	s->s3->tmp.certificate_status_expected = 0;
	s->s3->tmp.extended_master_secret = 0;

	if (s->s3->alpn_selected)
		{
		OPENSSL_free(s->s3->alpn_selected);
		s->s3->alpn_selected = NULL;
		}

	/* Clear ECC extensions */
	if (s->s3->tmp.peer_ecpointformatlist != 0)
		{
		OPENSSL_free(s->s3->tmp.peer_ecpointformatlist);
		s->s3->tmp.peer_ecpointformatlist = NULL;
		s->s3->tmp.peer_ecpointformatlist_length = 0;
		}

	/* There may be no extensions. */
	if (CBS_len(cbs) == 0)
		{
		goto ri_check;
		}

	/* Decode the extensions block and check it is valid. */
	if (!CBS_get_u16_length_prefixed(cbs, &extensions) ||
		!tls1_check_duplicate_extensions(&extensions))
		{
		*out_alert = SSL_AD_DECODE_ERROR;
		return 0;
		}

	while (CBS_len(&extensions) != 0)
		{
		uint16_t type;
		CBS extension;

		/* Decode the next extension. */
		if (!CBS_get_u16(&extensions, &type) ||
			!CBS_get_u16_length_prefixed(&extensions, &extension))
			{
			*out_alert = SSL_AD_DECODE_ERROR;
			return 0;
			}

		if (s->tlsext_debug_cb)
			{
			s->tlsext_debug_cb(s, 1, type, (unsigned char*)CBS_data(&extension),
				CBS_len(&extension), s->tlsext_debug_arg);
			}

		if (type == TLSEXT_TYPE_server_name)
			{
			/* The extension must be empty. */
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			/* We must have sent it in ClientHello. */
			if (s->tlsext_hostname == NULL)
				{
				*out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			tlsext_servername = 1;
			}
		else if (type == TLSEXT_TYPE_ec_point_formats)
			{
			CBS ec_point_format_list;

			if (!CBS_get_u8_length_prefixed(&extension, &ec_point_format_list) ||
				CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (!CBS_stow(&ec_point_format_list,
					&s->s3->tmp.peer_ecpointformatlist,
					&s->s3->tmp.peer_ecpointformatlist_length))
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}
			}
		else if (type == TLSEXT_TYPE_session_ticket)
			{
			if (s->tls_session_ticket_ext_cb &&
				!s->tls_session_ticket_ext_cb(s, CBS_data(&extension), CBS_len(&extension),
                                        s->tls_session_ticket_ext_cb_arg))
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}

			if ((SSL_get_options(s) & SSL_OP_NO_TICKET) || CBS_len(&extension) > 0)
				{
				*out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}

			s->tlsext_ticket_expected = 1;
			}
		else if (type == TLSEXT_TYPE_status_request)
			{
			/* The extension MUST be empty and may only sent if
			 * we've requested a status request message. */
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			if (!s->ocsp_stapling_enabled)
				{
				*out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}
			/* Set a flag to expect a CertificateStatus message */
			s->s3->tmp.certificate_status_expected = 1;
			}
		else if (type == TLSEXT_TYPE_next_proto_neg && s->s3->tmp.finish_md_len == 0) {
		unsigned char *selected;
		unsigned char selected_len;

		/* We must have requested it. */
		if (s->ctx->next_proto_select_cb == NULL)
			{
			*out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
			return 0;
			}

		/* The data must be valid. */
		if (!ssl_next_proto_validate(&extension))
			{
			*out_alert = SSL_AD_DECODE_ERROR;
			return 0;
			}

		if (s->ctx->next_proto_select_cb(s, &selected, &selected_len,
				CBS_data(&extension), CBS_len(&extension),
				s->ctx->next_proto_select_cb_arg) != SSL_TLSEXT_ERR_OK)
			{
			*out_alert = SSL_AD_INTERNAL_ERROR;
			return 0;
			}

		s->next_proto_negotiated = BUF_memdup(selected, selected_len);
		if (s->next_proto_negotiated == NULL)
			{
			*out_alert = SSL_AD_INTERNAL_ERROR;
			return 0;
			}
		s->next_proto_negotiated_len = selected_len;
		s->s3->next_proto_neg_seen = 1;
		}
		else if (type == TLSEXT_TYPE_application_layer_protocol_negotiation)
			{
			CBS protocol_name_list, protocol_name;

			/* We must have requested it. */
			if (s->alpn_client_proto_list == NULL)
				{
				*out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
				return 0;
				}

			/* The extension data consists of a ProtocolNameList
			 * which must have exactly one ProtocolName. Each of
			 * these is length-prefixed. */
			if (!CBS_get_u16_length_prefixed(&extension, &protocol_name_list) ||
				CBS_len(&extension) != 0 ||
				!CBS_get_u8_length_prefixed(&protocol_name_list, &protocol_name) ||
				CBS_len(&protocol_name_list) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			if (!CBS_stow(&protocol_name,
					&s->s3->alpn_selected,
					&s->s3->alpn_selected_len))
				{
				*out_alert = SSL_AD_INTERNAL_ERROR;
				return 0;
				}
			}

		else if (type == TLSEXT_TYPE_channel_id)
			{
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			s->s3->tlsext_channel_id_valid = 1;
			}
		else if (type == TLSEXT_TYPE_channel_id_new)
			{
			if (CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			s->s3->tlsext_channel_id_valid = 1;
			s->s3->tlsext_channel_id_new = 1;
			}
		else if (type == TLSEXT_TYPE_certificate_timestamp)
			{
			if (CBS_len(&extension) == 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			/* Session resumption uses the original session information. */
			if (!s->hit)
				{
				if (!CBS_stow(&extension,
					&s->session->tlsext_signed_cert_timestamp_list,
					&s->session->tlsext_signed_cert_timestamp_list_length))
					{
					*out_alert = SSL_AD_INTERNAL_ERROR;
					return 0;
					}
				}
			}
		else if (type == TLSEXT_TYPE_renegotiate)
			{
			if (!ssl_parse_serverhello_renegotiate_ext(s, &extension, out_alert))
				return 0;
			renegotiate_seen = 1;
			}
		else if (type == TLSEXT_TYPE_use_srtp)
                        {
                        if (!ssl_parse_serverhello_use_srtp_ext(s, &extension, out_alert))
                                return 0;
                        }

		else if (type == TLSEXT_TYPE_extended_master_secret)
			{
			if (/* It is invalid for the server to select EMS and
			       SSLv3. */
			    s->version == SSL3_VERSION ||
			    CBS_len(&extension) != 0)
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}

			s->s3->tmp.extended_master_secret = 1;
			}
		}

	if (!s->hit && tlsext_servername == 1)
		{
 		if (s->tlsext_hostname)
			{
			if (s->session->tlsext_hostname == NULL)
				{
				s->session->tlsext_hostname = BUF_strdup(s->tlsext_hostname);	
				if (!s->session->tlsext_hostname)
					{
					*out_alert = SSL_AD_UNRECOGNIZED_NAME;
					return 0;
					}
				}
			else 
				{
				*out_alert = SSL_AD_DECODE_ERROR;
				return 0;
				}
			}
		}

	ri_check:

	/* Determine if we need to see RI. Strictly speaking if we want to
	 * avoid an attack we should *always* see RI even on initial server
	 * hello because the client doesn't see any renegotiation during an
	 * attack. However this would mean we could not connect to any server
	 * which doesn't support RI so for the immediate future tolerate RI
	 * absence on initial connect only.
	 */
	if (!renegotiate_seen
		&& !(s->options & SSL_OP_LEGACY_SERVER_CONNECT)
		&& !(s->options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION))
		{
		*out_alert = SSL_AD_HANDSHAKE_FAILURE;
		OPENSSL_PUT_ERROR(SSL, ssl_scan_serverhello_tlsext, SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		return 0;
		}

	return 1;
	}


int ssl_prepare_clienthello_tlsext(SSL *s)
	{
	return 1;
	}

int ssl_prepare_serverhello_tlsext(SSL *s)
	{
	return 1;
	}

static int ssl_check_clienthello_tlsext(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	/* The handling of the ECPointFormats extension is done elsewhere, namely in 
	 * ssl3_choose_cipher in s3_lib.c.
	 */
	/* The handling of the EllipticCurves extension is done elsewhere, namely in 
	 * ssl3_choose_cipher in s3_lib.c.
	 */

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

	switch (ret)
		{
		case SSL_TLSEXT_ERR_ALERT_FATAL:
			ssl3_send_alert(s,SSL3_AL_FATAL,al); 
			return -1;

		case SSL_TLSEXT_ERR_ALERT_WARNING:
			ssl3_send_alert(s,SSL3_AL_WARNING,al);
			return 1;

		case SSL_TLSEXT_ERR_NOACK:
			s->should_ack_sni = 0;
			return 1;

		default:
			return 1;
		}
	}

static int ssl_check_serverhello_tlsext(SSL *s)
	{
	int ret=SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	/* If we are client and using an elliptic curve cryptography cipher
	 * suite, then if server returns an EC point formats lists extension
	 * it must contain uncompressed.
	 */
	unsigned long alg_k = s->s3->tmp.new_cipher->algorithm_mkey;
	unsigned long alg_a = s->s3->tmp.new_cipher->algorithm_auth;
	if (((alg_k & SSL_kEECDH) || (alg_a & SSL_aECDSA)) &&
		!tls1_check_point_format(s, TLSEXT_ECPOINTFORMAT_uncompressed))
		{
		OPENSSL_PUT_ERROR(SSL, ssl_check_serverhello_tlsext, SSL_R_TLS_INVALID_ECPOINTFORMAT_LIST);
		return -1;
		}
	ret = SSL_TLSEXT_ERR_OK;

	if (s->ctx != NULL && s->ctx->tlsext_servername_callback != 0) 
		ret = s->ctx->tlsext_servername_callback(s, &al, s->ctx->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->tlsext_servername_callback != 0) 		
		ret = s->initial_ctx->tlsext_servername_callback(s, &al, s->initial_ctx->tlsext_servername_arg);

	switch (ret)
		{
		case SSL_TLSEXT_ERR_ALERT_FATAL:
			ssl3_send_alert(s,SSL3_AL_FATAL,al);
			return -1;

		case SSL_TLSEXT_ERR_ALERT_WARNING:
			ssl3_send_alert(s,SSL3_AL_WARNING,al);
			return 1;

		default:
			return 1;
		}
	}

int ssl_parse_serverhello_tlsext(SSL *s, CBS *cbs)
	{
	int alert = -1;
	if (s->version < SSL3_VERSION)
		return 1;

	if (ssl_scan_serverhello_tlsext(s, cbs, &alert) <= 0)
		{
		ssl3_send_alert(s, SSL3_AL_FATAL, alert);
		return 0;
		}

	if (ssl_check_serverhello_tlsext(s) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_parse_serverhello_tlsext, SSL_R_SERVERHELLO_TLSEXT);
		return 0;
		}

	return 1;
	}

/* Since the server cache lookup is done early on in the processing of the
 * ClientHello, and other operations depend on the result, we need to handle
 * any TLS session ticket extension at the same time.
 *
 *   ctx: contains the early callback context, which is the result of a
 *       shallow parse of the ClientHello.
 *   ret: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * If s->tls_session_secret_cb is set then we are expecting a pre-shared key
 * ciphersuite, in which case we have no use for session tickets and one will
 * never be decrypted, nor will s->tlsext_ticket_expected be set to 1.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    0: no ticket was found (or was ignored, based on settings).
 *    1: a zero length extension was found, indicating that the client supports
 *       session tickets but doesn't currently have one to offer.
 *    2: either s->tls_session_secret_cb was set, or a ticket was offered but
 *       couldn't be decrypted because of a non-fatal error.
 *    3: a ticket was successfully decrypted and *ret was set.
 *
 * Side effects:
 *   Sets s->tlsext_ticket_expected to 1 if the server will have to issue
 *   a new session ticket to the client because the client indicated support
 *   (and s->tls_session_secret_cb is NULL) but the client either doesn't have
 *   a session ticket or we couldn't use the one it gave us, or if
 *   s->ctx->tlsext_ticket_key_cb asked to renew the client's ticket.
 *   Otherwise, s->tlsext_ticket_expected is set to 0.
 */
int tls1_process_ticket(SSL *s, const struct ssl_early_callback_ctx *ctx,
			SSL_SESSION **ret)
	{
	*ret = NULL;
	s->tlsext_ticket_expected = 0;
	const unsigned char *data;
	size_t len;
	int r;

	/* If tickets disabled behave as if no ticket present
	 * to permit stateful resumption.
	 */
	if (SSL_get_options(s) & SSL_OP_NO_TICKET)
		return 0;
	if ((s->version <= SSL3_VERSION) && !ctx->extensions)
		return 0;
	if (!SSL_early_callback_ctx_extension_get(
		ctx, TLSEXT_TYPE_session_ticket, &data, &len))
		{
		return 0;
		}
	if (len == 0)
		{
		/* The client will accept a ticket but doesn't
		 * currently have one. */
		s->tlsext_ticket_expected = 1;
		return 1;
		}
	if (s->tls_session_secret_cb)
		{
		/* Indicate that the ticket couldn't be
		 * decrypted rather than generating the session
		 * from ticket now, trigger abbreviated
		 * handshake based on external mechanism to
		 * calculate the master secret later. */
		return 2;
		}
	r = tls_decrypt_ticket(s, data, len, ctx->session_id,
			       ctx->session_id_len, ret);
	switch (r)
		{
		case 2: /* ticket couldn't be decrypted */
			s->tlsext_ticket_expected = 1;
			return 2;
		case 3: /* ticket was decrypted */
			return r;
		case 4: /* ticket decrypted but need to renew */
			s->tlsext_ticket_expected = 1;
			return 3;
		default: /* fatal error */
			return -1;
		}
	}

/* tls_decrypt_ticket attempts to decrypt a session ticket.
 *
 *   etick: points to the body of the session ticket extension.
 *   eticklen: the length of the session tickets extenion.
 *   sess_id: points at the session ID.
 *   sesslen: the length of the session ID.
 *   psess: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    2: the ticket couldn't be decrypted.
 *    3: a ticket was successfully decrypted and *psess was set.
 *    4: same as 3, but the ticket needs to be renewed.
 */
static int tls_decrypt_ticket(SSL *s, const unsigned char *etick, int eticklen,
				const unsigned char *sess_id, int sesslen,
				SSL_SESSION **psess)
	{
	SSL_SESSION *sess;
	unsigned char *sdec;
	const unsigned char *p;
	int slen, mlen, renew_ticket = 0;
	unsigned char tick_hmac[EVP_MAX_MD_SIZE];
	HMAC_CTX hctx;
	EVP_CIPHER_CTX ctx;
	SSL_CTX *tctx = s->initial_ctx;
	/* Need at least keyname + iv + some encrypted data */
	if (eticklen < 48)
		return 2;
	/* Initialize session ticket encryption and HMAC contexts */
	HMAC_CTX_init(&hctx);
	EVP_CIPHER_CTX_init(&ctx);
	if (tctx->tlsext_ticket_key_cb)
		{
		unsigned char *nctick = (unsigned char *)etick;
		int rv = tctx->tlsext_ticket_key_cb(s, nctick, nctick + 16,
							&ctx, &hctx, 0);
		if (rv < 0)
			return -1;
		if (rv == 0)
			return 2;
		if (rv == 2)
			renew_ticket = 1;
		}
	else
		{
		/* Check key name matches */
		if (memcmp(etick, tctx->tlsext_tick_key_name, 16))
			return 2;
		HMAC_Init_ex(&hctx, tctx->tlsext_tick_hmac_key, 16,
					tlsext_tick_md(), NULL);
		EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL,
				tctx->tlsext_tick_aes_key, etick + 16);
		}
	/* Attempt to process session ticket, first conduct sanity and
	 * integrity checks on ticket.
	 */
	mlen = HMAC_size(&hctx);
	if (mlen < 0)
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return -1;
		}
	eticklen -= mlen;
	/* Check HMAC of encrypted ticket */
	HMAC_Update(&hctx, etick, eticklen);
	HMAC_Final(&hctx, tick_hmac, NULL);
	HMAC_CTX_cleanup(&hctx);
	if (CRYPTO_memcmp(tick_hmac, etick + eticklen, mlen))
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return 2;
		}
	/* Attempt to decrypt session data */
	/* Move p after IV to start of encrypted ticket, update length */
	p = etick + 16 + EVP_CIPHER_CTX_iv_length(&ctx);
	eticklen -= 16 + EVP_CIPHER_CTX_iv_length(&ctx);
	sdec = OPENSSL_malloc(eticklen);
	if (!sdec)
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return -1;
		}
	EVP_DecryptUpdate(&ctx, sdec, &slen, p, eticklen);
	if (EVP_DecryptFinal_ex(&ctx, sdec + slen, &mlen) <= 0)
		{
		EVP_CIPHER_CTX_cleanup(&ctx);
		OPENSSL_free(sdec);
		return 2;
		}
	slen += mlen;
	EVP_CIPHER_CTX_cleanup(&ctx);
	p = sdec;

	sess = d2i_SSL_SESSION(NULL, &p, slen);
	OPENSSL_free(sdec);
	if (sess)
		{
		/* The session ID, if non-empty, is used by some clients to
		 * detect that the ticket has been accepted. So we copy it to
		 * the session structure. If it is empty set length to zero
		 * as required by standard.
		 */
		if (sesslen)
			memcpy(sess->session_id, sess_id, sesslen);
		sess->session_id_length = sesslen;
		*psess = sess;
		if (renew_ticket)
			return 4;
		else
			return 3;
		}
        ERR_clear_error();
	/* For session parse failure, indicate that we need to send a new
	 * ticket. */
	return 2;
	}

/* Tables to translate from NIDs to TLS v1.2 ids */

typedef struct 
	{
	int nid;
	int id;
	} tls12_lookup;

static const tls12_lookup tls12_md[] = {
	{NID_md5, TLSEXT_hash_md5},
	{NID_sha1, TLSEXT_hash_sha1},
	{NID_sha224, TLSEXT_hash_sha224},
	{NID_sha256, TLSEXT_hash_sha256},
	{NID_sha384, TLSEXT_hash_sha384},
	{NID_sha512, TLSEXT_hash_sha512}
};

static const tls12_lookup tls12_sig[] = {
	{EVP_PKEY_RSA, TLSEXT_signature_rsa},
	{EVP_PKEY_EC, TLSEXT_signature_ecdsa}
};

static int tls12_find_id(int nid, const tls12_lookup *table, size_t tlen)
	{
	size_t i;
	for (i = 0; i < tlen; i++)
		{
		if (table[i].nid == nid)
			return table[i].id;
		}
	return -1;
	}

static int tls12_find_nid(int id, const tls12_lookup *table, size_t tlen)
	{
	size_t i;
	for (i = 0; i < tlen; i++)
		{
		if ((table[i].id) == id)
			return table[i].nid;
		}
	return NID_undef;
	}

int tls12_get_sigandhash(unsigned char *p, const EVP_PKEY *pk, const EVP_MD *md)
	{
	int sig_id, md_id;
	if (!md)
		return 0;
	md_id = tls12_find_id(EVP_MD_type(md), tls12_md,
				sizeof(tls12_md)/sizeof(tls12_lookup));
	if (md_id == -1)
		return 0;
	sig_id = tls12_get_sigid(pk);
	if (sig_id == -1)
		return 0;
	p[0] = (unsigned char)md_id;
	p[1] = (unsigned char)sig_id;
	return 1;
	}

int tls12_get_sigid(const EVP_PKEY *pk)
	{
	return tls12_find_id(pk->type, tls12_sig,
				sizeof(tls12_sig)/sizeof(tls12_lookup));
	}

const EVP_MD *tls12_get_hash(unsigned char hash_alg)
	{
	switch(hash_alg)
		{
		case TLSEXT_hash_md5:
		return EVP_md5();
		case TLSEXT_hash_sha1:
		return EVP_sha1();
		case TLSEXT_hash_sha224:
		return EVP_sha224();

		case TLSEXT_hash_sha256:
		return EVP_sha256();
		case TLSEXT_hash_sha384:
		return EVP_sha384();

		case TLSEXT_hash_sha512:
		return EVP_sha512();
		default:
		return NULL;

		}
	}

static int tls12_get_pkey_idx(unsigned char sig_alg)
	{
	switch(sig_alg)
		{
	case TLSEXT_signature_rsa:
		return SSL_PKEY_RSA_SIGN;
	case TLSEXT_signature_ecdsa:
		return SSL_PKEY_ECC;
		}
	return -1;
	}

/* Convert TLS 1.2 signature algorithm extension values into NIDs */
static void tls1_lookup_sigalg(int *phash_nid, int *psign_nid,
			int *psignhash_nid, const unsigned char *data)
	{
	int sign_nid = 0, hash_nid = 0;
	if (!phash_nid && !psign_nid && !psignhash_nid)
		return;
	if (phash_nid || psignhash_nid)
		{
		hash_nid = tls12_find_nid(data[0], tls12_md,
					sizeof(tls12_md)/sizeof(tls12_lookup));
		if (phash_nid)
			*phash_nid = hash_nid;
		}
	if (psign_nid || psignhash_nid)
		{
		sign_nid = tls12_find_nid(data[1], tls12_sig,
					sizeof(tls12_sig)/sizeof(tls12_lookup));
		if (psign_nid)
			*psign_nid = sign_nid;
		}
	if (psignhash_nid)
		{
		if (sign_nid && hash_nid)
			OBJ_find_sigid_by_algs(psignhash_nid,
							hash_nid, sign_nid);
		else
			*psignhash_nid = NID_undef;
		}
	}
/* Given preference and allowed sigalgs set shared sigalgs */
static int tls12_do_shared_sigalgs(TLS_SIGALGS *shsig,
				const unsigned char *pref, size_t preflen,
				const unsigned char *allow, size_t allowlen)
	{
	const unsigned char *ptmp, *atmp;
	size_t i, j, nmatch = 0;
	for (i = 0, ptmp = pref; i < preflen; i+=2, ptmp+=2)
		{
		/* Skip disabled hashes or signature algorithms */
		if (tls12_get_hash(ptmp[0]) == NULL)
			continue;
		if (tls12_get_pkey_idx(ptmp[1]) == -1)
			continue;
		for (j = 0, atmp = allow; j < allowlen; j+=2, atmp+=2)
			{
			if (ptmp[0] == atmp[0] && ptmp[1] == atmp[1])
				{
				nmatch++;
				if (shsig)
					{
					shsig->rhash = ptmp[0];
					shsig->rsign = ptmp[1];
					tls1_lookup_sigalg(&shsig->hash_nid,
						&shsig->sign_nid,
						&shsig->signandhash_nid,
						ptmp);
					shsig++;
					}
				break;
				}
			}
		}
	return nmatch;
	}

/* Set shared signature algorithms for SSL structures */
static int tls1_set_shared_sigalgs(SSL *s)
	{
	const unsigned char *pref, *allow, *conf;
	size_t preflen, allowlen, conflen;
	size_t nmatch;
	TLS_SIGALGS *salgs = NULL;
	CERT *c = s->cert;
	if (c->shared_sigalgs)
		{
		OPENSSL_free(c->shared_sigalgs);
		c->shared_sigalgs = NULL;
		}
	/* If client use client signature algorithms if not NULL */
	if (!s->server && c->client_sigalgs)
		{
		conf = c->client_sigalgs;
		conflen = c->client_sigalgslen;
		}
	else if (c->conf_sigalgs)
		{
		conf = c->conf_sigalgs;
		conflen = c->conf_sigalgslen;
		}
	else
		conflen = tls12_get_psigalgs(s, &conf);
	if(s->options & SSL_OP_CIPHER_SERVER_PREFERENCE)
		{
		pref = conf;
		preflen = conflen;
		allow = c->peer_sigalgs;
		allowlen = c->peer_sigalgslen;
		}
	else
		{
		allow = conf;
		allowlen = conflen;
		pref = c->peer_sigalgs;
		preflen = c->peer_sigalgslen;
		}
	nmatch = tls12_do_shared_sigalgs(NULL, pref, preflen, allow, allowlen);
	if (!nmatch)
		return 1;
	salgs = OPENSSL_malloc(nmatch * sizeof(TLS_SIGALGS));
	if (!salgs)
		return 0;
	nmatch = tls12_do_shared_sigalgs(salgs, pref, preflen, allow, allowlen);
	c->shared_sigalgs = salgs;
	c->shared_sigalgslen = nmatch;
	return 1;
	}
		

/* Set preferred digest for each key type */

int tls1_process_sigalgs(SSL *s, const CBS *sigalgs)
	{
	int idx;
	size_t i;
	const EVP_MD *md;
	CERT *c = s->cert;
	TLS_SIGALGS *sigptr;

	/* Extension ignored for inappropriate versions */
	if (!SSL_USE_SIGALGS(s))
		return 1;
	/* Length must be even */
	if (CBS_len(sigalgs) % 2 != 0)
		return 0;
	/* Should never happen */
	if (!c)
		return 0;

	if (!CBS_stow(sigalgs, &c->peer_sigalgs, &c->peer_sigalgslen))
		return 0;

	tls1_set_shared_sigalgs(s);

	for (i = 0, sigptr = c->shared_sigalgs;
			i < c->shared_sigalgslen; i++, sigptr++)
		{
		idx = tls12_get_pkey_idx(sigptr->rsign);
		if (idx > 0 && c->pkeys[idx].digest == NULL)
			{
			md = tls12_get_hash(sigptr->rhash);
			c->pkeys[idx].digest = md;
			c->pkeys[idx].valid_flags = CERT_PKEY_EXPLICIT_SIGN;
			if (idx == SSL_PKEY_RSA_SIGN)
				{
				c->pkeys[SSL_PKEY_RSA_ENC].valid_flags = CERT_PKEY_EXPLICIT_SIGN;
				c->pkeys[SSL_PKEY_RSA_ENC].digest = md;
				}
			}

		}
	/* In strict mode leave unset digests as NULL to indicate we can't
	 * use the certificate for signing.
	 */
	if (!(s->cert->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT))
		{
		/* Set any remaining keys to default values. NOTE: if alg is
		 * not supported it stays as NULL.
	 	 */
		if (!c->pkeys[SSL_PKEY_RSA_SIGN].digest)
			{
			c->pkeys[SSL_PKEY_RSA_SIGN].digest = EVP_sha1();
			c->pkeys[SSL_PKEY_RSA_ENC].digest = EVP_sha1();
			}
		if (!c->pkeys[SSL_PKEY_ECC].digest)
			c->pkeys[SSL_PKEY_ECC].digest = EVP_sha1();
		}
	return 1;
	}


int SSL_get_sigalgs(SSL *s, int idx,
			int *psign, int *phash, int *psignhash,
			unsigned char *rsig, unsigned char *rhash)
	{
	const unsigned char *psig = s->cert->peer_sigalgs;
	if (psig == NULL)
		return 0;
	if (idx >= 0)
		{
		idx <<= 1;
		if (idx >= (int)s->cert->peer_sigalgslen)
			return 0;
		psig += idx;
		if (rhash)
			*rhash = psig[0];
		if (rsig)
			*rsig = psig[1];
		tls1_lookup_sigalg(phash, psign, psignhash, psig);
		}
	return s->cert->peer_sigalgslen / 2;
	}

int SSL_get_shared_sigalgs(SSL *s, int idx,
			int *psign, int *phash, int *psignhash,
			unsigned char *rsig, unsigned char *rhash)
	{
	TLS_SIGALGS *shsigalgs = s->cert->shared_sigalgs;
	if (!shsigalgs || idx >= (int)s->cert->shared_sigalgslen)
		return 0;
	shsigalgs += idx;
	if (phash)
		*phash = shsigalgs->hash_nid;
	if (psign)
		*psign = shsigalgs->sign_nid;
	if (psignhash)
		*psignhash = shsigalgs->signandhash_nid;
	if (rsig)
		*rsig = shsigalgs->rsign;
	if (rhash)
		*rhash = shsigalgs->rhash;
	return s->cert->shared_sigalgslen;
	}
	
/* tls1_channel_id_hash calculates the signed data for a Channel ID on the given
 * SSL connection and writes it to |md|. */
int
tls1_channel_id_hash(EVP_MD_CTX *md, SSL *s)
	{
	EVP_MD_CTX ctx;
	unsigned char temp_digest[EVP_MAX_MD_SIZE];
	unsigned temp_digest_len;
	int i;
	static const char kClientIDMagic[] = "TLS Channel ID signature";

	if (s->s3->handshake_buffer)
		if (!ssl3_digest_cached_records(s, free_handshake_buffer))
			return 0;

	EVP_DigestUpdate(md, kClientIDMagic, sizeof(kClientIDMagic));

	if (s->hit && s->s3->tlsext_channel_id_new)
		{
		static const char kResumptionMagic[] = "Resumption";
		EVP_DigestUpdate(md, kResumptionMagic,
				 sizeof(kResumptionMagic));
		if (s->session->original_handshake_hash_len == 0)
			return 0;
		EVP_DigestUpdate(md, s->session->original_handshake_hash,
				 s->session->original_handshake_hash_len);
		}

	EVP_MD_CTX_init(&ctx);
	for (i = 0; i < SSL_MAX_DIGEST; i++)
		{
		if (s->s3->handshake_dgst[i] == NULL)
			continue;
		EVP_MD_CTX_copy_ex(&ctx, s->s3->handshake_dgst[i]);
		EVP_DigestFinal_ex(&ctx, temp_digest, &temp_digest_len);
		EVP_DigestUpdate(md, temp_digest, temp_digest_len);
		}
	EVP_MD_CTX_cleanup(&ctx);

	return 1;
	}

/* tls1_record_handshake_hashes_for_channel_id records the current handshake
 * hashes in |s->session| so that Channel ID resumptions can sign that data. */
int tls1_record_handshake_hashes_for_channel_id(SSL *s)
	{
	int digest_len;
	/* This function should never be called for a resumed session because
	 * the handshake hashes that we wish to record are for the original,
	 * full handshake. */
	if (s->hit)
		return -1;
	/* It only makes sense to call this function if Channel IDs have been
	 * negotiated. */
	if (!s->s3->tlsext_channel_id_new)
		return -1;

	digest_len = tls1_handshake_digest(
		s, s->session->original_handshake_hash,
		sizeof(s->session->original_handshake_hash));
	if (digest_len < 0)
		return -1;

	s->session->original_handshake_hash_len = digest_len;

	return 1;
	}

int tls1_set_sigalgs(CERT *c, const int *psig_nids, size_t salglen, int client)
	{
	unsigned char *sigalgs, *sptr;
	int rhash, rsign;
	size_t i;
	if (salglen & 1)
		return 0;
	sigalgs = OPENSSL_malloc(salglen);
	if (sigalgs == NULL)
		return 0;
	for (i = 0, sptr = sigalgs; i < salglen; i+=2)
		{
		rhash = tls12_find_id(*psig_nids++, tls12_md,
					sizeof(tls12_md)/sizeof(tls12_lookup));
		rsign = tls12_find_id(*psig_nids++, tls12_sig,
				sizeof(tls12_sig)/sizeof(tls12_lookup));

		if (rhash == -1 || rsign == -1)
			goto err;
		*sptr++ = rhash;
		*sptr++ = rsign;
		}

	if (client)
		{
		if (c->client_sigalgs)
			OPENSSL_free(c->client_sigalgs);
		c->client_sigalgs = sigalgs;
		c->client_sigalgslen = salglen;
		}
	else
		{
		if (c->conf_sigalgs)
			OPENSSL_free(c->conf_sigalgs);
		c->conf_sigalgs = sigalgs;
		c->conf_sigalgslen = salglen;
		}

	return 1;

	err:
	OPENSSL_free(sigalgs);
	return 0;
	}

static int tls1_check_sig_alg(CERT *c, X509 *x, int default_nid)
	{
	int sig_nid;
	size_t i;
	if (default_nid == -1)
		return 1;
	sig_nid = X509_get_signature_nid(x);
	if (default_nid)
		return sig_nid == default_nid ? 1 : 0;
	for (i = 0; i < c->shared_sigalgslen; i++)
		if (sig_nid == c->shared_sigalgs[i].signandhash_nid)
			return 1;
	return 0;
	}
/* Check to see if a certificate issuer name matches list of CA names */
static int ssl_check_ca_name(STACK_OF(X509_NAME) *names, X509 *x)
	{
	X509_NAME *nm;
	size_t i;
	nm = X509_get_issuer_name(x);
	for (i = 0; i < sk_X509_NAME_num(names); i++)
		{
		if(!X509_NAME_cmp(nm, sk_X509_NAME_value(names, i)))
			return 1;
		}
	return 0;
	}

/* Check certificate chain is consistent with TLS extensions and is
 * usable by server. This servers two purposes: it allows users to 
 * check chains before passing them to the server and it allows the
 * server to check chains before attempting to use them.
 */

/* Flags which need to be set for a certificate when stict mode not set */

#define CERT_PKEY_VALID_FLAGS \
	(CERT_PKEY_EE_SIGNATURE|CERT_PKEY_EE_PARAM)
/* Strict mode flags */
#define CERT_PKEY_STRICT_FLAGS \
	 (CERT_PKEY_VALID_FLAGS|CERT_PKEY_CA_SIGNATURE|CERT_PKEY_CA_PARAM \
	 | CERT_PKEY_ISSUER_NAME|CERT_PKEY_CERT_TYPE)

int tls1_check_chain(SSL *s, X509 *x, EVP_PKEY *pk, STACK_OF(X509) *chain,
									int idx)
	{
	size_t i;
	int rv = 0;
	int check_flags = 0, strict_mode;
	CERT_PKEY *cpk = NULL;
	CERT *c = s->cert;
	/* idx == -1 means checking server chains */
	if (idx != -1)
		{
		/* idx == -2 means checking client certificate chains */
		if (idx == -2)
			{
			cpk = c->key;
			idx = cpk - c->pkeys;
			}
		else
			cpk = c->pkeys + idx;
		x = cpk->x509;
		pk = cpk->privatekey;
		chain = cpk->chain;
		strict_mode = c->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT;
		/* If no cert or key, forget it */
		if (!x || !pk)
			goto end;
		}
	else
		{
		if (!x || !pk)
			goto end;
		idx = ssl_cert_type(x, pk);
		if (idx == -1)
			goto end;
		cpk = c->pkeys + idx;
		if (c->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT)
			check_flags = CERT_PKEY_STRICT_FLAGS;
		else
			check_flags = CERT_PKEY_VALID_FLAGS;
		strict_mode = 1;
		}

	/* Check all signature algorithms are consistent with
	 * signature algorithms extension if TLS 1.2 or later
	 * and strict mode.
	 */
	if (TLS1_get_version(s) >= TLS1_2_VERSION && strict_mode)
		{
		int default_nid;
		unsigned char rsign = 0;
		if (c->peer_sigalgs)
			default_nid = 0;
		/* If no sigalgs extension use defaults from RFC5246 */
		else
			{
			switch(idx)
				{	
			case SSL_PKEY_RSA_ENC:
			case SSL_PKEY_RSA_SIGN:
				rsign = TLSEXT_signature_rsa;
				default_nid = NID_sha1WithRSAEncryption;
				break;

			case SSL_PKEY_ECC:
				rsign = TLSEXT_signature_ecdsa;
				default_nid = NID_ecdsa_with_SHA1;
				break;

			default:
				default_nid = -1;
				break;
				}
			}
		/* If peer sent no signature algorithms extension and we
		 * have set preferred signature algorithms check we support
		 * sha1.
		 */
		if (default_nid > 0 && c->conf_sigalgs)
			{
			size_t j;
			const unsigned char *p = c->conf_sigalgs;
			for (j = 0; j < c->conf_sigalgslen; j += 2, p += 2)
				{
				if (p[0] == TLSEXT_hash_sha1 && p[1] == rsign)
					break;
				}
			if (j == c->conf_sigalgslen)
				{
				if (check_flags)
					goto skip_sigs;
				else
					goto end;
				}
			}
		/* Check signature algorithm of each cert in chain */
		if (!tls1_check_sig_alg(c, x, default_nid))
			{
			if (!check_flags) goto end;
			}
		else
			rv |= CERT_PKEY_EE_SIGNATURE;
		rv |= CERT_PKEY_CA_SIGNATURE;
		for (i = 0; i < sk_X509_num(chain); i++)
			{
			if (!tls1_check_sig_alg(c, sk_X509_value(chain, i),
							default_nid))
				{
				if (check_flags)
					{
					rv &= ~CERT_PKEY_CA_SIGNATURE;
					break;
					}
				else
					goto end;
				}
			}
		}
	/* Else not TLS 1.2, so mark EE and CA signing algorithms OK */
	else if(check_flags)
		rv |= CERT_PKEY_EE_SIGNATURE|CERT_PKEY_CA_SIGNATURE;
	skip_sigs:
	/* Check cert parameters are consistent */
	if (tls1_check_cert_param(s, x, check_flags ? 1 : 2))
		rv |= CERT_PKEY_EE_PARAM;
	else if (!check_flags)
		goto end;
	if (!s->server)
		rv |= CERT_PKEY_CA_PARAM;
	/* In strict mode check rest of chain too */
	else if (strict_mode)
		{
		rv |= CERT_PKEY_CA_PARAM;
		for (i = 0; i < sk_X509_num(chain); i++)
			{
			X509 *ca = sk_X509_value(chain, i);
			if (!tls1_check_cert_param(s, ca, 0))
				{
				if (check_flags)
					{
					rv &= ~CERT_PKEY_CA_PARAM;
					break;
					}
				else
					goto end;
				}
			}
		}
	if (!s->server && strict_mode)
		{
		STACK_OF(X509_NAME) *ca_dn;
		uint8_t check_type = 0;
		switch (pk->type)
			{
		case EVP_PKEY_RSA:
			check_type = TLS_CT_RSA_SIGN;
			break;
		case EVP_PKEY_EC:
			check_type = TLS_CT_ECDSA_SIGN;
			break;
			}
		if (check_type)
			{
			if (s->s3->tmp.certificate_types &&
				memchr(s->s3->tmp.certificate_types, check_type, s->s3->tmp.num_certificate_types))
				{
					rv |= CERT_PKEY_CERT_TYPE;
				}
			if (!(rv & CERT_PKEY_CERT_TYPE) && !check_flags)
				goto end;
			}
		else
			rv |= CERT_PKEY_CERT_TYPE;


		ca_dn = s->s3->tmp.ca_names;

		if (!sk_X509_NAME_num(ca_dn))
			rv |= CERT_PKEY_ISSUER_NAME;

		if (!(rv & CERT_PKEY_ISSUER_NAME))
			{
			if (ssl_check_ca_name(ca_dn, x))
				rv |= CERT_PKEY_ISSUER_NAME;
			}
		if (!(rv & CERT_PKEY_ISSUER_NAME))
			{
			for (i = 0; i < sk_X509_num(chain); i++)
				{
				X509 *xtmp = sk_X509_value(chain, i);
				if (ssl_check_ca_name(ca_dn, xtmp))
					{
					rv |= CERT_PKEY_ISSUER_NAME;
					break;
					}
				}
			}
		if (!check_flags && !(rv & CERT_PKEY_ISSUER_NAME))
			goto end;
		}
	else
		rv |= CERT_PKEY_ISSUER_NAME|CERT_PKEY_CERT_TYPE;

	if (!check_flags || (rv & check_flags) == check_flags)
		rv |= CERT_PKEY_VALID;

	end:

	if (TLS1_get_version(s) >= TLS1_2_VERSION)
		{
		if (cpk->valid_flags & CERT_PKEY_EXPLICIT_SIGN)
			rv |= CERT_PKEY_EXPLICIT_SIGN|CERT_PKEY_SIGN;
		else if (cpk->digest)
			rv |= CERT_PKEY_SIGN;
		}
	else
		rv |= CERT_PKEY_SIGN|CERT_PKEY_EXPLICIT_SIGN;

	/* When checking a CERT_PKEY structure all flags are irrelevant
	 * if the chain is invalid.
	 */
	if (!check_flags)
		{
		if (rv & CERT_PKEY_VALID)
			cpk->valid_flags = rv;
		else
			{
			/* Preserve explicit sign flag, clear rest */
			cpk->valid_flags &= CERT_PKEY_EXPLICIT_SIGN;
			return 0;
			}
		}
	return rv;
	}

/* Set validity of certificates in an SSL structure */
void tls1_set_cert_validity(SSL *s)
	{
	tls1_check_chain(s, NULL, NULL, NULL, SSL_PKEY_RSA_ENC);
	tls1_check_chain(s, NULL, NULL, NULL, SSL_PKEY_RSA_SIGN);
	tls1_check_chain(s, NULL, NULL, NULL, SSL_PKEY_ECC);
	}
/* User level utiity function to check a chain is suitable */
int SSL_check_chain(SSL *s, X509 *x, EVP_PKEY *pk, STACK_OF(X509) *chain)
	{
	return tls1_check_chain(s, x, pk, chain, -1);
	}

