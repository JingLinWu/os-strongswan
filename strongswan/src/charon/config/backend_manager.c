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
 *
 * $Id: backend_manager.c 4044 2008-06-06 15:05:54Z martin $
 */

#include "backend_manager.h"

#include <sys/types.h>
#include <pthread.h>

#include <daemon.h>
#include <utils/linked_list.h>
#include <utils/mutex.h>


typedef struct private_backend_manager_t private_backend_manager_t;

/**
 * Private data of an backend_manager_t object.
 */
struct private_backend_manager_t {

	/**
	 * Public part of backend_manager_t object.
	 */
	backend_manager_t public;
	
	/**
	 * list of registered backends
	 */
	linked_list_t *backends;
	
	/**
	 * locking mutex
	 */
	mutex_t *mutex;
};

/**
 * data to pass nested IKE enumerator
 */
typedef struct {
	private_backend_manager_t *this;
	host_t *me;
	host_t *other;
} ike_data_t;

/**
 * data to pass nested peer enumerator
 */
typedef struct {
	private_backend_manager_t *this;
	identification_t *me;
	identification_t *other;
} peer_data_t;

/**
 * destroy IKE enumerator data and unlock list
 */
static void ike_enum_destroy(ike_data_t *data)
{
	data->this->mutex->unlock(data->this->mutex);
	free(data);
}

/**
 * destroy PEER enumerator data and unlock list
 */
static void peer_enum_destroy(peer_data_t *data)
{
	data->this->mutex->unlock(data->this->mutex);
	free(data);
}

/**
 * inner enumerator constructor for IKE cfgs
 */
static enumerator_t *ike_enum_create(backend_t *backend, ike_data_t *data)
{
	return backend->create_ike_cfg_enumerator(backend, data->me, data->other);
}

/**
 * inner enumerator constructor for Peer cfgs
 */
static enumerator_t *peer_enum_create(backend_t *backend, peer_data_t *data)
{
	return backend->create_peer_cfg_enumerator(backend, data->me, data->other);
}
/**
 * inner enumerator constructor for all Peer cfgs
 */
static enumerator_t *peer_enum_create_all(backend_t *backend)
{
	return backend->create_peer_cfg_enumerator(backend, NULL, NULL);
}

/**
 * implements backend_manager_t.get_ike_cfg.
 */
static ike_cfg_t *get_ike_cfg(private_backend_manager_t *this, 
							  host_t *me, host_t *other)
{
	ike_cfg_t *current, *found = NULL;
	enumerator_t *enumerator;
	host_t *my_candidate, *other_candidate;
	ike_data_t *data;
	enum {
		MATCH_NONE  = 0x00,
		MATCH_ANY   = 0x01,
		MATCH_ME    = 0x04,
		MATCH_OTHER = 0x08,
	} prio, best = MATCH_ANY;
	
	data = malloc_thing(ike_data_t);
	data->this = this;
	data->me = me;
	data->other = other;
	
	DBG2(DBG_CFG, "looking for a config for %H...%H", me, other);
	
	this->mutex->lock(this->mutex);
	enumerator = enumerator_create_nested(
						this->backends->create_enumerator(this->backends),
						(void*)ike_enum_create, data, (void*)ike_enum_destroy);
	while (enumerator->enumerate(enumerator, (void**)&current))
	{
		prio = MATCH_NONE;
		
		my_candidate = host_create_from_dns(current->get_my_addr(current),
											me->get_family(me), 0);
		if (!my_candidate)
		{
			continue;
		}
		if (my_candidate->ip_equals(my_candidate, me))
		{
			prio += MATCH_ME;
		}
		else if (my_candidate->is_anyaddr(my_candidate))
		{
			prio += MATCH_ANY;
		}
		my_candidate->destroy(my_candidate);
		
		other_candidate = host_create_from_dns(current->get_other_addr(current),
											   other->get_family(other), 0);
		if (!other_candidate)
		{
			continue;
		}
		if (other_candidate->ip_equals(other_candidate, other))
		{
			prio += MATCH_OTHER;
		}
		else if (other_candidate->is_anyaddr(other_candidate))
		{
			prio += MATCH_ANY;
		}
		other_candidate->destroy(other_candidate);
		
		DBG2(DBG_CFG, "  candidate: %s...%s, prio %d", 
			 current->get_my_addr(current), current->get_other_addr(current),
			 prio);
		
		/* we require at least two MATCH_ANY */
		if (prio > best)
		{
			best = prio;
			DESTROY_IF(found);
			found = current;
			found->get_ref(found);
		}
	}
	enumerator->destroy(enumerator);
	this->mutex->unlock(this->mutex);
	return found;
}


static enumerator_t *create_peer_cfg_enumerator(private_backend_manager_t *this)
{
	this->mutex->lock(this->mutex);
	return enumerator_create_nested(
							this->backends->create_enumerator(this->backends),
							(void*)peer_enum_create_all, this->mutex,
							(void*)this->mutex->unlock);
}

/**
 * implements backend_manager_t.get_peer_cfg.
 */			
static peer_cfg_t *get_peer_cfg(private_backend_manager_t *this,
								identification_t *me, identification_t *other,
								auth_info_t *auth)
{
	peer_cfg_t *current, *found = NULL;
	enumerator_t *enumerator;
	identification_t *my_candidate, *other_candidate;
	id_match_t best = ID_MATCH_NONE;
	peer_data_t *data;
	
	DBG2(DBG_CFG, "looking for a config for %D...%D", me, other);
	
	data = malloc_thing(peer_data_t);
	data->this = this;
	data->me = me;
	data->other = other;
	
	this->mutex->lock(this->mutex);
	enumerator = enumerator_create_nested(
						this->backends->create_enumerator(this->backends),
						(void*)peer_enum_create, data, (void*)peer_enum_destroy);
	while (enumerator->enumerate(enumerator, &current))
	{
		id_match_t m1, m2, sum;

		my_candidate = current->get_my_id(current);
		other_candidate = current->get_other_id(current);
		
		/* own ID may have wildcards in both, config and request (missing IDr) */
		m1 = my_candidate->matches(my_candidate, me);
		if (!m1)
		{
			m1 = me->matches(me, my_candidate);
		}
		m2 = other->matches(other, other_candidate);
		sum = m1 + m2;
		
		if (m1 && m2)
		{
			if (auth->complies(auth, current->get_auth(current)))
			{
				DBG2(DBG_CFG, "  candidate '%s': %D...%D, prio %d",
				 	 current->get_name(current), my_candidate,
				 	 other_candidate, sum);
				if (sum > best)
				{
					DESTROY_IF(found);
					found = current;
					found->get_ref(found);
					best = sum;
				}
			}
		}
	}
	if (found)
	{
		DBG1(DBG_CFG, "found matching config \"%s\": %D...%D, prio %d",
			 found->get_name(found), found->get_my_id(found),
			 found->get_other_id(found), best);
	}
	enumerator->destroy(enumerator);
	this->mutex->unlock(this->mutex);
	return found;
}

/**
 * implements backend_manager_t.get_peer_cfg_by_name.
 */			
static peer_cfg_t *get_peer_cfg_by_name(private_backend_manager_t *this, char *name)
{
	backend_t *backend;
	peer_cfg_t *config = NULL;
	enumerator_t *enumerator;
	
	this->mutex->lock(this->mutex);
	enumerator = this->backends->create_enumerator(this->backends);
	while (config == NULL && enumerator->enumerate(enumerator, (void**)&backend))
	{
		config = backend->get_peer_cfg_by_name(backend, name);
	}
	enumerator->destroy(enumerator);
	this->mutex->unlock(this->mutex);
	return config;
}

/**
 * Implementation of backend_manager_t.remove_backend.
 */
static void remove_backend(private_backend_manager_t *this, backend_t *backend)
{
	this->mutex->lock(this->mutex);
	this->backends->remove(this->backends, backend, NULL);
	this->mutex->unlock(this->mutex);
}

/**
 * Implementation of backend_manager_t.add_backend.
 */
static void add_backend(private_backend_manager_t *this, backend_t *backend)
{
	this->mutex->lock(this->mutex);
	this->backends->insert_last(this->backends, backend);
	this->mutex->unlock(this->mutex);
}

/**
 * Implementation of backend_manager_t.destroy.
 */
static void destroy(private_backend_manager_t *this)
{
	this->backends->destroy(this->backends);
	this->mutex->destroy(this->mutex);
	free(this);
}

/*
 * Described in header-file
 */
backend_manager_t *backend_manager_create()
{
	private_backend_manager_t *this = malloc_thing(private_backend_manager_t);
	
	this->public.get_ike_cfg = (ike_cfg_t* (*)(backend_manager_t*, host_t*, host_t*))get_ike_cfg;
	this->public.get_peer_cfg = (peer_cfg_t* (*)(backend_manager_t*,identification_t*,identification_t*,auth_info_t*))get_peer_cfg;
	this->public.get_peer_cfg_by_name = (peer_cfg_t* (*)(backend_manager_t*,char*))get_peer_cfg_by_name;
	this->public.create_peer_cfg_enumerator = (enumerator_t* (*)(backend_manager_t*))create_peer_cfg_enumerator;
	this->public.add_backend = (void(*)(backend_manager_t*, backend_t *backend))add_backend;
	this->public.remove_backend = (void(*)(backend_manager_t*, backend_t *backend))remove_backend;
	this->public.destroy = (void (*)(backend_manager_t*))destroy;
	
	this->backends = linked_list_create();
	this->mutex = mutex_create(MUTEX_RECURSIVE);
	
	return &this->public;
}

