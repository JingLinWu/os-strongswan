/**
 * @file certinfo.c
 * 
 * @brief Implementation of certinfo_t.
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
 */

#include <time.h>
#include <stdio.h>

#include <library.h>

#include "certinfo.h"

typedef struct private_certinfo_t private_certinfo_t;

/**
 * Private data of a certinfo_t object.
 */
struct private_certinfo_t {
	/**
	 * Public interface for this certificate status information object.
	 */
	certinfo_t public;
	
	/**
	 * Serial number of the certificate
	 */
	chunk_t serialNumber;

	/**
	 * Certificate status
	 */
	cert_status_t status;

	/**
	 * Certificate status is for one-time use only
	 */
	bool once;

	/**
	 * Time when the certificate status info was generated
	 */
	time_t thisUpdate;

	/**
	 * Time when an updated certifcate status info will be available
	 */
	time_t nextUpdate;

	/**
	 * Time of certificate revocation
	 */
    time_t revocationTime;

	/**
	 * Reason of certificate revocation
	 */
    crl_reason_t revocationReason;
};

ENUM(cert_status_names, CERT_GOOD, CERT_UNTRUSTED,
	"good",
	"revoked",
	"unknown",
	"unknown",
	"untrusted",
);

ENUM(crl_reason_names, REASON_UNSPECIFIED, REASON_REMOVE_FROM_CRL,
	"unspecified",
	"key compromise",
	"ca compromise",
	"affiliation changed",
	"superseded",
	"cessation of operation",
	"certificate hold",
	"reason #7",
	"remove from crl",
);

/**
 * Implements certinfo_t.compare_serialNumber
 */
static int compare_serialNumber(const private_certinfo_t *this, const private_certinfo_t *that)
{
	return chunk_compare(this->serialNumber, that->serialNumber);
}

/**
 * Implements certinfo_t.equals_serialNumber
 */
static bool equals_serialNumber(const private_certinfo_t *this, const private_certinfo_t *that)
{
	return chunk_equals(this->serialNumber, that->serialNumber);
}

/**
 * Implements certinfo_t.get_serialNumber
 */
static chunk_t get_serialNumber(const private_certinfo_t *this)
{
	return this->serialNumber;
}

/**
 * Implements certinfo_t.set_status
 */
static void set_status(private_certinfo_t *this, cert_status_t status)
{
	this->status = status;
}

/**
 * Implements certinfo_t.get_status
 */
static cert_status_t get_status(const private_certinfo_t *this)
{
	return this->status;
}

/**
 * Implements certinfo_t.set_thisUpdate
 */
static void set_thisUpdate(private_certinfo_t *this, time_t thisUpdate)
{
	this->thisUpdate = thisUpdate;
}

/**
 * Implements certinfo_t.get_thisUpdate
 */
static time_t get_thisUpdate(const private_certinfo_t *this)
{
	return this->thisUpdate;
}

/**
 * Implements certinfo_t.set_nextUpdate
 */
static void set_nextUpdate(private_certinfo_t *this, time_t nextUpdate)
{
	this->nextUpdate = nextUpdate;
}

/**
 * Implements certinfo_t.get_nextUpdate
 */
static time_t get_nextUpdate(const private_certinfo_t *this)
{
	return this->nextUpdate;
}

/**
 * Implements certinfo_t.set_revocationTime
 */
static void set_revocationTime(private_certinfo_t *this, time_t revocationTime)
{
	this->revocationTime = revocationTime;
}

/**
 * Implements certinfo_t.get_revocationTime
 */
static time_t get_revocationTime(const private_certinfo_t *this)
{
	return this->revocationTime;
}

/**
 * Implements certinfo_t.set_revocationReason
 */
static void set_revocationReason(private_certinfo_t *this, crl_reason_t reason)
{
	this->revocationReason = reason;
}

/**
 * Implements certinfo_t.get_revocationReason
 */
static crl_reason_t get_revocationReason(const private_certinfo_t *this)
{
	return this->revocationReason;
}

/**
 * Implements certinfo_t.update
 */
static void update(private_certinfo_t *this, const private_certinfo_t *that)
{
	if (equals_serialNumber(this, that))
	{
		chunk_t this_serialNumber = this->serialNumber;

		*this = *that;
		this->serialNumber = this_serialNumber;
	}
}

/**
 * Implements certinfo_t.destroy
 */
static void destroy(private_certinfo_t *this)
{
	free(this->serialNumber.ptr);
	free(this);
}

/*
 * Described in header.
 */
certinfo_t *certinfo_create(chunk_t serial)
{
	private_certinfo_t *this = malloc_thing(private_certinfo_t);
	
	/* initialize */
	this->serialNumber = chunk_clone(serial);
	this->status = CERT_UNDEFINED;
	this->thisUpdate = UNDEFINED_TIME;
	this->nextUpdate = UNDEFINED_TIME;
	this->revocationTime = UNDEFINED_TIME;
	this->revocationReason = REASON_UNSPECIFIED;

	/* public functions */
	this->public.compare_serialNumber = (int (*) (const certinfo_t*,const certinfo_t*))compare_serialNumber;
	this->public.equals_serialNumber = (bool (*) (const certinfo_t*,const certinfo_t*))equals_serialNumber;
	this->public.get_serialNumber = (chunk_t (*) (const certinfo_t*))get_serialNumber;
	this->public.set_status = (void (*) (certinfo_t*,cert_status_t))set_status;
	this->public.get_status = (cert_status_t (*) (const certinfo_t*))get_status;
	this->public.set_thisUpdate = (void (*) (certinfo_t*,time_t))set_thisUpdate;
	this->public.get_thisUpdate = (time_t (*) (const certinfo_t*))get_thisUpdate;
	this->public.set_nextUpdate = (void (*) (certinfo_t*,time_t))set_nextUpdate;
	this->public.get_nextUpdate = (time_t (*) (const certinfo_t*))get_nextUpdate;
	this->public.set_revocationTime = (void (*) (certinfo_t*,time_t))set_revocationTime;
	this->public.get_revocationTime = (time_t (*) (const certinfo_t*))get_revocationTime;
	this->public.set_revocationReason = (void (*) (certinfo_t*, crl_reason_t))set_revocationReason;
	this->public.get_revocationReason = (crl_reason_t(*) (const certinfo_t*))get_revocationReason;
	this->public.update = (void (*) (certinfo_t*, const certinfo_t*))update;
	this->public.destroy = (void (*) (certinfo_t*))destroy;

	return &this->public;
}
