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
 *
 * $Id: sys_logger.h 3589 2008-03-13 14:14:44Z martin $
 */

/**
 * @defgroup sys_logger sys_logger
 * @{ @ingroup listeners
 */

#ifndef SYS_LOGGER_H_
#define SYS_LOGGER_H_

typedef struct sys_logger_t sys_logger_t;

#include <syslog.h>

#include <bus/bus.h>

/**
 * Logger for syslog which implements bus_listener_t.
 */
struct sys_logger_t {
	
	/**
	 * Implements the bus_listener_t interface.
	 */
	bus_listener_t listener;
	
	/**
	 * Set the loglevel for a signal type.
	 *
	 * @param singal	type of signal
	 * @param level		max level to log
	 */
	void (*set_level) (sys_logger_t *this, signal_t signal, level_t level);
	
	/**
	 * Destroys a sys_logger_t object.
	 */
	void (*destroy) (sys_logger_t *this);
};

/**
 * Constructor to create a sys_logger_t object.
 *
 * @param facility	syslog facility to use
 * @return			sys_logger_t object
 */
sys_logger_t *sys_logger_create(int facility);

#endif /* SYS_LOGGER_H_ @} */
