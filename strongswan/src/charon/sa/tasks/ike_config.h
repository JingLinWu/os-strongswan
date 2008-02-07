/**
 * @file ike_config.h
 * 
 * @brief Interface ike_config_t.
 * 
 */

/*
 * Copyright (C) 2007 Martin Willi
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

#ifndef IKE_CONFIG_H_
#define IKE_CONFIG_H_

typedef struct ike_config_t ike_config_t;

#include <library.h>
#include <sa/ike_sa.h>
#include <sa/tasks/task.h>

/**
 * @brief Task of type IKE_CONFIG, sets up a virtual IP and other
 * configurations for an IKE_SA.
 *
 * @b Constructors:
 *  - ike_config_create()
 * 
 * @ingroup tasks
 */
struct ike_config_t {

	/**
	 * Implements the task_t interface
	 */
	task_t task;
};

/**
 * @brief Create a new ike_config task.
 *
 * @param ike_sa		IKE_SA this task works for
 * @param initiator		TRUE for initiator
 * @return			  	ike_config task to handle by the task_manager
 */
ike_config_t *ike_config_create(ike_sa_t *ike_sa, bool initiator);

#endif /* IKE_CONFIG_H_ */
