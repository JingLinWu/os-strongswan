/**
 * @file crl.h
 * 
 * @brief Interface of crl_t.
 * 
 */

/*
 * Copyright (C) 2006 Andreas Steffen
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
 * RCSID $Id: crl.h 3300 2007-10-12 21:53:18Z andreas $
 */

#ifndef CRL_H_
#define CRL_H_

typedef struct crl_t crl_t;

#include <library.h>
#include <crypto/rsa/rsa_public_key.h>
#include <crypto/certinfo.h>
#include <utils/identification.h>
#include <utils/iterator.h>

/**
 * @brief X.509 certificate revocation list
 * 
 * @b Constructors:
 *  - crl_create_from_chunk()
 *  - crl_create_from_file()
 * 
 * @ingroup transforms
 */
struct crl_t {

	/**
	 * @brief Get the crl's issuer ID.
	 * 
	 * The resulting ID is always a identification_t
	 * of type ID_DER_ASN1_DN.
	 * 
	 * @param this				calling object
	 * @return					issuers ID
	 */
	identification_t *(*get_issuer) (const crl_t *this);

	/**
	 * @brief Check if both crls have the same issuer.
	 * 
	 * @param this				calling object
	 * @param other				other crl
	 * @return					TRUE if the same issuer
	 */
	bool (*equals_issuer) (const crl_t *this, const crl_t *other);

	/**
	 * @brief Check if ia candidate cert is the issuer of the crl
	 * 
	 * @param this				calling object
	 * @param issuer			candidate issuer of the crl
	 * @return					TRUE if issuer
	 */
	bool (*is_issuer) (const crl_t *this, const x509_t *issuer);

	/**
	 * @brief Checks the validity interval of the crl
	 * 
	 * @param this			calling object
	 * @return				TRUE if the crl is valid
	 */
	bool (*is_valid) (const crl_t *this);
	
	/**
	 * @brief Checks if this crl is newer (thisUpdate) than the other crl
	 * 
	 * @param this			calling object
	 * @param other			other crl object
	 * @return				TRUE if this was issued more recently than other
	 */
	bool (*is_newer) (const crl_t *this, const crl_t *other);
	
	/**
	 * @brief Check if a crl is trustworthy.
	 * 
	 * @param this			calling object
	 * @param signer		signer's RSA public key
	 * @return				TRUE if crl is trustworthy
	 */
	bool (*verify) (const crl_t *this, const rsa_public_key_t *signer);

	/**
	 * @brief Get the certificate status
	 * 
	 * @param this			calling object
	 * @param certinfo		certinfo is updated
	 */
	void (*get_status) (const crl_t *this, certinfo_t *certinfo);
	
	/**
	 * @brief Log the info of this CRL to out.
	 *
	 * @param this			calling object
	 * @param out			stream to write to
	 * @param utc			TRUE for UTC, FALSE for local time
	 */
	void (*list)(crl_t *this, FILE* out, bool utc);

	/**
	 * @brief Write a der-encoded crl to a file
	 * 
	 * @param this			calling object
	 * @param path			path where the file is to be stored
	 * @param mask			file access control rights
	 * @param force			overwrite the file if it already exists
	 * @return				TRUE if successfully written
	 */
	bool (*write_to_file) (const crl_t *this, const char *path, mode_t mask, bool force);

	/**
	 * @brief Destroys the crl.
	 * 
	 * @param this			crl to destroy
	 */
	void (*destroy) (crl_t *this);
};

/**
 * @brief Read a x509 crl from a DER encoded blob.
 * 
 * @param chunk 	chunk containing DER encoded data
 * @return 			created crl_t, or NULL if invalid.
 * 
 * @ingroup transforms
 */
crl_t *crl_create_from_chunk(chunk_t chunk);

/**
 * @brief Read a x509 crl from a DER encoded file.
 * 
 * @param filename 	file containing DER encoded data
 * @return 			created crl_t, or NULL if invalid.
 * 
 * @ingroup transforms
 */
crl_t *crl_create_from_file(const char *filename);

#endif /* CRL_H_ */
