//-----------------------------------------------------------------------------
// Copyright (C) 2018 Merlok
// Copyright (C) 2018 drHatson
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// crypto commands
//-----------------------------------------------------------------------------

#include "polarssl/libpcrypto.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mbedtls/aes.h>
#include <mbedtls/cmac.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include "mbedtls/ctr_drbg.h"
//test!!!
#include <util.h>

// NIST Special Publication 800-38A — Recommendation for block cipher modes of operation: methods and techniques, 2001.
int aes_encode(uint8_t *iv, uint8_t *key, uint8_t *input, uint8_t *output, int length){
	uint8_t iiv[16] = {0};
	if (iv)
		memcpy(iiv, iv, 16);
	
	mbedtls_aes_context aes;
	mbedtls_aes_init(&aes);
	if (mbedtls_aes_setkey_enc(&aes, key, 128))
		return 1;
	if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iiv, input, output))
		return 2;
	mbedtls_aes_free(&aes);

	return 0;
}

int aes_decode(uint8_t *iv, uint8_t *key, uint8_t *input, uint8_t *output, int length){
	uint8_t iiv[16] = {0};
	if (iv)
		memcpy(iiv, iv, 16);
	
	mbedtls_aes_context aes;
	mbedtls_aes_init(&aes);
	if (mbedtls_aes_setkey_dec(&aes, key, 128))
		return 1;
	if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iiv, input, output))
		return 2;
	mbedtls_aes_free(&aes);

	return 0;
}

// NIST Special Publication 800-38B — Recommendation for block cipher modes of operation: The CMAC mode for authentication.
// https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/AES_CMAC.pdf
int aes_cmac(uint8_t *iv, uint8_t *key, uint8_t *input, uint8_t *mac, int length) {
	memset(mac, 0x00, 16);
	
	//  NIST 800-38B 
	return mbedtls_aes_cmac_prf_128(key, MBEDTLS_AES_BLOCK_SIZE, input, length, mac);
}

int aes_cmac8(uint8_t *iv, uint8_t *key, uint8_t *input, uint8_t *mac, int length) {
	uint8_t cmac[16] = {0};
	memset(mac, 0x00, 8);
	
	int res = aes_cmac(iv, key, input, cmac, length);
	if (res)
		return res;
	
	for(int i = 0; i < 8; i++) 
		mac[i] = cmac[i * 2 + 1];

	return 0;
}

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
{                                                       \
	(b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
	(b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
	(b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
	(b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

/**
 * This function just returns data from rand().
 * Although predictable and often similar on multiple
 * runs, this does not result in identical random on
 * each run. So do not use this if the results of a
 * test depend on the random data that is generated.
 *
 * rng_state shall be NULL.
 */
static int rnd_std_rand( void *rng_state, unsigned char *output, size_t len )
{
#if !defined(__OpenBSD__)
	size_t i;

	if( rng_state != NULL )
		rng_state  = NULL;

	for( i = 0; i < len; ++i )
		output[i] = rand();
#else
	if( rng_state != NULL )
		rng_state = NULL;

	arc4random_buf( output, len );
#endif /* !OpenBSD */

	return( 0 );
}

/**
 * This function only returns zeros
 *
 * rng_state shall be NULL.
 */
static int rnd_zero_rand( void *rng_state, unsigned char *output, size_t len )
{
	if( rng_state != NULL )
		rng_state  = NULL;

	memset( output, 0, len );

	return( 0 );
}

typedef struct
{
	unsigned char *buf;
	size_t length;
} rnd_buf_info;

/**
 * This function returns random based on a buffer it receives.
 *
 * rng_state shall be a pointer to a rnd_buf_info structure.
 *
 * The number of bytes released from the buffer on each call to
 * the random function is specified by per_call. (Can be between
 * 1 and 4)
 *
 * After the buffer is empty it will return rand();
 */
static int rnd_buffer_rand( void *rng_state, unsigned char *output, size_t len )
{
	rnd_buf_info *info = (rnd_buf_info *) rng_state;
	size_t use_len;

	if( rng_state == NULL )
		return( rnd_std_rand( NULL, output, len ) );

	use_len = len;
	if( len > info->length )
		use_len = info->length;

	if( use_len )
	{
		memcpy( output, info->buf, use_len );
		info->buf += use_len;
		info->length -= use_len;
	}

	if( len - use_len > 0 )
		return( rnd_std_rand( NULL, output + use_len, len - use_len ) );

	printf("rnd[%d] %s\n", len, sprint_hex_inrow(output, len));
	return( 0 );
}

/**
 * Info structure for the pseudo random function
 *
 * Key should be set at the start to a test-unique value.
 * Do not forget endianness!
 * State( v0, v1 ) should be set to zero.
 */
typedef struct
{
	uint32_t key[16];
	uint32_t v0, v1;
} rnd_pseudo_info;

/**
 * This function returns random based on a pseudo random function.
 * This means the results should be identical on all systems.
 * Pseudo random is based on the XTEA encryption algorithm to
 * generate pseudorandom.
 *
 * rng_state shall be a pointer to a rnd_pseudo_info structure.
 */
static int rnd_pseudo_rand( void *rng_state, unsigned char *output, size_t len )
{
	rnd_pseudo_info *info = (rnd_pseudo_info *) rng_state;
	uint32_t i, *k, sum, delta=0x9E3779B9;
	unsigned char result[4], *out = output;

	if( rng_state == NULL )
		return( rnd_std_rand( NULL, output, len ) );

	k = info->key;

	while( len > 0 )
	{
		size_t use_len = ( len > 4 ) ? 4 : len;
		sum = 0;

		for( i = 0; i < 32; i++ )
		{
			info->v0 += ( ( ( info->v1 << 4 ) ^ ( info->v1 >> 5 ) )
							+ info->v1 ) ^ ( sum + k[sum & 3] );
			sum += delta;
			info->v1 += ( ( ( info->v0 << 4 ) ^ ( info->v0 >> 5 ) )
							+ info->v0 ) ^ ( sum + k[( sum>>11 ) & 3] );
		}

		PUT_UINT32_BE( info->v0, result, 0 );
		memcpy( out, result, use_len );
		len -= use_len;
		out += 4;
	}

	return( 0 );
}

#define T_PRIVATE_KEY "C477F9F65C22CCE20657FAA5B2D1D8122336F851A508A1ED04E479C34985BF96"
#define T_Q_X         "B7E08AFDFE94BAD3F1DC8C734798BA1C62B3A0AD1E9EA2A38201CD0889BC7A19"
#define T_Q_Y         "3603F747959DBF7A4BB226E41928729063ADC7AE43529E61B563BBC606CC5E09"
#define T_K           "7A1A7E52797FC8CAAA435D2A4DACE39158504BF204FBE19F14DBB427FAEE50AE"
#define T_R           "2B42F576D07F4165FF65D1F3B1500F81E44C316F1F0B3EF57325B69ACA46104F"
#define T_S           "DC42C2122D6392CD3E3A993A89502A8198C1886FE69D262C4B329BDB6B63FAF1"

static int fixed_rand( void *rng_state, unsigned char *output, size_t len ) {
	memset(output, 0x00, len);
	if (len <= 32) {
		uint8_t rnd[33] = {0};
		int rndlen = 0;
		param_gethex_to_eol(T_K, 0, rnd, sizeof(rnd), &rndlen);
		memcpy(output, rnd, len);
	}
	return 0;
}

int ecdsa_signature_verify(uint8_t *key_xy, uint8_t *input, uint8_t *mac, int length) {
	int ret;
	
	uint8_t shahash[32] = {0}; // SHA-256
	
	mbedtls_sha256_context sctx;
	mbedtls_sha256_init(&sctx);
	mbedtls_sha256_starts(&sctx, 0); // SHA-256, not 224 
	mbedtls_sha256_update(&sctx, input, length);
	mbedtls_sha256_finish(&sctx, shahash);	
	mbedtls_sha256_free(&sctx);
	printf("hash: %s\n", sprint_hex(shahash, sizeof(shahash)));
	
	int res;
	
	mbedtls_ecdsa_context ctx;	
	mbedtls_ecdsa_init(&ctx);
	// secp256r1
	mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
	mbedtls_mpi_read_string(&ctx.d, 16, T_PRIVATE_KEY);
	mbedtls_ecp_point_read_string(&ctx.Q, 16, T_Q_X, T_Q_Y);
	
//	mbedtls_ctr_drbg_context ctr_drbg;
//	mbedtls_ctr_drbg_init(&ctr_drbg);

	// init keys
	uint8_t buf[257] = {0};
	size_t buflen = 0;
	mbedtls_mpi_write_string(&ctx.d, 16, (char *)buf, sizeof(buf), &buflen);
	printf("prvkey[%d]: %s\n", buflen, buf);
	mbedtls_ecp_point_write_binary(&ctx.grp, &ctx.Q, MBEDTLS_ECP_PF_UNCOMPRESSED,
								   &buflen, buf, sizeof(buf));
	printf("pubkey[%d]: %s\n", buflen, sprint_hex_inrow(buf, buflen));


	// make signature
	uint8_t signature[300] = {0}; 
	size_t siglen = 0; 
	res = mbedtls_ecdsa_write_signature(&ctx, MBEDTLS_MD_SHA256, shahash, sizeof(shahash), signature, &siglen, fixed_rand, NULL);
	printf("res: %x signature[%x]: %s\n", (res<0)?-res:res, siglen, sprint_hex(signature, siglen));

	// check vectors
	
	
	// verify signature
	res = mbedtls_ecdsa_read_signature(&ctx, shahash, sizeof(shahash), signature, siglen);
	printf("signature check res: %x\n", (res<0)?-res:res);
		
	// verify wrong signature
	shahash[0] ^= 0xFF;
	res = mbedtls_ecdsa_read_signature(&ctx, shahash, sizeof(shahash), signature, siglen);
	printf("wrong signature check res: %x\n", (res<0)?-res:res);

	ret = 1;
	goto exit;
exit:
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_ecdsa_free(&ctx);
	return ret;
}
