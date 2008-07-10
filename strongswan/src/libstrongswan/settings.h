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
 * @defgroup settings settings
 * @{ @ingroup libstrongswan
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

typedef struct settings_t settings_t;

#include <library.h>

/**
 * Generic configuration options read from a config file.
 *
 * The sytax is quite simple:
 *
 * settings := (section|keyvalue)*
 * section  := name { settings }
 * keyvalue := key = value\n
 *
 * E.g.:
 * @code
   a = b
   section-one {
  		somevalue = asdf
  		subsection {
  			othervalue = xxx
  		}
  		yetanother = zz
  	}
  	section-two {
  	}
  	@endcode
 *
 * The values are accesses using the get() functions using dotted keys, e.g.
 *   section-one.subsection.othervalue
 */
struct settings_t {

	/**
	 * Get a settings value as a string.
	 *
	 * @param key		key including sections
	 * @param def		value returned if key not found
	 * @return			value pointing to internal string
	 */
	char* (*get_str)(settings_t *this, char *key, char *def);
	
	/**
	 * Get a boolean yes|no, true|false value.
	 *
	 * @param jey		key including sections
	 * @param def		default value returned if key not found
	 * @return			value of the key
	 */
	bool (*get_bool)(settings_t *this, char *key, bool def);
	
	/**
	 * Get an integer value.
	 *
	 * @param key		key including sections
	 * @param def		default value to return if key not found
	 * @return			value of the key
	 */
	int (*get_int)(settings_t *this, char *key, bool def);
	
	/**
     * Destroy a settings instance.
     */
    void (*destroy)(settings_t *this);
};

/**
 * Load setings from a file.
 */
settings_t *settings_create(char *file);

#endif /* SETTINGS_H_ @}*/
