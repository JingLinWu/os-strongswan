/*
 * Copyright (C) 2008 Martin Willi
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
 * $Id$
 */

/**
 * @defgroup stroke_attribute stroke_attribute
 * @{ @ingroup stroke
 */

#ifndef STROKE_ATTRIBUTE_H_
#define STROKE_ATTRIBUTE_H_

#include <stroke_msg.h>
#include <config/attributes/attribute_provider.h>

typedef struct stroke_attribute_t stroke_attribute_t;

/**
 * Stroke IKEv2 cfg attribute provider
 */
struct stroke_attribute_t {

	/**
	 * Implements attribute provider interface
	 */
	attribute_provider_t provider;
	
	/**
	 * Add a virtual IP address.
	 *
	 * @param msg		stroke message
	 * @param end		end of stroke message that contains virtual IP.
	 */
	void (*add_pool)(stroke_attribute_t *this, stroke_msg_t *msg);
	
	/**
	 * Remove a virtual IP address.
	 *
	 * @param msg		stroke message
	 */
	void (*del_pool)(stroke_attribute_t *this, stroke_msg_t *msg);
	
	/**
     * Destroy a stroke_attribute instance.
     */
    void (*destroy)(stroke_attribute_t *this);
};

/**
 * Create a stroke_attribute instance.
 */
stroke_attribute_t *stroke_attribute_create();

#endif /* STROKE_ATTRIBUTE_H_ @}*/
