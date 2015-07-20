/* ssl/s3_clnt.c */
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by 
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>

#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/rand.h>
#include <openssl/obj.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/md5.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/engine.h>
#include <openssl/x509.h>

#include "ssl_locl.h"
#include "../crypto/dh/internal.h"

static const SSL_METHOD *ssl3_get_client_method(int ver)
	{
	switch (ver)
		{
	case TLS1_2_VERSION:
		return TLSv1_2_client_method();
	case TLS1_1_VERSION:
		return TLSv1_1_client_method();
	case TLS1_VERSION:
		return TLSv1_client_method();
	case SSL3_VERSION:
		return SSLv3_client_method();
	default:
		return NULL;
		}
	}

IMPLEMENT_tls_meth_func(TLS1_2_VERSION, TLSv1_2_client_method,
			ssl_undefined_function,
			ssl3_connect,
			ssl3_get_client_method,
			TLSv1_2_enc_data)

IMPLEMENT_tls_meth_func(TLS1_1_VERSION, TLSv1_1_client_method,
			ssl_undefined_function,
			ssl3_connect,
			ssl3_get_client_method,
			TLSv1_1_enc_data)

IMPLEMENT_tls_meth_func(TLS1_VERSION, TLSv1_client_method,
			ssl_undefined_function,
			ssl3_connect,
			ssl3_get_client_method,
			TLSv1_enc_data)

IMPLEMENT_tls_meth_func(SSL3_VERSION, SSLv3_client_method,
			ssl_undefined_function,
			ssl3_connect,
			ssl3_get_client_method,
			SSLv3_enc_data)

int ssl3_connect(SSL *s)
	{
	BUF_MEM *buf=NULL;
	void (*cb)(const SSL *ssl,int type,int val)=NULL;
	int ret= -1;
	int new_state,state,skip=0;

	ERR_clear_error();
	ERR_clear_system_error();

	if (s->info_callback != NULL)
		cb=s->info_callback;
	else if (s->ctx->info_callback != NULL)
		cb=s->ctx->info_callback;
	
	s->in_handshake++;
	if (!SSL_in_init(s) || SSL_in_before(s)) SSL_clear(s); 

	for (;;)
		{
		state=s->state;

		switch(s->state)
			{
		case SSL_ST_RENEGOTIATE:
			s->renegotiate=1;
			s->state=SSL_ST_CONNECT;
			s->ctx->stats.sess_connect_renegotiate++;
			/* break */
		case SSL_ST_BEFORE:
		case SSL_ST_CONNECT:
		case SSL_ST_BEFORE|SSL_ST_CONNECT:
		case SSL_ST_OK|SSL_ST_CONNECT:

			s->server=0;
			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_START,1);

			if ((s->version & 0xff00 ) != 0x0300)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_connect, ERR_R_INTERNAL_ERROR);
				ret = -1;
				goto end;
				}
				
			/* s->version=SSL3_VERSION; */
			s->type=SSL_ST_CONNECT;

			if (s->init_buf == NULL)
				{
				if ((buf=BUF_MEM_new()) == NULL)
					{
					ret= -1;
					goto end;
					}
				if (!BUF_MEM_grow(buf,SSL3_RT_MAX_PLAIN_LENGTH))
					{
					ret= -1;
					goto end;
					}
				s->init_buf=buf;
				buf=NULL;
				}

			if (!ssl3_setup_buffers(s)) { ret= -1; goto end; }

			/* setup buffing BIO */
			if (!ssl_init_wbio_buffer(s,0)) { ret= -1; goto end; }

			/* don't push the buffering BIO quite yet */

			ssl3_init_finished_mac(s);

			s->state=SSL3_ST_CW_CLNT_HELLO_A;
			s->ctx->stats.sess_connect++;
			s->init_num=0;
			break;

		case SSL3_ST_CW_CLNT_HELLO_A:
		case SSL3_ST_CW_CLNT_HELLO_B:

			s->shutdown=0;
			ret=ssl3_send_client_hello(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_SRVR_HELLO_A;
			s->init_num=0;

			/* turn on buffering for the next lot of output */
			if (s->bbio != s->wbio)
				s->wbio=BIO_push(s->bbio,s->wbio);

			break;

		case SSL3_ST_CR_SRVR_HELLO_A:
		case SSL3_ST_CR_SRVR_HELLO_B:
			ret=ssl3_get_server_hello(s);
			if (ret <= 0) goto end;

			if (s->hit)
				{
				s->state=SSL3_ST_CR_CHANGE;
				if (s->tlsext_ticket_expected)
					{
					/* receive renewed session ticket */
					s->state=SSL3_ST_CR_SESSION_TICKET_A;
					}
				}
			else
				{
				s->state=SSL3_ST_CR_CERT_A;
				}
			s->init_num=0;
			break;

		case SSL3_ST_CR_CERT_A:
		case SSL3_ST_CR_CERT_B:
			if (ssl_cipher_has_server_public_key(s->s3->tmp.new_cipher))
				{
				ret=ssl3_get_server_certificate(s);
				if (ret <= 0) goto end;
				if (s->s3->tmp.certificate_status_expected)
					s->state=SSL3_ST_CR_CERT_STATUS_A;
				else
					s->state=SSL3_ST_CR_KEY_EXCH_A;
				}
			else
				{
				skip = 1;
				s->state=SSL3_ST_CR_KEY_EXCH_A;
				}
			s->init_num=0;
			break;

		case SSL3_ST_CR_KEY_EXCH_A:
		case SSL3_ST_CR_KEY_EXCH_B:
			ret=ssl3_get_server_key_exchange(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_CERT_REQ_A;
			s->init_num=0;

			/* at this point we check that we have the
			 * required stuff from the server */
			if (!ssl3_check_cert_and_algorithm(s))
				{
				ret= -1;
				goto end;
				}
			break;

		case SSL3_ST_CR_CERT_REQ_A:
		case SSL3_ST_CR_CERT_REQ_B:
			ret=ssl3_get_certificate_request(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_SRVR_DONE_A;
			s->init_num=0;
			break;

		case SSL3_ST_CR_SRVR_DONE_A:
		case SSL3_ST_CR_SRVR_DONE_B:
			ret=ssl3_get_server_done(s);
			if (ret <= 0) goto end;
			if (s->s3->tmp.cert_req)
				s->state=SSL3_ST_CW_CERT_A;
			else
				s->state=SSL3_ST_CW_KEY_EXCH_A;
			s->init_num=0;

			break;

		case SSL3_ST_CW_CERT_A:
		case SSL3_ST_CW_CERT_B:
		case SSL3_ST_CW_CERT_C:
		case SSL3_ST_CW_CERT_D:
			ret=ssl3_send_client_certificate(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_KEY_EXCH_A;
			s->init_num=0;
			break;

		case SSL3_ST_CW_KEY_EXCH_A:
		case SSL3_ST_CW_KEY_EXCH_B:
			ret=ssl3_send_client_key_exchange(s);
			if (ret <= 0) goto end;
			/* For TLS, cert_req is set to 2, so a cert chain
			 * of nothing is sent, but no verify packet is sent */
			if (s->s3->tmp.cert_req == 1)
				{
				s->state=SSL3_ST_CW_CERT_VRFY_A;
				}
			else
				{
				s->state=SSL3_ST_CW_CHANGE_A;
				s->s3->change_cipher_spec=0;
				}

			s->init_num=0;
			break;

		case SSL3_ST_CW_CERT_VRFY_A:
		case SSL3_ST_CW_CERT_VRFY_B:
			ret=ssl3_send_cert_verify(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_CHANGE_A;
			s->init_num=0;
			s->s3->change_cipher_spec=0;
			break;

		case SSL3_ST_CW_CHANGE_A:
		case SSL3_ST_CW_CHANGE_B:
			ret=ssl3_send_change_cipher_spec(s,
				SSL3_ST_CW_CHANGE_A,SSL3_ST_CW_CHANGE_B);
			if (ret <= 0) goto end;

 			s->state=SSL3_ST_CW_FINISHED_A;
			if (s->s3->tlsext_channel_id_valid)
				s->state=SSL3_ST_CW_CHANNEL_ID_A;
			if (s->s3->next_proto_neg_seen)
				s->state=SSL3_ST_CW_NEXT_PROTO_A;
			s->init_num=0;

			s->session->cipher=s->s3->tmp.new_cipher;
			if (!s->method->ssl3_enc->setup_key_block(s))
				{
				ret= -1;
				goto end;
				}

			if (!s->method->ssl3_enc->change_cipher_state(s,
				SSL3_CHANGE_CIPHER_CLIENT_WRITE))
				{
				ret= -1;
				goto end;
				}

			break;

		case SSL3_ST_CW_NEXT_PROTO_A:
		case SSL3_ST_CW_NEXT_PROTO_B:
			ret=ssl3_send_next_proto(s);
			if (ret <= 0) goto end;
			if (s->s3->tlsext_channel_id_valid)
				s->state=SSL3_ST_CW_CHANNEL_ID_A;
			else
				s->state=SSL3_ST_CW_FINISHED_A;
			break;

		case SSL3_ST_CW_CHANNEL_ID_A:
		case SSL3_ST_CW_CHANNEL_ID_B:
			ret=ssl3_send_channel_id(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_FINISHED_A;
			break;

		case SSL3_ST_CW_FINISHED_A:
		case SSL3_ST_CW_FINISHED_B:
			ret=ssl3_send_finished(s,
				SSL3_ST_CW_FINISHED_A,SSL3_ST_CW_FINISHED_B,
				s->method->ssl3_enc->client_finished_label,
				s->method->ssl3_enc->client_finished_label_len);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_FLUSH;

			/* clear flags */
			s->s3->flags&= ~SSL3_FLAGS_POP_BUFFER;
			if (s->hit)
				{
				s->s3->tmp.next_state=SSL_ST_OK;
				}
			else
				{
				/* This is a non-resumption handshake. If it
				 * involves ChannelID, then record the
				 * handshake hashes at this point in the
				 * session so that any resumption of this
				 * session with ChannelID can sign those
				 * hashes. */
				if (s->s3->tlsext_channel_id_new)
					{
					ret = tls1_record_handshake_hashes_for_channel_id(s);
					if (ret <= 0)
						goto end;
					}
				if ((SSL_get_mode(s) & SSL_MODE_HANDSHAKE_CUTTHROUGH)
				    && ssl3_can_cutthrough(s)
				    && s->s3->previous_server_finished_len == 0 /* no cutthrough on renegotiation (would complicate the state machine) */
				   )
					{
					s->s3->tmp.next_state=SSL3_ST_CUTTHROUGH_COMPLETE;
					}
				else
					{
					/* Allow NewSessionTicket if ticket expected */
					if (s->tlsext_ticket_expected)
						s->s3->tmp.next_state=SSL3_ST_CR_SESSION_TICKET_A;
					else
						s->s3->tmp.next_state=SSL3_ST_CR_CHANGE;
					}
				}
			s->init_num=0;
			break;

		case SSL3_ST_CR_SESSION_TICKET_A:
		case SSL3_ST_CR_SESSION_TICKET_B:
			ret=ssl3_get_new_session_ticket(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_CHANGE;
			s->init_num=0;
		break;

		case SSL3_ST_CR_CERT_STATUS_A:
		case SSL3_ST_CR_CERT_STATUS_B:
			ret=ssl3_get_cert_status(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_KEY_EXCH_A;
			s->init_num=0;
		break;

		case SSL3_ST_CR_CHANGE:
			/* At this point, the next message must be entirely
			 * behind a ChangeCipherSpec. */
			if (!ssl3_expect_change_cipher_spec(s))
				{
				ret = -1;
				goto end;
				}
			s->state = SSL3_ST_CR_FINISHED_A;
			break;

		case SSL3_ST_CR_FINISHED_A:
		case SSL3_ST_CR_FINISHED_B:
			ret=ssl3_get_finished(s,SSL3_ST_CR_FINISHED_A,
				SSL3_ST_CR_FINISHED_B);
			if (ret <= 0) goto end;

			if (s->hit)
				s->state=SSL3_ST_CW_CHANGE_A;
			else
				s->state=SSL_ST_OK;
			s->init_num=0;
			break;

		case SSL3_ST_CW_FLUSH:
			s->rwstate=SSL_WRITING;
			if (BIO_flush(s->wbio) <= 0)
				{
				ret= -1;
				goto end;
				}
			s->rwstate=SSL_NOTHING;
			s->state=s->s3->tmp.next_state;
			break;

		case SSL3_ST_CUTTHROUGH_COMPLETE:
			/* Allow NewSessionTicket if ticket expected */
			if (s->tlsext_ticket_expected)
				s->state=SSL3_ST_CR_SESSION_TICKET_A;
			else
				s->state=SSL3_ST_CR_CHANGE;

			ssl_free_wbio_buffer(s);
			ret = 1;
			goto end;
			/* break; */

		case SSL_ST_OK:
			/* clean a few things up */
			ssl3_cleanup_key_block(s);

			if (s->init_buf != NULL)
				{
				BUF_MEM_free(s->init_buf);
				s->init_buf=NULL;
				}

			/* If we are not 'joining' the last two packets,
			 * remove the buffering now */
			if (!(s->s3->flags & SSL3_FLAGS_POP_BUFFER))
				ssl_free_wbio_buffer(s);
			/* else do it later in ssl3_write */

			s->init_num=0;
			s->renegotiate=0;
			s->new_session=0;

			ssl_update_cache(s,SSL_SESS_CACHE_CLIENT);
			if (s->hit) s->ctx->stats.sess_hit++;

			ret=1;
			/* s->server=0; */
			s->handshake_func=ssl3_connect;
			s->ctx->stats.sess_connect_good++;

			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_DONE,1);

			goto end;
			/* break; */
			
		default:
			OPENSSL_PUT_ERROR(SSL, ssl3_connect, SSL_R_UNKNOWN_STATE);
			ret= -1;
			goto end;
			/* break; */
			}

		/* did we do anything */
		if (!s->s3->tmp.reuse_message && !skip)
			{
			if (s->debug)
				{
				if ((ret=BIO_flush(s->wbio)) <= 0)
					goto end;
				}

			if ((cb != NULL) && (s->state != state))
				{
				new_state=s->state;
				s->state=state;
				cb(s,SSL_CB_CONNECT_LOOP,1);
				s->state=new_state;
				}
			}
		skip=0;
		}
end:
	s->in_handshake--;
	if (buf != NULL)
		BUF_MEM_free(buf);
	if (cb != NULL)
		cb(s,SSL_CB_CONNECT_EXIT,ret);
	return(ret);
	}


int ssl3_send_client_hello(SSL *s)
	{
	unsigned char *buf;
	unsigned char *p,*d;
	int i;
	unsigned long l;

	buf=(unsigned char *)s->init_buf->data;
	if (s->state == SSL3_ST_CW_CLNT_HELLO_A)
		{
		SSL_SESSION *sess = s->session;
		if (sess == NULL ||
			sess->ssl_version != s->version ||
			!sess->session_id_length ||
			sess->not_resumable)
			{
			if (!ssl_get_new_session(s,0))
				goto err;
			}
		if (s->method->version == DTLS_ANY_VERSION)
			{
			/* Determine which DTLS version to use */
			int options = s->options;
			/* If DTLS 1.2 disabled correct the version number */
			if (options & SSL_OP_NO_DTLSv1_2)
				{
				/* Disabling all versions is silly: return an
				 * error.
				 */
				if (options & SSL_OP_NO_DTLSv1)
					{
					OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, SSL_R_WRONG_SSL_VERSION);
					goto err;
					}
				/* Update method so we don't use any DTLS 1.2
				 * features.
				 */
				s->method = DTLSv1_client_method();
				s->version = DTLS1_VERSION;
				}
			else
				{
				/* We only support one version: update method */
				if (options & SSL_OP_NO_DTLSv1)
					s->method = DTLSv1_2_client_method();
				s->version = DTLS1_2_VERSION;
				}
			s->client_version = s->version;
			}
		/* else use the pre-loaded session */

		p=s->s3->client_random;

		/* If resending the ClientHello in DTLS after a
		 * HelloVerifyRequest, don't renegerate the client_random. The
		 * random must be reused. */
		if (!SSL_IS_DTLS(s) || !s->d1->send_cookie)
			{
			ssl_fill_hello_random(s, 0, p,
					      sizeof(s->s3->client_random));
			}

		/* Do the message type and length last.
		 * Note: the final argument to ssl_add_clienthello_tlsext below
		 * depends on the size of this prefix. */
		d=p= ssl_handshake_start(s);

		/* version indicates the negotiated version: for example from
		 * an SSLv2/v3 compatible client hello). The client_version
		 * field is the maximum version we permit and it is also
		 * used in RSA encrypted premaster secrets. Some servers can
		 * choke if we initially report a higher version then
		 * renegotiate to a lower one in the premaster secret. This
		 * didn't happen with TLS 1.0 as most servers supported it
		 * but it can with TLS 1.1 or later if the server only supports
		 * 1.0.
		 *
		 * Possible scenario with previous logic:
		 * 	1. Client hello indicates TLS 1.2
		 * 	2. Server hello says TLS 1.0
		 *	3. RSA encrypted premaster secret uses 1.2.
		 * 	4. Handhaked proceeds using TLS 1.0.
		 *	5. Server sends hello request to renegotiate.
		 *	6. Client hello indicates TLS v1.0 as we now
		 *	   know that is maximum server supports.
		 *	7. Server chokes on RSA encrypted premaster secret
		 *	   containing version 1.0.
		 *
		 * For interoperability it should be OK to always use the
		 * maximum version we support in client hello and then rely
		 * on the checking of version to ensure the servers isn't
		 * being inconsistent: for example initially negotiating with
		 * TLS 1.0 and renegotiating with TLS 1.2. We do this by using
		 * client_version in client hello and not resetting it to
		 * the negotiated version.
		 */
#if 0
		*(p++)=s->version>>8;
		*(p++)=s->version&0xff;
		s->client_version=s->version;
#else
		*(p++)=s->client_version>>8;
		*(p++)=s->client_version&0xff;
#endif

		/* Random stuff */
		memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
		p+=SSL3_RANDOM_SIZE;

		/* Session ID */
		if (s->new_session)
			i=0;
		else
			i=s->session->session_id_length;
		*(p++)=i;
		if (i != 0)
			{
			if (i > (int)sizeof(s->session->session_id))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			memcpy(p,s->session->session_id,i);
			p+=i;
			}
		
		/* cookie stuff for DTLS */
		if (SSL_IS_DTLS(s))
			{
			if ( s->d1->cookie_len > sizeof(s->d1->cookie))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			*(p++) = s->d1->cookie_len;
			memcpy(p, s->d1->cookie, s->d1->cookie_len);
			p += s->d1->cookie_len;
			}
		
		/* Ciphers supported */
		i = ssl_cipher_list_to_bytes(s, SSL_get_ciphers(s), &p[2]);
		if (i == 0)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, SSL_R_NO_CIPHERS_AVAILABLE);
			goto err;
			}
		s2n(i,p);
		p+=i;

		/* COMPRESSION */
		*(p++)=1;
		*(p++)=0; /* Add the NULL method */

		/* TLS extensions*/
		if (ssl_prepare_clienthello_tlsext(s) <= 0)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, SSL_R_CLIENTHELLO_TLSEXT);
			goto err;
			}
		if ((p = ssl_add_clienthello_tlsext(s, p, buf+SSL3_RT_MAX_PLAIN_LENGTH, p-buf)) == NULL)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_client_hello, ERR_R_INTERNAL_ERROR);
			goto err;
			}
		
		l= p-d;
		ssl_set_handshake_header(s, SSL3_MT_CLIENT_HELLO, l);
		s->state=SSL3_ST_CW_CLNT_HELLO_B;
		}

	/* SSL3_ST_CW_CLNT_HELLO_B */
	return ssl_do_write(s);
err:
	return(-1);
	}

int ssl3_get_server_hello(SSL *s)
	{
	STACK_OF(SSL_CIPHER) *sk;
	const SSL_CIPHER *c;
	CERT *ct = s->cert;
	int al=SSL_AD_INTERNAL_ERROR,ok;
	long n;
	CBS server_hello, server_random, session_id;
	uint16_t server_version, cipher_suite;
	uint8_t compression_method;
	unsigned long mask_ssl;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_SRVR_HELLO_A,
		SSL3_ST_CR_SRVR_HELLO_B,
		SSL3_MT_SERVER_HELLO,
		20000, /* ?? */
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok) return((int)n);

	CBS_init(&server_hello, s->init_msg, n);

	if (!CBS_get_u16(&server_hello, &server_version) ||
		!CBS_get_bytes(&server_hello, &server_random, SSL3_RANDOM_SIZE) ||
		!CBS_get_u8_length_prefixed(&server_hello, &session_id) ||
		CBS_len(&session_id) > SSL3_SESSION_ID_SIZE ||
		!CBS_get_u16(&server_hello, &cipher_suite) ||
		!CBS_get_u8(&server_hello, &compression_method))
		{
		al = SSL_AD_DECODE_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_DECODE_ERROR);
		goto f_err;
		}

	if (s->method->version == DTLS_ANY_VERSION)
		{
		/* Work out correct protocol version to use */
		int options = s->options;
		if (server_version == DTLS1_2_VERSION
			&& !(options & SSL_OP_NO_DTLSv1_2))
			s->method = DTLSv1_2_client_method();
		else if (server_version == DTLS1_VERSION
			&& !(options & SSL_OP_NO_DTLSv1))
			s->method = DTLSv1_client_method();
		else
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_WRONG_SSL_VERSION);
			s->version = server_version;
			al = SSL_AD_PROTOCOL_VERSION;
			goto f_err;
			}
		s->version = s->client_version = s->method->version;
		}

	if (server_version != s->version)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_WRONG_SSL_VERSION);
		s->version = (s->version & 0xff00) | (server_version & 0xff);
		al = SSL_AD_PROTOCOL_VERSION;
		goto f_err;
		}

	/* Copy over the server random. */
	memcpy(s->s3->server_random, CBS_data(&server_random), SSL3_RANDOM_SIZE);

	s->hit = 0;

	/* check if we want to resume the session based on external pre-shared secret */
	if (s->version >= TLS1_VERSION && s->tls_session_secret_cb)
		{
		const SSL_CIPHER *pref_cipher=NULL;
		s->session->master_key_length=sizeof(s->session->master_key);
		if (s->tls_session_secret_cb(s, s->session->master_key,
					     &s->session->master_key_length,
					     NULL, &pref_cipher,
					     s->tls_session_secret_cb_arg))
			{
			s->session->cipher = pref_cipher ?
				pref_cipher :
				ssl3_get_cipher_by_value(cipher_suite);
			s->hit = 1;
			}
		}

	if (!s->hit && CBS_len(&session_id) != 0 &&
		CBS_mem_equal(&session_id,
			s->session->session_id, s->session->session_id_length))
		{
		if(s->sid_ctx_length != s->session->sid_ctx_length
			|| memcmp(s->session->sid_ctx, s->sid_ctx, s->sid_ctx_length))
			{
			/* actually a client application bug */
			al = SSL_AD_ILLEGAL_PARAMETER;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT);
			goto f_err;
			}
		s->hit = 1;
		}

	/* a miss or crap from the other end */
	if (!s->hit)
		{
		/* If we were trying for session-id reuse, make a new
		 * SSL_SESSION so we don't stuff up other people */
		if (s->session->session_id_length > 0)
			{
			if (!ssl_get_new_session(s,0))
				{
				goto f_err;
				}
			}
		/* Note: session_id could be empty. */
		s->session->session_id_length = CBS_len(&session_id);
		memcpy(s->session->session_id, CBS_data(&session_id), CBS_len(&session_id));
		}

	c = ssl3_get_cipher_by_value(cipher_suite);
	if (c == NULL)
		{
		/* unknown cipher */
		al = SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_UNKNOWN_CIPHER_RETURNED);
		goto f_err;
		}
	/* ct->mask_ssl was computed from client capabilities. Now
	 * that the final version is known, compute a new mask_ssl. */
	if (!SSL_USE_TLS1_2_CIPHERS(s))
		mask_ssl = SSL_TLSV1_2;
	else
		mask_ssl = 0;
	/* If it is a disabled cipher we didn't send it in client hello,
	 * so return an error.
	 */
	if (c->algorithm_ssl & mask_ssl ||
		c->algorithm_mkey & ct->mask_k ||
		c->algorithm_auth & ct->mask_a)
		{
		al=SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_WRONG_CIPHER_RETURNED);
		goto f_err;
		}

	sk=ssl_get_ciphers_by_id(s);
	if (!sk_SSL_CIPHER_find(sk, NULL, c))
		{
		/* we did not say we would use this cipher */
		al=SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_WRONG_CIPHER_RETURNED);
		goto f_err;
		}

	/* Depending on the session caching (internal/external), the cipher
	   and/or cipher_id values may not be set. Make sure that
	   cipher_id is set and use it for comparison. */
	if (s->session->cipher)
		s->session->cipher_id = s->session->cipher->id;
	if (s->hit && (s->session->cipher_id != c->id))
		{
		al = SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED);
		goto f_err;
		}
	s->s3->tmp.new_cipher=c;
	/* Don't digest cached records if no sigalgs: we may need them for
	 * client authentication.
	 */
	if (!SSL_USE_SIGALGS(s) && !ssl3_digest_cached_records(s, free_handshake_buffer))
		goto f_err;

	/* Only the NULL compression algorithm is supported. */
	if (compression_method != 0)
		{
		al = SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
		goto f_err;
		}

	/* TLS extensions */
	if (!ssl_parse_serverhello_tlsext(s, &server_hello))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_PARSE_TLSEXT);
		goto err; 
		}

        /* There should be nothing left over in the record. */
	if (CBS_len(&server_hello) != 0)
		{
		/* wrong packet length */
		al=SSL_AD_DECODE_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_hello, SSL_R_BAD_PACKET_LENGTH);
		goto f_err;
		}

	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
err:
	return(-1);
	}

int ssl3_get_server_certificate(SSL *s)
	{
	int al,i,ok,ret= -1;
	unsigned long n;
	X509 *x=NULL;
	STACK_OF(X509) *sk=NULL;
	SESS_CERT *sc;
	EVP_PKEY *pkey=NULL;
	CBS cbs, certificate_list;
	const uint8_t* data;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_CERT_A,
		SSL3_ST_CR_CERT_B,
		SSL3_MT_CERTIFICATE,
		s->max_cert_list,
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok) return((int)n);

	CBS_init(&cbs, s->init_msg, n);

	if ((sk=sk_X509_new_null()) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (!CBS_get_u24_length_prefixed(&cbs, &certificate_list) ||
		CBS_len(&cbs) != 0)
		{
		al = SSL_AD_DECODE_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_LENGTH_MISMATCH);
		goto f_err;
		}

	while (CBS_len(&certificate_list) > 0)
		{
		CBS certificate;
		if (!CBS_get_u24_length_prefixed(&certificate_list, &certificate))
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_CERT_LENGTH_MISMATCH);
			goto f_err;
			}
		data = CBS_data(&certificate);
		x = d2i_X509(NULL, &data, CBS_len(&certificate));
		if (x == NULL)
			{
			al=SSL_AD_BAD_CERTIFICATE;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, ERR_R_ASN1_LIB);
			goto f_err;
			}
		if (!CBS_skip(&certificate, data - CBS_data(&certificate)))
			{
			al = SSL_AD_INTERNAL_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, ERR_R_INTERNAL_ERROR);
			goto f_err;
			}
		if (CBS_len(&certificate) != 0)
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_CERT_LENGTH_MISMATCH);
			goto f_err;
			}
		if (!sk_X509_push(sk,x))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, ERR_R_MALLOC_FAILURE);
			goto err;
			}
		x=NULL;
		}

	i=ssl_verify_cert_chain(s,sk);
	if ((s->verify_mode != SSL_VERIFY_NONE) && (i <= 0)
		)
		{
		al=ssl_verify_alarm_type(s->verify_result);
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_CERTIFICATE_VERIFY_FAILED);
		goto f_err; 
		}
	ERR_clear_error(); /* but we keep s->verify_result */

	sc=ssl_sess_cert_new();
	if (sc == NULL) goto err;

	if (s->session->sess_cert) ssl_sess_cert_free(s->session->sess_cert);
	s->session->sess_cert=sc;

	sc->cert_chain=sk;
	/* Inconsistency alert: cert_chain does include the peer's
	 * certificate, which we don't include in s3_srvr.c */
	x=sk_X509_value(sk,0);
	sk=NULL;
 	/* VRS 19990621: possible memory leak; sk=null ==> !sk_pop_free() @end*/

	pkey=X509_get_pubkey(x);

	if ((pkey == NULL) || EVP_PKEY_missing_parameters(pkey))
		{
		x=NULL;
		al=SSL3_AL_FATAL;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_UNABLE_TO_FIND_PUBLIC_KEY_PARAMETERS);
		goto f_err;
		}

	i=ssl_cert_type(x,pkey);
	if (i < 0)
		{
		x=NULL;
		al=SSL3_AL_FATAL;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
		goto f_err;
		}

	int exp_idx = ssl_cipher_get_cert_index(s->s3->tmp.new_cipher);
	if (exp_idx >= 0 && i != exp_idx)
		{
		x=NULL;
		al=SSL_AD_ILLEGAL_PARAMETER;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, SSL_R_WRONG_CERTIFICATE_TYPE);
		goto f_err;
		}
	sc->peer_cert_type=i;
	/* Why would the following ever happen?
	 * We just created sc a couple of lines ago. */
	if (sc->peer_pkeys[i].x509 != NULL)
		X509_free(sc->peer_pkeys[i].x509);
	sc->peer_pkeys[i].x509 = X509_up_ref(x);
	sc->peer_key = &(sc->peer_pkeys[i]);

	if (s->session->peer != NULL)
		X509_free(s->session->peer);
	s->session->peer = X509_up_ref(x);

	s->session->verify_result = s->verify_result;

	x=NULL;
	ret=1;
	if (0)
		{
f_err:
		ssl3_send_alert(s,SSL3_AL_FATAL,al);
		}
err:
	EVP_PKEY_free(pkey);
	X509_free(x);
	sk_X509_pop_free(sk,X509_free);
	return(ret);
	}

int ssl3_get_server_key_exchange(SSL *s)
	{
	EVP_MD_CTX md_ctx;
	int al,ok;
	long n,alg_k,alg_a;
	EVP_PKEY *pkey=NULL;
	const EVP_MD *md = NULL;
	RSA *rsa=NULL;
	DH *dh=NULL;
	EC_KEY *ecdh = NULL;
	BN_CTX *bn_ctx = NULL;
	EC_POINT *srvr_ecpoint = NULL;
	CBS server_key_exchange, server_key_exchange_orig, parameter;

	/* use same message size as in ssl3_get_certificate_request()
	 * as ServerKeyExchange message may be skipped */
	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_KEY_EXCH_A,
		SSL3_ST_CR_KEY_EXCH_B,
		-1,
		s->max_cert_list,
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);
	if (!ok) return((int)n);

	if (s->s3->tmp.message_type != SSL3_MT_SERVER_KEY_EXCHANGE)
		{
		if (ssl_cipher_requires_server_key_exchange(s->s3->tmp.new_cipher))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_UNEXPECTED_MESSAGE);
			ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
			return -1;
			}

		/* In plain PSK ciphersuite, ServerKeyExchange can be
		   omitted if no identity hint is sent. Set
		   session->sess_cert anyway to avoid problems
		   later.*/
		if (s->s3->tmp.new_cipher->algorithm_auth & SSL_aPSK)
			{
			/* PSK ciphersuites that also send a
			 * Certificate would have already initialized
			 * |sess_cert|. */
			if (s->session->sess_cert == NULL)
				s->session->sess_cert = ssl_sess_cert_new();
			if (s->session->psk_identity_hint)
				{
				OPENSSL_free(s->session->psk_identity_hint);
				s->session->psk_identity_hint = NULL;
				}
			}
		s->s3->tmp.reuse_message=1;
		return(1);
		}

	/* Retain a copy of the original CBS to compute the signature
	 * over. */
	CBS_init(&server_key_exchange, s->init_msg, n);
	server_key_exchange_orig = server_key_exchange;

	if (s->session->sess_cert != NULL)
		{
		if (s->session->sess_cert->peer_dh_tmp)
			{
			DH_free(s->session->sess_cert->peer_dh_tmp);
			s->session->sess_cert->peer_dh_tmp=NULL;
			}
		if (s->session->sess_cert->peer_ecdh_tmp)
			{
			EC_KEY_free(s->session->sess_cert->peer_ecdh_tmp);
			s->session->sess_cert->peer_ecdh_tmp=NULL;
			}
		}
	else
		{
		s->session->sess_cert=ssl_sess_cert_new();
		}

	alg_k=s->s3->tmp.new_cipher->algorithm_mkey;
	alg_a=s->s3->tmp.new_cipher->algorithm_auth;
	EVP_MD_CTX_init(&md_ctx);

	if (alg_a & SSL_aPSK)
		{
		CBS psk_identity_hint;

		/* Each of the PSK key exchanges begins with a
		 * psk_identity_hint. */
		if (!CBS_get_u16_length_prefixed(&server_key_exchange, &psk_identity_hint))
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_DECODE_ERROR);
			goto f_err;
			}

		/* Store PSK identity hint for later use, hint is used in
		 * ssl3_send_client_key_exchange.  Assume that the maximum
		 * length of a PSK identity hint can be as long as the maximum
		 * length of a PSK identity. Also do not allow NULL
		 * characters; identities are saved as C strings.
		 *
		 * TODO(davidben): Should invalid hints be ignored? It's a hint
		 * rather than a specific identity. */
		if (CBS_len(&psk_identity_hint) > PSK_MAX_IDENTITY_LEN ||
			CBS_contains_zero_byte(&psk_identity_hint))
			{
			al = SSL_AD_HANDSHAKE_FAILURE;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_DATA_LENGTH_TOO_LONG);
			goto f_err;
			}

		/* Save the identity hint as a C string. */
		if (!CBS_strdup(&psk_identity_hint, &s->session->psk_identity_hint))
			{
			al = SSL_AD_HANDSHAKE_FAILURE;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_MALLOC_FAILURE);
			goto f_err;
			}
		}

	if (alg_k & SSL_kEDH)
		{
		CBS dh_p, dh_g, dh_Ys;

		if (!CBS_get_u16_length_prefixed(&server_key_exchange, &dh_p) ||
			CBS_len(&dh_p) == 0 ||
			!CBS_get_u16_length_prefixed(&server_key_exchange, &dh_g) ||
			CBS_len(&dh_g) == 0 ||
			!CBS_get_u16_length_prefixed(&server_key_exchange, &dh_Ys) ||
			CBS_len(&dh_Ys) == 0)
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_DECODE_ERROR);
			goto f_err;
			}

		if ((dh=DH_new()) == NULL)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_DH_LIB);
			goto err;
			}

		if (!(dh->p = BN_bin2bn(CBS_data(&dh_p), CBS_len(&dh_p), NULL)))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_BN_LIB);
			goto err;
			}
		if (!(dh->g=BN_bin2bn(CBS_data(&dh_g), CBS_len(&dh_g), NULL)))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_BN_LIB);
			goto err;
			}
		if (!(dh->pub_key = BN_bin2bn(CBS_data(&dh_Ys), CBS_len(&dh_Ys), NULL)))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_BN_LIB);
			goto err;
			}

		if (DH_num_bits(dh) < 1024)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_BAD_DH_P_LENGTH);
			goto err;
			}

		if (alg_a & SSL_aRSA)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
		/* else anonymous DH, so no certificate or pkey. */

		s->session->sess_cert->peer_dh_tmp=dh;
		dh=NULL;
		}

	else if (alg_k & SSL_kEECDH)
		{
		uint16_t curve_id;
		int curve_nid = 0;
		EC_GROUP *ngroup;
		const EC_GROUP *group;
		CBS point;

		/* Extract elliptic curve parameters and the server's
		 * ephemeral ECDH public key.  Check curve is one of
		 * our preferences, if not server has sent an invalid
		 * curve.
		 */
		if (!tls1_check_curve(s, &server_key_exchange, &curve_id))
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_WRONG_CURVE);
			goto f_err;
			}

		if ((curve_nid = tls1_ec_curve_id2nid(curve_id)) == 0)
			{
			al=SSL_AD_INTERNAL_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS);
			goto f_err;
			}

		if ((ecdh=EC_KEY_new()) == NULL)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_MALLOC_FAILURE);
			goto err;
			}
		ngroup = EC_GROUP_new_by_curve_name(curve_nid);
		if (ngroup == NULL)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_EC_LIB);
			goto err;
			}
		if (EC_KEY_set_group(ecdh, ngroup) == 0)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_EC_LIB);
			goto err;
			}
		EC_GROUP_free(ngroup);

		group = EC_KEY_get0_group(ecdh);

		/* Next, get the encoded ECPoint */
		if (!CBS_get_u8_length_prefixed(&server_key_exchange, &point))
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_DECODE_ERROR);
			goto f_err;
			}

		if (((srvr_ecpoint = EC_POINT_new(group)) == NULL) ||
		    ((bn_ctx = BN_CTX_new()) == NULL))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_MALLOC_FAILURE);
			goto err;
			}

		if (!EC_POINT_oct2point(group, srvr_ecpoint,
				CBS_data(&point), CBS_len(&point), bn_ctx))
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_BAD_ECPOINT);
			goto f_err;
			}

		/* The ECC/TLS specification does not mention
		 * the use of DSA to sign ECParameters in the server
		 * key exchange message. We do support RSA and ECDSA.
		 */
		if (0) ;
		else if (alg_a & SSL_aRSA)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
		else if (alg_a & SSL_aECDSA)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_ECC].x509);
		/* else anonymous ECDH, so no certificate or pkey. */
		EC_KEY_set_public_key(ecdh, srvr_ecpoint);
		s->session->sess_cert->peer_ecdh_tmp=ecdh;
		ecdh=NULL;
		BN_CTX_free(bn_ctx);
		bn_ctx = NULL;
		EC_POINT_free(srvr_ecpoint);
		srvr_ecpoint = NULL;
		}

	else if (!(alg_k & SSL_kPSK))
		{
		al=SSL_AD_UNEXPECTED_MESSAGE;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_UNEXPECTED_MESSAGE);
		goto f_err;
		}

	/* At this point, |server_key_exchange| contains the
	 * signature, if any, while |server_key_exchange_orig|
	 * contains the entire message. From that, derive a CBS
	 * containing just the parameter. */
	CBS_init(&parameter, CBS_data(&server_key_exchange_orig),
		CBS_len(&server_key_exchange_orig) -
		CBS_len(&server_key_exchange));

	/* if it was signed, check the signature */
	if (pkey != NULL)
		{
		CBS signature;

		if (SSL_USE_SIGALGS(s))
			{
			if (!tls12_check_peer_sigalg(&md, &al, s, &server_key_exchange, pkey))
				goto f_err;
			}
		else
			md = EVP_sha1();

		/* The last field in |server_key_exchange| is the
		 * signature. */
		if (!CBS_get_u16_length_prefixed(&server_key_exchange, &signature) ||
			CBS_len(&server_key_exchange) != 0)
			{
			al = SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_DECODE_ERROR);
			goto f_err;
			}

		if (pkey->type == EVP_PKEY_RSA && !SSL_USE_SIGALGS(s))
			{
			int num;
			unsigned char *q, md_buf[EVP_MAX_MD_SIZE*2];
			size_t md_len = 0;

			q=md_buf;
			for (num=2; num > 0; num--)
				{
				unsigned int digest_len;
				EVP_DigestInit_ex(&md_ctx,
					(num == 2) ? EVP_md5() : EVP_sha1(), NULL);
				EVP_DigestUpdate(&md_ctx,&(s->s3->client_random[0]),SSL3_RANDOM_SIZE);
				EVP_DigestUpdate(&md_ctx,&(s->s3->server_random[0]),SSL3_RANDOM_SIZE);
				EVP_DigestUpdate(&md_ctx, CBS_data(&parameter), CBS_len(&parameter));
				EVP_DigestFinal_ex(&md_ctx, q, &digest_len);
				q += digest_len;
				md_len += digest_len;
				}
			if (!RSA_verify(NID_md5_sha1, md_buf, md_len,
					CBS_data(&signature), CBS_len(&signature),
					pkey->pkey.rsa))
				{
				al = SSL_AD_DECRYPT_ERROR;
				OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_BAD_SIGNATURE);
				goto f_err;
				}
			}
		else
			{
			if (!EVP_DigestVerifyInit(&md_ctx, NULL, md, NULL, pkey) ||
				!EVP_DigestVerifyUpdate(&md_ctx, s->s3->client_random, SSL3_RANDOM_SIZE) ||
				!EVP_DigestVerifyUpdate(&md_ctx, s->s3->server_random, SSL3_RANDOM_SIZE) ||
				!EVP_DigestVerifyUpdate(&md_ctx, CBS_data(&parameter), CBS_len(&parameter)) ||
				!EVP_DigestVerifyFinal(&md_ctx, CBS_data(&signature), CBS_len(&signature)))
				{
				/* bad signature */
				al=SSL_AD_DECRYPT_ERROR;
				OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_BAD_SIGNATURE);
				goto f_err;
				}
			}
		}
	else
		{
		if (ssl_cipher_has_server_public_key(s->s3->tmp.new_cipher))
			{
			/* Might be wrong key type, check it */
			if (ssl3_check_cert_and_algorithm(s))
				/* Otherwise this shouldn't happen */
				OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, ERR_R_INTERNAL_ERROR);
			goto err;
			}
		/* still data left over */
		if (CBS_len(&server_key_exchange) > 0)
			{
			al=SSL_AD_DECODE_ERROR;
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_key_exchange, SSL_R_EXTRA_DATA_IN_MESSAGE);
			goto f_err;
			}
		}
	EVP_PKEY_free(pkey);
	EVP_MD_CTX_cleanup(&md_ctx);
	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
err:
	EVP_PKEY_free(pkey);
	if (rsa != NULL)
		RSA_free(rsa);
	if (dh != NULL)
		DH_free(dh);
	BN_CTX_free(bn_ctx);
	EC_POINT_free(srvr_ecpoint);
	if (ecdh != NULL)
		EC_KEY_free(ecdh);
	EVP_MD_CTX_cleanup(&md_ctx);
	return(-1);
	}

static int ca_dn_cmp(const X509_NAME **a, const X509_NAME **b)
	{
	return(X509_NAME_cmp(*a,*b));
	}

int ssl3_get_certificate_request(SSL *s)
	{
	int ok,ret=0;
	unsigned long n;
	unsigned int i;
	X509_NAME *xn=NULL;
	STACK_OF(X509_NAME) *ca_sk=NULL;
	CBS cbs;
	CBS certificate_types;
	CBS certificate_authorities;
	const uint8_t *data;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_CERT_REQ_A,
		SSL3_ST_CR_CERT_REQ_B,
		-1,
		s->max_cert_list,
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok) return((int)n);

	s->s3->tmp.cert_req=0;

	if (s->s3->tmp.message_type == SSL3_MT_SERVER_DONE)
		{
		s->s3->tmp.reuse_message=1;
		/* If we get here we don't need any cached handshake records
		 * as we wont be doing client auth.
		 */
		if (s->s3->handshake_buffer)
			{
			if (!ssl3_digest_cached_records(s, free_handshake_buffer))
				goto err;
			}
		return(1);
		}

	if (s->s3->tmp.message_type != SSL3_MT_CERTIFICATE_REQUEST)
		{
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
		OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_WRONG_MESSAGE_TYPE);
		goto err;
		}

	/* TLS does not like anon-DH with client cert */
	if (s->version > SSL3_VERSION)
		{
		if (s->s3->tmp.new_cipher->algorithm_auth & SSL_aNULL)
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_TLS_CLIENT_CERT_REQ_WITH_ANON_CIPHER);
			goto err;
			}
		}

	CBS_init(&cbs, s->init_msg, n);

	ca_sk = sk_X509_NAME_new(ca_dn_cmp);
	if (ca_sk == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	/* get the certificate types */
	if (!CBS_get_u8_length_prefixed(&cbs, &certificate_types))
		{
		ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
		OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_DECODE_ERROR);
		goto err;
		}
	if (!CBS_stow(&certificate_types,
			&s->s3->tmp.certificate_types,
			&s->s3->tmp.num_certificate_types))
		{
		ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
		goto err;
		}
	if (SSL_USE_SIGALGS(s))
		{
		CBS supported_signature_algorithms;
		if (!CBS_get_u16_length_prefixed(&cbs, &supported_signature_algorithms))
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_DECODE_ERROR);
			goto err;
			}
		/* Clear certificate digests and validity flags */
		for (i = 0; i < SSL_PKEY_NUM; i++)
			{
			s->cert->pkeys[i].digest = NULL;
			s->cert->pkeys[i].valid_flags = 0;
			}
		if (!tls1_process_sigalgs(s, &supported_signature_algorithms))
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_SIGNATURE_ALGORITHMS_ERROR);
			goto err;
			}
		}

	/* get the CA RDNs */
	if (!CBS_get_u16_length_prefixed(&cbs, &certificate_authorities))
		{
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
		OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_LENGTH_MISMATCH);
		goto err;
		}

	while (CBS_len(&certificate_authorities) > 0)
		{
		CBS distinguished_name;
		if (!CBS_get_u16_length_prefixed(&certificate_authorities, &distinguished_name))
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_CA_DN_TOO_LONG);
			goto err;
			}

		data = CBS_data(&distinguished_name);
		if ((xn=d2i_X509_NAME(NULL, &data, CBS_len(&distinguished_name))) == NULL)
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, ERR_R_ASN1_LIB);
			goto err;
			}

		if (!CBS_skip(&distinguished_name, data - CBS_data(&distinguished_name)))
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_server_certificate, ERR_R_INTERNAL_ERROR);
			goto err;
			}
		if (CBS_len(&distinguished_name) != 0)
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, SSL_R_CA_DN_LENGTH_MISMATCH);
			goto err;
			}
		if (!sk_X509_NAME_push(ca_sk,xn))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_get_certificate_request, ERR_R_MALLOC_FAILURE);
			goto err;
			}
		}

	/* we should setup a certificate to return.... */
	s->s3->tmp.cert_req=1;
	if (s->s3->tmp.ca_names != NULL)
		sk_X509_NAME_pop_free(s->s3->tmp.ca_names,X509_NAME_free);
	s->s3->tmp.ca_names=ca_sk;
	ca_sk=NULL;

	ret=1;
err:
	if (ca_sk != NULL) sk_X509_NAME_pop_free(ca_sk,X509_NAME_free);
	return(ret);
	}

int ssl3_get_new_session_ticket(SSL *s)
	{
	int ok,al,ret=0;
	long n;
	CBS new_session_ticket, ticket;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_SESSION_TICKET_A,
		SSL3_ST_CR_SESSION_TICKET_B,
		SSL3_MT_NEWSESSION_TICKET,
		16384,
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok)
		return((int)n);

	CBS_init(&new_session_ticket, s->init_msg, n);

	if (!CBS_get_u32(&new_session_ticket, &s->session->tlsext_tick_lifetime_hint) ||
		!CBS_get_u16_length_prefixed(&new_session_ticket, &ticket) ||
		CBS_len(&new_session_ticket) != 0)
		{
		al = SSL_AD_DECODE_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_new_session_ticket, SSL_R_DECODE_ERROR);
		goto f_err;
		}

	if (!CBS_stow(&ticket, &s->session->tlsext_tick, &s->session->tlsext_ticklen))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_get_new_session_ticket, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	/* There are two ways to detect a resumed ticket sesion.
	 * One is to set an appropriate session ID and then the server
	 * must return a match in ServerHello. This allows the normal
	 * client session ID matching to work and we know much 
	 * earlier that the ticket has been accepted.
	 * 
	 * The other way is to set zero length session ID when the
	 * ticket is presented and rely on the handshake to determine
	 * session resumption.
	 *
	 * We choose the former approach because this fits in with
	 * assumptions elsewhere in OpenSSL. The session ID is set
	 * to the SHA256 (or SHA1 is SHA256 is disabled) hash of the
	 * ticket.
	 */ 
	EVP_Digest(CBS_data(&ticket), CBS_len(&ticket),
			s->session->session_id, &s->session->session_id_length,
							EVP_sha256(), NULL);
	ret=1;
	return(ret);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
err:
	return(-1);
	}

int ssl3_get_cert_status(SSL *s)
	{
	int ok, al;
	long n;
	CBS certificate_status, ocsp_response;
	uint8_t status_type;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_CERT_STATUS_A,
		SSL3_ST_CR_CERT_STATUS_B,
		SSL3_MT_CERTIFICATE_STATUS,
		16384,
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok) return((int)n);

	CBS_init(&certificate_status, s->init_msg, n);
	if (!CBS_get_u8(&certificate_status, &status_type) ||
		status_type != TLSEXT_STATUSTYPE_ocsp ||
		!CBS_get_u24_length_prefixed(&certificate_status, &ocsp_response) ||
		CBS_len(&ocsp_response) == 0 ||
		CBS_len(&certificate_status) != 0)
		{
		al = SSL_AD_DECODE_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_cert_status, SSL_R_DECODE_ERROR);
		goto f_err;
		}

	if (!CBS_stow(&ocsp_response,
			&s->session->ocsp_response, &s->session->ocsp_response_length))
		{
		al = SSL_AD_INTERNAL_ERROR;
		OPENSSL_PUT_ERROR(SSL, ssl3_get_cert_status, ERR_R_MALLOC_FAILURE);
		goto f_err;
		}
	return 1;
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
	return(-1);
	}

int ssl3_get_server_done(SSL *s)
	{
	int ok,ret=0;
	long n;

	n=s->method->ssl_get_message(s,
		SSL3_ST_CR_SRVR_DONE_A,
		SSL3_ST_CR_SRVR_DONE_B,
		SSL3_MT_SERVER_DONE,
		30, /* should be very small, like 0 :-) */
		SSL_GET_MESSAGE_HASH_MESSAGE,
		&ok);

	if (!ok) return((int)n);
	if (n > 0)
		{
		/* should contain no data */
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
		OPENSSL_PUT_ERROR(SSL, ssl3_get_server_done, SSL_R_LENGTH_MISMATCH);
		return -1;
		}
	ret=1;
	return(ret);
	}


int ssl3_send_client_key_exchange(SSL *s)
	{
	unsigned char *p;
	int n = 0;
	unsigned long alg_k;
	unsigned long alg_a;
	unsigned char *q;
	EVP_PKEY *pkey=NULL;
	EC_KEY *clnt_ecdh = NULL;
	const EC_POINT *srvr_ecpoint = NULL;
	EVP_PKEY *srvr_pub_pkey = NULL;
	unsigned char *encodedPoint = NULL;
	int encoded_pt_len = 0;
	BN_CTX * bn_ctx = NULL;
	unsigned int psk_len = 0;
	unsigned char psk[PSK_MAX_PSK_LEN];
	uint8_t *pms = NULL;
	size_t pms_len = 0;

	if (s->state == SSL3_ST_CW_KEY_EXCH_A)
		{
		p = ssl_handshake_start(s);

		alg_k=s->s3->tmp.new_cipher->algorithm_mkey;
		alg_a=s->s3->tmp.new_cipher->algorithm_auth;

		/* If using a PSK key exchange, prepare the pre-shared key. */
		if (alg_a & SSL_aPSK)
			{
			char identity[PSK_MAX_IDENTITY_LEN + 1];
			size_t identity_len;

			if (s->psk_client_callback == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, SSL_R_PSK_NO_CLIENT_CB);
				goto err;
				}

			memset(identity, 0, sizeof(identity));
			psk_len = s->psk_client_callback(s, s->session->psk_identity_hint,
				identity, sizeof(identity), psk, sizeof(psk));
			if (psk_len > PSK_MAX_PSK_LEN)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			else if (psk_len == 0)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, SSL_R_PSK_IDENTITY_NOT_FOUND);
				ssl3_send_alert(s, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
				goto err;
				}
			identity_len = OPENSSL_strnlen(identity, sizeof(identity));
			if (identity_len > PSK_MAX_IDENTITY_LEN)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}

			if (s->session->psk_identity != NULL)
				OPENSSL_free(s->session->psk_identity);
			s->session->psk_identity = BUF_strdup(identity);
			if (s->session->psk_identity == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}

			/* Write out psk_identity. */
			s2n(identity_len, p);
			memcpy(p, identity, identity_len);
			p += identity_len;
			n = 2 + identity_len;
			}

		/* Depending on the key exchange method, compute |pms|
		 * and |pms_len|. */
		if (alg_k & SSL_kRSA)
			{
			RSA *rsa;
			size_t enc_pms_len;

			pms_len = SSL_MAX_MASTER_KEY_LENGTH;
			pms = OPENSSL_malloc(pms_len);
			if (pms == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}

			if (s->session->sess_cert == NULL)
				{
				/* We should always have a server certificate with SSL_kRSA. */
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}

			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
			if ((pkey == NULL) ||
				(pkey->type != EVP_PKEY_RSA) ||
				(pkey->pkey.rsa == NULL))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			rsa=pkey->pkey.rsa;
			EVP_PKEY_free(pkey);
				
			pms[0]=s->client_version>>8;
			pms[1]=s->client_version&0xff;
			if (RAND_bytes(&pms[2],SSL_MAX_MASTER_KEY_LENGTH-2) <= 0)
					goto err;

			s->session->master_key_length=SSL_MAX_MASTER_KEY_LENGTH;

			q=p;
			/* In TLS and beyond, reserve space for the length prefix. */
			if (s->version > SSL3_VERSION)
				{
				p += 2;
				n += 2;
				}
			if (!RSA_encrypt(rsa, &enc_pms_len, p, RSA_size(rsa),
					pms, pms_len, RSA_PKCS1_PADDING))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, SSL_R_BAD_RSA_ENCRYPT);
				goto err;
				}
			n += enc_pms_len;

			/* Log the premaster secret, if logging is enabled. */
			if (!ssl_ctx_log_rsa_client_key_exchange(s->ctx,
					p, enc_pms_len, pms, pms_len))
				{
				goto err;
				}

			/* Fill in the length prefix. */
			if (s->version > SSL3_VERSION)
				{
				s2n(enc_pms_len, q);
				}
			}
		else if (alg_k & SSL_kEDH)
			{
			DH *dh_srvr, *dh_clnt;
			SESS_CERT *scert = s->session->sess_cert;
			int dh_len;
			size_t pub_len;

			if (scert == NULL) 
				{
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, SSL_R_UNEXPECTED_MESSAGE);
				goto err;
				}

			if (scert->peer_dh_tmp == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			dh_srvr=scert->peer_dh_tmp;

			/* generate a new random key */
			if ((dh_clnt=DHparams_dup(dh_srvr)) == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_DH_LIB);
				goto err;
				}
			if (!DH_generate_key(dh_clnt))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_DH_LIB);
				DH_free(dh_clnt);
				goto err;
				}

			pms_len = DH_size(dh_clnt);
			pms = OPENSSL_malloc(pms_len);
			if (pms == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				DH_free(dh_clnt);
				goto err;
				}

			dh_len = DH_compute_key(pms, dh_srvr->pub_key, dh_clnt);
			if (dh_len <= 0)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_DH_LIB);
				DH_free(dh_clnt);
				goto err;
				}
			pms_len = dh_len;

			/* send off the data */
			pub_len = BN_num_bytes(dh_clnt->pub_key);
			s2n(pub_len, p);
			BN_bn2bin(dh_clnt->pub_key, p);
			n += 2 + pub_len;

			DH_free(dh_clnt);
			}

		else if (alg_k & SSL_kEECDH)
			{
			const EC_GROUP *srvr_group = NULL;
			EC_KEY *tkey;
			int field_size = 0, ecdh_len;

			if (s->session->sess_cert == NULL) 
				{
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, SSL_R_UNEXPECTED_MESSAGE);
				goto err;
				}

			if (s->session->sess_cert->peer_ecdh_tmp == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			tkey = s->session->sess_cert->peer_ecdh_tmp;

			srvr_group   = EC_KEY_get0_group(tkey);
			srvr_ecpoint = EC_KEY_get0_public_key(tkey);

			if ((srvr_group == NULL) || (srvr_ecpoint == NULL))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}

			if ((clnt_ecdh=EC_KEY_new()) == NULL) 
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}

			if (!EC_KEY_set_group(clnt_ecdh, srvr_group))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_EC_LIB);
				goto err;
				}
			/* Generate a new ECDH key pair */
			if (!(EC_KEY_generate_key(clnt_ecdh)))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_ECDH_LIB);
				goto err;
				}

			field_size = EC_GROUP_get_degree(srvr_group);
			if (field_size <= 0)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_ECDH_LIB);
				goto err;
				}

			pms_len = (field_size + 7) / 8;
			pms = OPENSSL_malloc(pms_len);
			if (pms == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}

			ecdh_len = ECDH_compute_key(pms, pms_len, srvr_ecpoint, clnt_ecdh, NULL);
			if (ecdh_len <= 0)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_ECDH_LIB);
				goto err;
				}
			pms_len = ecdh_len;

			/* First check the size of encoding and
			 * allocate memory accordingly.
			 */
			encoded_pt_len = 
				EC_POINT_point2oct(srvr_group, 
					EC_KEY_get0_public_key(clnt_ecdh), 
					POINT_CONVERSION_UNCOMPRESSED, 
					NULL, 0, NULL);

			encodedPoint = (unsigned char *) 
				OPENSSL_malloc(encoded_pt_len * 
					sizeof(unsigned char)); 
			bn_ctx = BN_CTX_new();
			if ((encodedPoint == NULL) || 
				(bn_ctx == NULL)) 
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}

			/* Encode the public key */
			encoded_pt_len = EC_POINT_point2oct(srvr_group,
				EC_KEY_get0_public_key(clnt_ecdh),
				POINT_CONVERSION_UNCOMPRESSED,
				encodedPoint, encoded_pt_len, bn_ctx);

			*p = encoded_pt_len; /* length of encoded point */
			/* Encoded point will be copied here */
			p += 1;
			n += 1;
			/* copy the point */
			memcpy(p, encodedPoint, encoded_pt_len);
			/* increment n to account for length field */
			n += encoded_pt_len;

			/* Free allocated memory */
			BN_CTX_free(bn_ctx);
			bn_ctx = NULL;
			OPENSSL_free(encodedPoint);
			encodedPoint = NULL;
			EC_KEY_free(clnt_ecdh);
			clnt_ecdh = NULL;
			EVP_PKEY_free(srvr_pub_pkey);
			srvr_pub_pkey = NULL;
			}
		else if (alg_k & SSL_kPSK)
			{
			/* For plain PSK, other_secret is a block of 0s with the same
			 * length as the pre-shared key. */
			pms_len = psk_len;
			pms = OPENSSL_malloc(pms_len);
			if (pms == NULL)
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}
			memset(pms, 0, pms_len);
			}
		else
			{
			ssl3_send_alert(s, SSL3_AL_FATAL,
			    SSL_AD_HANDSHAKE_FAILURE);
			OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
			goto err;
			}

		/* For a PSK cipher suite, other_secret is combined
		 * with the pre-shared key. */
		if ((alg_a & SSL_aPSK) && psk_len != 0)
			{
			CBB cbb, child;
			uint8_t *new_pms;
			size_t new_pms_len;

			if (!CBB_init(&cbb, 2 + psk_len + 2 + pms_len))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_MALLOC_FAILURE);
				goto err;
				}
			if (!CBB_add_u16_length_prefixed(&cbb, &child) ||
				!CBB_add_bytes(&child, pms, pms_len) ||
				!CBB_add_u16_length_prefixed(&cbb, &child) ||
				!CBB_add_bytes(&child, psk, psk_len) ||
				!CBB_finish(&cbb, &new_pms, &new_pms_len))
				{
				CBB_cleanup(&cbb);
				OPENSSL_PUT_ERROR(SSL, ssl3_send_client_key_exchange, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			OPENSSL_cleanse(pms, pms_len);
			OPENSSL_free(pms);
			pms = new_pms;
			pms_len = new_pms_len;
			}

		ssl_set_handshake_header(s, SSL3_MT_CLIENT_KEY_EXCHANGE, n);
		s->state=SSL3_ST_CW_KEY_EXCH_B;

		/* The message must be added to the finished hash before
		 * calculating the master secret. */
		s->method->ssl3_enc->add_to_finished_hash(s);

		s->session->master_key_length =
			s->method->ssl3_enc->generate_master_secret(s,
				s->session->master_key,
				pms, pms_len);
		if (s->session->master_key_length == 0)
			{
			goto err;
			}
		s->session->extended_master_secret = s->s3->tmp.extended_master_secret;
		OPENSSL_cleanse(pms, pms_len);
		OPENSSL_free(pms);
		}

	/* SSL3_ST_CW_KEY_EXCH_B */
	/* The message has already been added to the finished hash. */
	return s->method->ssl3_enc->do_write(s, dont_add_to_finished_hash);

err:
	BN_CTX_free(bn_ctx);
	if (encodedPoint != NULL) OPENSSL_free(encodedPoint);
	if (clnt_ecdh != NULL) 
		EC_KEY_free(clnt_ecdh);
	EVP_PKEY_free(srvr_pub_pkey);
	if (pms)
		{
		OPENSSL_cleanse(pms, pms_len);
		OPENSSL_free(pms);
		}
	return -1;
	}

int ssl3_send_cert_verify(SSL *s)
	{
	unsigned char *buf, *p;
	const EVP_MD *md = NULL;
	uint8_t digest[EVP_MAX_MD_SIZE];
	size_t digest_length;
	EVP_PKEY *pkey;
	EVP_PKEY_CTX *pctx = NULL;
	size_t signature_length = 0;
	unsigned long n = 0;

	buf=(unsigned char *)s->init_buf->data;

	if (s->state == SSL3_ST_CW_CERT_VRFY_A)
		{
		p= ssl_handshake_start(s);
		pkey = s->cert->key->privatekey;

		/* Write out the digest type if needbe. */
		if (SSL_USE_SIGALGS(s))
			{
			md = s->cert->key->digest;
			if (!tls12_get_sigandhash(p, pkey, md))
				{
				OPENSSL_PUT_ERROR(SSL, ssl3_send_cert_verify, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			p += 2;
			n += 2;
			}

		/* Compute the digest. */
		if (!ssl3_cert_verify_hash(s, digest, &digest_length, &md, pkey))
			goto err;

		/* The handshake buffer is no longer necessary. */
		if (s->s3->handshake_buffer && !ssl3_digest_cached_records(s, free_handshake_buffer))
			goto err;

		/* Sign the digest. */
		pctx = EVP_PKEY_CTX_new(pkey, NULL);
		if (pctx == NULL)
			goto err;

		/* Initialize the EVP_PKEY_CTX and determine the size of the signature. */
		if (!EVP_PKEY_sign_init(pctx) ||
			!EVP_PKEY_CTX_set_signature_md(pctx, md) ||
			!EVP_PKEY_sign(pctx, NULL, &signature_length,
				digest, digest_length))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_cert_verify, ERR_R_EVP_LIB);
			goto err;
			}

		if (p + 2 + signature_length > buf + SSL3_RT_MAX_PLAIN_LENGTH)
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_cert_verify, SSL_R_DATA_LENGTH_TOO_LONG);
			goto err;
			}

		if (!EVP_PKEY_sign(pctx, &p[2], &signature_length,
				digest, digest_length))
			{
			OPENSSL_PUT_ERROR(SSL, ssl3_send_cert_verify, ERR_R_EVP_LIB);
			goto err;
			}

		s2n(signature_length, p);
		n += signature_length + 2;

		ssl_set_handshake_header(s, SSL3_MT_CERTIFICATE_VERIFY, n);
		s->state=SSL3_ST_CW_CERT_VRFY_B;
		}
	EVP_PKEY_CTX_free(pctx);
	return ssl_do_write(s);
err:
	EVP_PKEY_CTX_free(pctx);
	return(-1);
	}

/* Check a certificate can be used for client authentication. Currently
 * check the cert exists and if we have a suitable digest for TLS 1.2.
 */
static int ssl3_check_client_certificate(SSL *s)
	{
	if (!s->cert || !s->cert->key->x509 || !s->cert->key->privatekey)
		return 0;
	/* If no suitable signature algorithm can't use certificate */
	if (SSL_USE_SIGALGS(s) && !s->cert->key->digest)
		return 0;
	/* If strict mode check suitability of chain before using it.
	 */
	if (s->cert->cert_flags & SSL_CERT_FLAGS_CHECK_TLS_STRICT &&
		!tls1_check_chain(s, NULL, NULL, NULL, -2))
		return 0;
	return 1;
	}

int ssl3_send_client_certificate(SSL *s)
	{
	X509 *x509=NULL;
	EVP_PKEY *pkey=NULL;
	int i;

	if (s->state ==	SSL3_ST_CW_CERT_A)
		{
		/* Let cert callback update client certificates if required */
		if (s->cert->cert_cb)
			{
			i = s->cert->cert_cb(s, s->cert->cert_cb_arg);
			if (i < 0)
				{
				s->rwstate=SSL_X509_LOOKUP;
				return -1;
				}
			if (i == 0)
				{
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_INTERNAL_ERROR);
				return 0;
				}
			s->rwstate=SSL_NOTHING;
			}
		if (ssl3_check_client_certificate(s))
			s->state=SSL3_ST_CW_CERT_C;
		else
			s->state=SSL3_ST_CW_CERT_B;
		}

	/* We need to get a client cert */
	if (s->state == SSL3_ST_CW_CERT_B)
		{
		/* If we get an error, we need to
		 * ssl->rwstate=SSL_X509_LOOKUP; return(-1);
		 * We then get retried later */
		i = ssl_do_client_cert_cb(s, &x509, &pkey);
		if (i < 0)
			{
			s->rwstate=SSL_X509_LOOKUP;
			return(-1);
			}
		s->rwstate=SSL_NOTHING;
		if ((i == 1) && (pkey != NULL) && (x509 != NULL))
			{
			s->state=SSL3_ST_CW_CERT_B;
			if (	!SSL_use_certificate(s,x509) ||
				!SSL_use_PrivateKey(s,pkey))
				i=0;
			}
		else if (i == 1)
			{
			i=0;
			OPENSSL_PUT_ERROR(SSL, ssl3_send_client_certificate, SSL_R_BAD_DATA_RETURNED_BY_CALLBACK);
			}

		if (x509 != NULL) X509_free(x509);
		if (pkey != NULL) EVP_PKEY_free(pkey);
		if (i && !ssl3_check_client_certificate(s))
			i = 0;
		if (i == 0)
			{
			if (s->version == SSL3_VERSION)
				{
				s->s3->tmp.cert_req=0;
				ssl3_send_alert(s,SSL3_AL_WARNING,SSL_AD_NO_CERTIFICATE);
				return(1);
				}
			else
				{
				s->s3->tmp.cert_req=2;
				}
			}

		/* Ok, we have a cert */
		s->state=SSL3_ST_CW_CERT_C;
		}

	if (s->state == SSL3_ST_CW_CERT_C)
		{
		s->state=SSL3_ST_CW_CERT_D;
		ssl3_output_cert_chain(s,
			(s->s3->tmp.cert_req == 2)?NULL:s->cert->key);
		}
	/* SSL3_ST_CW_CERT_D */
	return ssl_do_write(s);
	}

#define has_bits(i,m)	(((i)&(m)) == (m))

int ssl3_check_cert_and_algorithm(SSL *s)
	{
	int i,idx;
	long alg_k,alg_a;
	EVP_PKEY *pkey=NULL;
	SESS_CERT *sc;
	DH *dh;

	/* we don't have a certificate */
	if (!ssl_cipher_has_server_public_key(s->s3->tmp.new_cipher))
		return 1;

	alg_k=s->s3->tmp.new_cipher->algorithm_mkey;
	alg_a=s->s3->tmp.new_cipher->algorithm_auth;

	sc=s->session->sess_cert;
	if (sc == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, ERR_R_INTERNAL_ERROR);
		goto err;
		}

	dh=s->session->sess_cert->peer_dh_tmp;

	/* This is the passed certificate */

	idx=sc->peer_cert_type;
	if (idx == SSL_PKEY_ECC)
		{
		if (ssl_check_srvr_ecc_cert_and_alg(sc->peer_pkeys[idx].x509,
		    						s) == 0) 
			{ /* check failed */
			OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, SSL_R_BAD_ECC_CERT);
			goto f_err;
			}
		else 
			{
			return 1;
			}
		}
	else if (alg_a & SSL_aECDSA)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, SSL_R_MISSING_ECDSA_SIGNING_CERT);
		goto f_err;
		}
	pkey=X509_get_pubkey(sc->peer_pkeys[idx].x509);
	i=X509_certificate_type(sc->peer_pkeys[idx].x509,pkey);
	EVP_PKEY_free(pkey);

	
	/* Check that we have a certificate if we require one */
	if ((alg_a & SSL_aRSA) && !has_bits(i,EVP_PK_RSA|EVP_PKT_SIGN))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, SSL_R_MISSING_RSA_SIGNING_CERT);
		goto f_err;
		}
	if ((alg_k & SSL_kRSA) && !has_bits(i,EVP_PK_RSA|EVP_PKT_ENC))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, SSL_R_MISSING_RSA_ENCRYPTING_CERT);
		goto f_err;
		}
	if ((alg_k & SSL_kEDH) && 
		!(has_bits(i,EVP_PK_DH|EVP_PKT_EXCH) || (dh != NULL)))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_check_cert_and_algorithm, SSL_R_MISSING_DH_KEY);
		goto f_err;
		}

	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_HANDSHAKE_FAILURE);
err:
	return(0);
	}

int ssl3_send_next_proto(SSL *s)
	{
	unsigned int len, padding_len;
	unsigned char *d;

	if (s->state == SSL3_ST_CW_NEXT_PROTO_A)
		{
		len = s->next_proto_negotiated_len;
		padding_len = 32 - ((len + 2) % 32);
		d = (unsigned char *)s->init_buf->data;
		d[4] = len;
		memcpy(d + 5, s->next_proto_negotiated, len);
		d[5 + len] = padding_len;
		memset(d + 6 + len, 0, padding_len);
		*(d++)=SSL3_MT_NEXT_PROTO;
		l2n3(2 + len + padding_len, d);
		s->state = SSL3_ST_CW_NEXT_PROTO_B;
		s->init_num = 4 + 2 + len + padding_len;
		s->init_off = 0;
		}

	return ssl3_do_write(s, SSL3_RT_HANDSHAKE, add_to_finished_hash);
}


int ssl3_send_channel_id(SSL *s)
	{
	unsigned char *d;
	int ret = -1, public_key_len;
	EVP_MD_CTX md_ctx;
	size_t sig_len;
	ECDSA_SIG *sig = NULL;
	unsigned char *public_key = NULL, *derp, *der_sig = NULL;

	if (s->state != SSL3_ST_CW_CHANNEL_ID_A)
		return ssl3_do_write(s, SSL3_RT_HANDSHAKE, add_to_finished_hash);

	if (!s->tlsext_channel_id_private && s->ctx->channel_id_cb)
		{
		EVP_PKEY *key = NULL;
		s->ctx->channel_id_cb(s, &key);
		if (key != NULL)
			{
			s->tlsext_channel_id_private = key;
			}
		}
	if (!s->tlsext_channel_id_private)
		{
		s->rwstate=SSL_CHANNEL_ID_LOOKUP;
		return (-1);
		}
	s->rwstate=SSL_NOTHING;

	d = (unsigned char *)s->init_buf->data;
	*(d++)=SSL3_MT_ENCRYPTED_EXTENSIONS;
	l2n3(2 + 2 + TLSEXT_CHANNEL_ID_SIZE, d);
	if (s->s3->tlsext_channel_id_new)
		s2n(TLSEXT_TYPE_channel_id_new, d);
	else
		s2n(TLSEXT_TYPE_channel_id, d);
	s2n(TLSEXT_CHANNEL_ID_SIZE, d);

	EVP_MD_CTX_init(&md_ctx);

	public_key_len = i2d_PublicKey(s->tlsext_channel_id_private, NULL);
	if (public_key_len <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_CANNOT_SERIALIZE_PUBLIC_KEY);
		goto err;
		}
	/* i2d_PublicKey will produce an ANSI X9.62 public key which, for a
	 * P-256 key, is 0x04 (meaning uncompressed) followed by the x and y
	 * field elements as 32-byte, big-endian numbers. */
	if (public_key_len != 65)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_CHANNEL_ID_NOT_P256);
		goto err;
		}
	public_key = OPENSSL_malloc(public_key_len);
	if (!public_key)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	derp = public_key;
	i2d_PublicKey(s->tlsext_channel_id_private, &derp);

	if (EVP_DigestSignInit(&md_ctx, NULL, EVP_sha256(), NULL,
			       s->tlsext_channel_id_private) != 1)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_EVP_DIGESTSIGNINIT_FAILED);
		goto err;
		}

	if (!tls1_channel_id_hash(&md_ctx, s))
		goto err;

	if (!EVP_DigestSignFinal(&md_ctx, NULL, &sig_len))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_EVP_DIGESTSIGNFINAL_FAILED);
		goto err;
		}

	der_sig = OPENSSL_malloc(sig_len);
	if (!der_sig)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (!EVP_DigestSignFinal(&md_ctx, der_sig, &sig_len))
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_EVP_DIGESTSIGNFINAL_FAILED);
		goto err;
		}

	derp = der_sig;
	sig = d2i_ECDSA_SIG(NULL, (const unsigned char**) &derp, sig_len);
	if (sig == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl3_send_channel_id, SSL_R_D2I_ECDSA_SIG);
		goto err;
		}

	/* The first byte of public_key will be 0x4, denoting an uncompressed key. */
	memcpy(d, public_key + 1, 64);
	d += 64;
	memset(d, 0, 2 * 32);
	BN_bn2bin(sig->r, d + 32 - BN_num_bytes(sig->r));
	d += 32;
	BN_bn2bin(sig->s, d + 32 - BN_num_bytes(sig->s));
	d += 32;

	s->state = SSL3_ST_CW_CHANNEL_ID_B;
	s->init_num = 4 + 2 + 2 + TLSEXT_CHANNEL_ID_SIZE;
	s->init_off = 0;

	ret = ssl3_do_write(s, SSL3_RT_HANDSHAKE, add_to_finished_hash);

err:
	EVP_MD_CTX_cleanup(&md_ctx);
	if (public_key)
		OPENSSL_free(public_key);
	if (der_sig)
		OPENSSL_free(der_sig);
	if (sig)
		ECDSA_SIG_free(sig);

	return ret;
	}

int ssl_do_client_cert_cb(SSL *s, X509 **px509, EVP_PKEY **ppkey)
	{
	int i = 0;
	if (s->ctx->client_cert_cb)
		i = s->ctx->client_cert_cb(s,px509,ppkey);
	return i;
	}
