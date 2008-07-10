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
 * $Id: acquire_job.h 3589 2008-03-13 14:14:44Z martin $
 */

/**
 * @defgroup acquire_job acquire_job
 * @{ @ingroup jobs
 */

#ifndef ACQUIRE_JOB_H_
#define ACQUIRE_JOB_H_

typedef struct acquire_job_t acquire_job_t;

#include <library.h>
#include <processing/jobs/job.h>

/**
 * Class representing an ACQUIRE Job.
 * 
 * This job initiates a CHILD SA on kernel request.
 */
struct acquire_job_t {
	/**
	 * The job_t interface.
	 */
	job_t job_interface;
};

/**
 * Creates a job of type ACQUIRE.
 *
 * We use the reqid to find the routed CHILD_SA.
 *
 * @param reqid		reqid of the CHILD_SA to acquire
 * @return			acquire_job_t object
 */
acquire_job_t *acquire_job_create(u_int32_t reqid);

#endif /* REKEY_CHILD_SA_JOB_H_ @} */
