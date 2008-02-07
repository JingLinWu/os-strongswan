/**
 * @file printf_hook.h
 *
 * @brief Printf hook definitions and arginfo functions.
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

#ifndef PRINTF_HOOK_H_
#define PRINTF_HOOK_H_

#include <printf.h>

/**
 * Printf() hook characters.
 * We define all characters here to have them on a central place.
 */

/** 2 arguments: u_char *buffer, int size */
#define PRINTF_BYTES			'b'
/** 1 argument: chunk_t *chunk; use #-modifier to print inline */
#define PRINTF_CHUNK			'B'
/** 1 argument: identification_t *id */
#define PRINTF_IDENTIFICATION	'D'
/** 1 argumnet: host_t *host; use #-modifier to include port number */
#define PRINTF_HOST				'H'
/** 1 argument: ike_sa_t *ike_sa */
#define PRINTF_ENUM				'N'
/** 1 argument: child_sa_t *child_sa */
#define PRINTF_TRAFFIC_SELECTOR	'R'
/** 1 argument: time_t *time; with #-modifier 2 arguments: time_t *time, bool utc */
#define PRINTF_TIME				'T'
/** 1 argument: time_t *delta; with #-modifier 2 arguments: time_t *begin, time_t *end */
#define PRINTF_TIME_DELTA		'V'

/**
 * Generic arginfo handlers for printf() hooks
 */
int arginfo_ptr(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_ptr_ptr(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_ptr_int(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_int_int(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_ptr_alt_ptr_int(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_ptr_alt_ptr_ptr(const struct printf_info *info, size_t n, int *argtypes);
int arginfo_int_alt_int_int(const struct printf_info *info, size_t n, int *argtypes);

#endif /* PRINTF_HOOK_H_ */
