/**
 * @file psk_authenticator.h
 *
 * @brief Interface of psk_authenticator_t.
 *
 */

/*
 * Copyright (C) 2006 Martin Willi
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

#ifndef PSK_AUTHENTICATOR_H_
#define PSK_AUTHENTICATOR_H_

typedef struct psk_authenticator_t psk_authenticator_t;

#include <sa/authenticators/authenticator.h>

/**
 * @brief Implementation of the authenticator_t interface using AUTH_PSK.
 *
 * @b Constructors:
 *  - psk_authenticator_create()
 *  - authenticator_create() using auth_method AUTH_PSK
 *
 * @ingroup authenticators
 */
struct psk_authenticator_t {

	/**
	 * Implemented authenticator_t interface.
	 */
	authenticator_t authenticator_interface;
};

/**
 * @brief Creates an authenticator for AUTH_PSK.
 *
 * @param ike_sa		associated ike_sa
 * @return				psk_authenticator_t object
 *
 * @ingroup authenticators
 */
psk_authenticator_t *psk_authenticator_create(ike_sa_t *ike_sa);

#endif /* PSK_AUTHENTICATOR_H_ */
