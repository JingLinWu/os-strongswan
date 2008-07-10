/*
 * Copyright (C) 2008 Tobias Brunner
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * $Id: openssl_rsa_private_key.c 3963 2008-05-15 12:41:06Z tobias $
 */

#include "openssl_rsa_private_key.h"
#include "openssl_rsa_public_key.h"

#include <debug.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>

/**
 *  Public exponent to use for key generation.
 */
#define PUBLIC_EXPONENT 0x10001

typedef struct private_openssl_rsa_private_key_t private_openssl_rsa_private_key_t;

/**
 * Private data of a openssl_rsa_private_key_t object.
 */
struct private_openssl_rsa_private_key_t {
	/**
	 * Public interface for this signer.
	 */
	openssl_rsa_private_key_t public;
	
	/**
	 * RSA object from OpenSSL
	 */
	RSA *rsa;

	/**
	 * Keyid formed as a SHA-1 hash of a privateKey object
	 */
	identification_t* keyid;

	/**
	 * Keyid formed as a SHA-1 hash of a privateKeyInfo object
	 */
	identification_t* keyid_info;
	
	/**
	 * reference count
	 */
	refcount_t ref;	
};

/**
 * shared functions, implemented in openssl_rsa_public_key.c
 */
bool openssl_rsa_public_key_build_id(RSA *rsa, identification_t **keyid,
								 identification_t **keyid_info);


openssl_rsa_public_key_t *openssl_rsa_public_key_create_from_n_e(BIGNUM *n, BIGNUM *e);


/**
 * Build an EMPSA PKCS1 signature described in PKCS#1
 */
static bool build_emsa_pkcs1_signature(private_openssl_rsa_private_key_t *this,
									   int type, chunk_t data, chunk_t *signature)
{
	bool success = FALSE;
	const EVP_MD *hasher = EVP_get_digestbynid(type);
	if (!hasher)
	{
		return FALSE;
	}
	
	EVP_MD_CTX *ctx = EVP_MD_CTX_create();
	EVP_PKEY *key = EVP_PKEY_new();
	if (!ctx || !key)
	{
		goto error;
	}
	
	if (!EVP_PKEY_set1_RSA(key, this->rsa))
	{
		goto error;
	}
	
	if (!EVP_SignInit_ex(ctx, hasher, NULL))
	{
		goto error;
	}
	
	if (!EVP_SignUpdate(ctx, data.ptr, data.len))
	{
		goto error;
	}
	
	*signature = chunk_alloc(RSA_size(this->rsa));
	
	if (!EVP_SignFinal(ctx, signature->ptr, &signature->len, key))
	{
		goto error;
	}
	
	success = TRUE;
	
error:
	if (key)
	{
		EVP_PKEY_free(key);
	}
	if (ctx)
	{
		EVP_MD_CTX_destroy(ctx);
	}
	return success;
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static key_type_t get_type(private_openssl_rsa_private_key_t *this)
{
	return KEY_RSA;
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static bool sign(private_openssl_rsa_private_key_t *this, signature_scheme_t scheme, 
				 chunk_t data, chunk_t *signature)
{
	switch (scheme)
	{
		case SIGN_DEFAULT:
			/* default is EMSA-PKCS1 using SHA1 */
		case SIGN_RSA_EMSA_PKCS1_SHA1:
			return build_emsa_pkcs1_signature(this, NID_sha1, data, signature);
		case SIGN_RSA_EMSA_PKCS1_SHA256:
			return build_emsa_pkcs1_signature(this, NID_sha256, data, signature);
		case SIGN_RSA_EMSA_PKCS1_SHA384:
			return build_emsa_pkcs1_signature(this, NID_sha384, data, signature);
		case SIGN_RSA_EMSA_PKCS1_SHA512:
			return build_emsa_pkcs1_signature(this, NID_sha512, data, signature);
		case SIGN_RSA_EMSA_PKCS1_MD5:
			return build_emsa_pkcs1_signature(this, NID_md5, data, signature);
		default:
			DBG1("signature scheme %N not supported in RSA",
				 signature_scheme_names, scheme);
			return FALSE;
	}
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static bool decrypt(private_openssl_rsa_private_key_t *this,
					chunk_t crypto, chunk_t *plain)
{
	DBG1("RSA private key decryption not implemented");
	return FALSE;
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static size_t get_keysize(private_openssl_rsa_private_key_t *this)
{
	return RSA_size(this->rsa);
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static identification_t* get_id(private_openssl_rsa_private_key_t *this,
								id_type_t type)
{
	switch (type)
	{
		case ID_PUBKEY_INFO_SHA1:
			return this->keyid_info;
		case ID_PUBKEY_SHA1:
			return this->keyid;
		default:
			return NULL;
	}
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static openssl_rsa_public_key_t* get_public_key(private_openssl_rsa_private_key_t *this)
{
	return openssl_rsa_public_key_create_from_n_e(this->rsa->n, this->rsa->e);
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static bool belongs_to(private_openssl_rsa_private_key_t *this, public_key_t *public)
{
	identification_t *keyid;

	if (public->get_type(public) != KEY_RSA)
	{
		return FALSE;
	}
	keyid = public->get_id(public, ID_PUBKEY_SHA1);
	if (keyid && keyid->equals(keyid, this->keyid))
	{
		return TRUE;
	}
	keyid = public->get_id(public, ID_PUBKEY_INFO_SHA1);
	if (keyid && keyid->equals(keyid, this->keyid_info))
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Implementation of private_key_t.get_encoding.
 */
static chunk_t get_encoding(private_openssl_rsa_private_key_t *this)
{
	chunk_t enc = chunk_alloc(i2d_RSAPrivateKey(this->rsa, NULL));
	u_char *p = enc.ptr;
	i2d_RSAPrivateKey(this->rsa, &p);
	return enc;
}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static private_openssl_rsa_private_key_t* get_ref(private_openssl_rsa_private_key_t *this)
{
	ref_get(&this->ref);
	return this;

}

/**
 * Implementation of openssl_rsa_private_key.destroy.
 */
static void destroy(private_openssl_rsa_private_key_t *this)
{
	if (ref_put(&this->ref))
	{
		if (this->rsa)
		{
			RSA_free(this->rsa);
		}
		DESTROY_IF(this->keyid);
		DESTROY_IF(this->keyid_info);
		free(this);
	}
}

/**
 * Internal generic constructor
 */
static private_openssl_rsa_private_key_t *openssl_rsa_private_key_create_empty(void)
{
	private_openssl_rsa_private_key_t *this = malloc_thing(private_openssl_rsa_private_key_t);
	
	this->public.interface.get_type = (key_type_t (*)(private_key_t *this))get_type;
	this->public.interface.sign = (bool (*)(private_key_t *this, signature_scheme_t scheme, chunk_t data, chunk_t *signature))sign;
	this->public.interface.decrypt = (bool (*)(private_key_t *this, chunk_t crypto, chunk_t *plain))decrypt;
	this->public.interface.get_keysize = (size_t (*) (private_key_t *this))get_keysize;
	this->public.interface.get_id = (identification_t* (*) (private_key_t *this,id_type_t))get_id;
	this->public.interface.get_public_key = (public_key_t* (*)(private_key_t *this))get_public_key;
	this->public.interface.belongs_to = (bool (*) (private_key_t *this, public_key_t *public))belongs_to;
	this->public.interface.get_encoding = (chunk_t(*)(private_key_t*))get_encoding;
	this->public.interface.get_ref = (private_key_t* (*)(private_key_t *this))get_ref;
	this->public.interface.destroy = (void (*)(private_key_t *this))destroy;
	
	this->keyid = NULL;
	this->keyid_info = NULL;
	this->ref = 1;
	
	return this;
}

/**
 * Generate an RSA key of specified key size
 */
static openssl_rsa_private_key_t *generate(size_t key_size)
{
	private_openssl_rsa_private_key_t *this = openssl_rsa_private_key_create_empty();
	
	this->rsa = RSA_generate_key(key_size, PUBLIC_EXPONENT, NULL, NULL);
	
	if (!openssl_rsa_public_key_build_id(this->rsa, &this->keyid, &this->keyid_info))
	{
		destroy(this);
		return NULL;
	}
	
	return &this->public;
}

/**
 * load private key from an ASN1 encoded blob
 */
static openssl_rsa_private_key_t *load(chunk_t blob)
{
	u_char *p = blob.ptr;
	private_openssl_rsa_private_key_t *this = openssl_rsa_private_key_create_empty();
	
	this->rsa = d2i_RSAPrivateKey(NULL, (const u_char**)&p, blob.len);
	
	chunk_clear(&blob);

	if (!this->rsa)
	{
		destroy(this);
		return NULL;
	}
	
	if (!openssl_rsa_public_key_build_id(this->rsa, &this->keyid, &this->keyid_info))
	{
		destroy(this);
		return NULL;
	}
	
	if (!RSA_check_key(this->rsa))
	{
		destroy(this);
		return NULL;
	}
	
	return &this->public;
}

typedef struct private_builder_t private_builder_t;
/**
 * Builder implementation for key loading/generation
 */
struct private_builder_t {
	/** implements the builder interface */
	builder_t public;
	/** loaded/generated private key */
	openssl_rsa_private_key_t *key;
};

/**
 * Implementation of builder_t.build
 */
static openssl_rsa_private_key_t *build(private_builder_t *this)
{
	openssl_rsa_private_key_t *key = this->key;
	
	free(this);
	return key;
}

/**
 * Implementation of builder_t.add
 */
static void add(private_builder_t *this, builder_part_t part, ...)
{
	va_list args;
	
	if (this->key)
	{
		DBG1("ignoring surplus build part %N", builder_part_names, part);
		return;
	}
	
	switch (part)
	{
		case BUILD_BLOB_ASN1_DER:
		{
			va_start(args, part);
			this->key = load(va_arg(args, chunk_t));
			va_end(args);
			break;
		}		
		case BUILD_KEY_SIZE:
		{
			va_start(args, part);
			this->key = generate(va_arg(args, u_int));
			va_end(args);
			break;
		}
		default:
			DBG1("ignoring unsupported build part %N", builder_part_names, part);
			break;
	}
}

/**
 * Builder construction function
 */
builder_t *openssl_rsa_private_key_builder(key_type_t type)
{
	private_builder_t *this;
	
	if (type != KEY_RSA)
	{
		return NULL;
	}
	
	this = malloc_thing(private_builder_t);
	
	this->key = NULL;
	this->public.add = (void(*)(builder_t *this, builder_part_t part, ...))add;
	this->public.build = (void*(*)(builder_t *this))build;
	
	return &this->public;
}

