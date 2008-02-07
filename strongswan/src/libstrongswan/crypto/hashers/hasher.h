/**
 * @file hasher.h
 * 
 * @brief Interface hasher_t.
 * 
 */

/*
 * Copyright (C) 2005-2006 Martin Willi
 * Copyright (C) 2005 Jan Hutter
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
 * RCSID $Id: hasher.h 3307 2007-10-17 02:56:24Z andreas $
 */

#ifndef HASHER_H_
#define HASHER_H_

typedef enum hash_algorithm_t hash_algorithm_t;
typedef struct hasher_t hasher_t;

#include <library.h>

/**
 * @brief Algorithms to use for hashing.
 *
 * Currently only the following algorithms are implemented:
 * - HASH_MD5
 * - HASH_SHA1
 * - HASH_SHA256
 * - HASH_SHA384
 * - HASH_SHA512
 *
 * @ingroup hashers
 */
enum hash_algorithm_t {
	HASH_UNKNOWN = 0,
	HASH_MD2 =     1,
	/** Implemented in class md5_hasher_t */
	HASH_MD5 =     2,
	/** Implemented in class sha1_hasher_t */
	HASH_SHA1 =    3,
	/** Implemented in class sha2_hasher_t */
	HASH_SHA256 =  4,
	/** Implemented in class sha2_hasher_t */
	HASH_SHA384 =  5,
	/** Implemented in class sha2_hasher_t */
	HASH_SHA512 =  6,
};

#define HASH_SIZE_MD2		16
#define HASH_SIZE_MD5		16
#define HASH_SIZE_SHA1		20
#define HASH_SIZE_SHA256	32
#define HASH_SIZE_SHA384	48
#define HASH_SIZE_SHA512	64
#define HASH_SIZE_MAX		64

/**
 * enum names for hash_algorithm_t.
 */
extern enum_name_t *hash_algorithm_names;

/**
 * @brief Generic interface for all hash functions.
 * 
 * @b Constructors:
 *  - hasher_create()
 * 
 * @ingroup hashers
 */
struct hasher_t {
	/**
	 * @brief Hash data and write it in the buffer.
	 * 
	 * If the parameter hash is NULL, no result is written back
	 * and more data can be appended to already hashed data.
	 * If not, the result is written back and the hasher is reset.
	 * 
	 * The hash output parameter must hold at least
	 * hash_t.get_block_size() bytes.
	 * 
	 * @param this			calling object
	 * @param data			data to hash
	 * @param[out] hash		pointer where the hash will be written
	 */
	void (*get_hash) (hasher_t *this, chunk_t data, u_int8_t *hash);
	
	/**
	 * @brief Hash data and allocate space for the hash.
	 * 
	 * If the parameter hash is NULL, no result is written back
	 * and more data can be appended to already hashed data.
	 * If not, the result is written back and the hasher is reset.
	 * 
	 * @param this			calling object
	 * @param data			chunk with data to hash
	 * @param[out] hash		chunk which will hold allocated hash
	 */
	void (*allocate_hash) (hasher_t *this, chunk_t data, chunk_t *hash);
	
	/**
	 * @brief Get the size of the resulting hash.
	 * 
	 * @param this			calling object
	 * @return				hash size in bytes
	 */
	size_t (*get_hash_size) (hasher_t *this);
	
	/**
	 * @brief Resets the hashers state.
	 * 
	 * @param this			calling object
	 */
	void (*reset) (hasher_t *this);
	
	/**
	 * @brief Get the state of the hasher.
	 *
	 * A hasher stores internal state information. This state may be
	 * manipulated to include a "seed" into the hashing operation. It used by
	 * some exotic protocols (such as AKA).
	 * The data pointed by chunk may be manipulated, but not replaced nor freed.
	 * This is more a hack than a feature. The hasher's state may be byte
	 * order dependant; use with care.
	 *
	 * @param this			calling object
	 */
	chunk_t (*get_state) (hasher_t *this);
	
	/**
	 * @brief Destroys a hasher object.
	 *
	 * @param this 			calling object
	 */
	void (*destroy) (hasher_t *this);
};

/**
 * @brief Generic interface to create a hasher_t.
 * 
 * @param hash_algorithm	Algorithm to use for hashing
 * @return
 * 							- hasher_t object
 * 							- NULL if algorithm not supported
 * 
 * @ingroup hashers
 */
hasher_t *hasher_create(hash_algorithm_t hash_algorithm);

/**
 * @brief Conversion of ASN.1 OID to hash algorithm.
 * 
 * @param oid				ASN.1 OID
 * @return
 * 							- hash algorithm
 * 							- HASH_UNKNOWN if OID unsuported
 * 
 * @ingroup hashers
 */
hash_algorithm_t hasher_algorithm_from_oid(int oid);

/**
 * @brief Conversion of hash signature algorithm ASN.1 OID.
 * 
 * @param alg				hash algorithm
 * @return
 * 							- ASN.1 OID if known hash algorithm
 * 							- OID_UNKNOW
 * 
 * @ingroup hashers
 */
int hasher_signature_algorithm_to_oid(hash_algorithm_t alg);

#endif /* HASHER_H_ */
