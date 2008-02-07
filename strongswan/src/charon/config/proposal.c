/**
 * @file proposal.c
 * 
 * @brief Implementation of proposal_t.
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

#include <string.h>

#include "proposal.h"

#include <daemon.h>
#include <utils/linked_list.h>
#include <utils/identification.h>
#include <utils/lexparser.h>
#include <crypto/prfs/prf.h>
#include <crypto/crypters/crypter.h>
#include <crypto/signers/signer.h>


ENUM(protocol_id_names, PROTO_NONE, PROTO_ESP,
	"PROTO_NONE",
	"IKE",
	"AH",
	"ESP",
);

ENUM_BEGIN(transform_type_names, UNDEFINED_TRANSFORM_TYPE, UNDEFINED_TRANSFORM_TYPE, 
	"UNDEFINED_TRANSFORM_TYPE");
ENUM_NEXT(transform_type_names, ENCRYPTION_ALGORITHM, EXTENDED_SEQUENCE_NUMBERS, UNDEFINED_TRANSFORM_TYPE,
	"ENCRYPTION_ALGORITHM",
	"PSEUDO_RANDOM_FUNCTION",
	"INTEGRITY_ALGORITHM",
	"DIFFIE_HELLMAN_GROUP",
	"EXTENDED_SEQUENCE_NUMBERS");
ENUM_END(transform_type_names, EXTENDED_SEQUENCE_NUMBERS);

ENUM(extended_sequence_numbers_names, NO_EXT_SEQ_NUMBERS, EXT_SEQ_NUMBERS,
	"NO_EXT_SEQ_NUMBERS",
	"EXT_SEQ_NUMBERS",
);

typedef struct private_proposal_t private_proposal_t;

/**
 * Private data of an proposal_t object
 */
struct private_proposal_t {

	/**
	 * Public part
	 */
	proposal_t public;
	
	/**
	 * protocol (ESP or AH)
	 */
	protocol_id_t protocol;
	
	/**
	 * priority ordered list of encryption algorithms
	 */
	linked_list_t *encryption_algos;
	
	/**
	 * priority ordered list of integrity algorithms
	 */
	linked_list_t *integrity_algos;
	
	/**
	 * priority ordered list of pseudo random functions
	 */
	linked_list_t *prf_algos;
	
	/**
	 * priority ordered list of dh groups
	 */
	linked_list_t *dh_groups;
	
	/**
	 * priority ordered list of extended sequence number flags
	 */
	linked_list_t *esns;
	
	/** 
	 * senders SPI
	 */
	u_int64_t spi;
};

/**
 * Add algorithm/keysize to a algorithm list
 */
static void add_algo(linked_list_t *list, u_int16_t algo, size_t key_size)
{
	algorithm_t *algo_key;
	
	algo_key = malloc_thing(algorithm_t);
	algo_key->algorithm = algo;
	algo_key->key_size = key_size;
	list->insert_last(list, (void*)algo_key);
}

/**
 * Implements proposal_t.add_algorithm
 */
static void add_algorithm(private_proposal_t *this, transform_type_t type, u_int16_t algo, size_t key_size)
{
	switch (type)
	{
		case ENCRYPTION_ALGORITHM:
			add_algo(this->encryption_algos, algo, key_size);
			break;
		case INTEGRITY_ALGORITHM:
			add_algo(this->integrity_algos, algo, key_size);
			break;
		case PSEUDO_RANDOM_FUNCTION:
			add_algo(this->prf_algos, algo, key_size);
			break;
		case DIFFIE_HELLMAN_GROUP:
			add_algo(this->dh_groups, algo, 0);
			break;
		case EXTENDED_SEQUENCE_NUMBERS:
			add_algo(this->esns, algo, 0);
			break;
		default:
			break;
	}
}

/**
 * Implements proposal_t.create_algorithm_iterator.
 */
static iterator_t *create_algorithm_iterator(private_proposal_t *this, transform_type_t type)
{
	switch (type)
	{
		case ENCRYPTION_ALGORITHM:
			return this->encryption_algos->create_iterator(this->encryption_algos, TRUE);
		case INTEGRITY_ALGORITHM:
			return this->integrity_algos->create_iterator(this->integrity_algos, TRUE);
		case PSEUDO_RANDOM_FUNCTION:
			return this->prf_algos->create_iterator(this->prf_algos, TRUE);
		case DIFFIE_HELLMAN_GROUP:
			return this->dh_groups->create_iterator(this->dh_groups, TRUE);
		case EXTENDED_SEQUENCE_NUMBERS:
			return this->esns->create_iterator(this->esns, TRUE);
		default:
			break;
	}
	return NULL;
}

/**
 * Implements proposal_t.get_algorithm.
 */
static bool get_algorithm(private_proposal_t *this, transform_type_t type, algorithm_t** algo)
{
	iterator_t *iterator = create_algorithm_iterator(this, type);
	if (iterator->iterate(iterator, (void**)algo))
	{
		iterator->destroy(iterator);
		return TRUE;
	}
	iterator->destroy(iterator);
	return FALSE;
}

/**
 * Implements proposal_t.has_dh_group
 */
static bool has_dh_group(private_proposal_t *this, diffie_hellman_group_t group)
{
	algorithm_t *current;
	iterator_t *iterator;
	bool result = FALSE;
	
	iterator = this->dh_groups->create_iterator(this->dh_groups, TRUE);
	if (iterator->get_count(iterator))
	{
		while (iterator->iterate(iterator, (void**)&current))
		{
			if (current->algorithm == group)
			{
				result = TRUE;
				break;
			}
		}
	}
	else if (group == MODP_NONE)
	{
		result = TRUE;
	}
	iterator->destroy(iterator);
	return result;
}

/**
 * Find a matching alg/keysize in two linked lists
 */
static bool select_algo(linked_list_t *first, linked_list_t *second, bool *add, u_int16_t *alg, size_t *key_size)
{
	iterator_t *first_iter, *second_iter;
	algorithm_t *first_alg, *second_alg;
	
	/* if in both are zero algorithms specified, we HAVE a match */
	if (first->get_count(first) == 0 && second->get_count(second) == 0)
	{
		*add = FALSE;
		return TRUE;
	}
	
	first_iter = first->create_iterator(first, TRUE);
	second_iter = second->create_iterator(second, TRUE);
	/* compare algs, order of algs in "first" is preferred */
	while (first_iter->iterate(first_iter, (void**)&first_alg))
	{
		second_iter->reset(second_iter);
		while (second_iter->iterate(second_iter, (void**)&second_alg))
		{
			if (first_alg->algorithm == second_alg->algorithm &&
				first_alg->key_size == second_alg->key_size)
			{
				/* ok, we have an algorithm */
				*alg = first_alg->algorithm;
				*key_size = first_alg->key_size;
				*add = TRUE;
				first_iter->destroy(first_iter);
				second_iter->destroy(second_iter);
				return TRUE;
			}
		}
	}
	/* no match in all comparisons */
	first_iter->destroy(first_iter);
	second_iter->destroy(second_iter);
	return FALSE;
}

/**
 * Implements proposal_t.select.
 */
static proposal_t *select_proposal(private_proposal_t *this, private_proposal_t *other)
{
	proposal_t *selected;
	u_int16_t algo;
	size_t key_size;
	bool add;
	
	DBG2(DBG_CFG, "selecting proposal:");
	
	/* check protocol */
	if (this->protocol != other->protocol)
	{
		DBG2(DBG_CFG, "  protocol mismatch, skipping");
		return NULL;
	}
	
	selected = proposal_create(this->protocol);
	
	/* select encryption algorithm */
	if (select_algo(this->encryption_algos, other->encryption_algos, &add, &algo, &key_size))
	{
		if (add)
		{
			selected->add_algorithm(selected, ENCRYPTION_ALGORITHM, algo, key_size);
		}
	}
	else
	{
		selected->destroy(selected);
		DBG2(DBG_CFG, "  no acceptable ENCRYPTION_ALGORITHM found, skipping");
		return NULL;
	}
	/* select integrity algorithm */
	if (select_algo(this->integrity_algos, other->integrity_algos, &add, &algo, &key_size))
	{
		if (add)
		{
			selected->add_algorithm(selected, INTEGRITY_ALGORITHM, algo, key_size);
		}
	}
	else
	{
		selected->destroy(selected);
		DBG2(DBG_CFG, "  no acceptable INTEGRITY_ALGORITHM found, skipping");
		return NULL;
	}
	/* select prf algorithm */
	if (select_algo(this->prf_algos, other->prf_algos, &add, &algo, &key_size))
	{
		if (add)
		{
			selected->add_algorithm(selected, PSEUDO_RANDOM_FUNCTION, algo, key_size);
		}
	}
	else
	{
		selected->destroy(selected);
		DBG2(DBG_CFG, "  no acceptable PSEUDO_RANDOM_FUNCTION found, skipping");
		return NULL;
	}
	/* select a DH-group */
	if (select_algo(this->dh_groups, other->dh_groups, &add, &algo, &key_size))
	{
		if (add)
		{
			selected->add_algorithm(selected, DIFFIE_HELLMAN_GROUP, algo, 0);
		}
	}
	else
	{
		selected->destroy(selected);
		DBG2(DBG_CFG, "  no acceptable DIFFIE_HELLMAN_GROUP found, skipping");
		return NULL;
	}
	/* select if we use ESNs */
	if (select_algo(this->esns, other->esns, &add, &algo, &key_size))
	{
		if (add)
		{
			selected->add_algorithm(selected, EXTENDED_SEQUENCE_NUMBERS, algo, 0);
		}
	}
	else
	{
		selected->destroy(selected);
		DBG2(DBG_CFG, "  no acceptable EXTENDED_SEQUENCE_NUMBERS found, skipping");
		return NULL;
	}
	DBG2(DBG_CFG, "  proposal matches");
	
	/* apply SPI from "other" */
	selected->set_spi(selected, other->spi);
	
	/* everything matched, return new proposal */
	return selected;
}

/**
 * Implements proposal_t.get_protocols.
 */
static protocol_id_t get_protocol(private_proposal_t *this)
{
	return this->protocol;
}

/**
 * Implements proposal_t.set_spi.
 */
static void set_spi(private_proposal_t *this, u_int64_t spi)
{
	this->spi = spi;
}

/**
 * Implements proposal_t.get_spi.
 */
static u_int64_t get_spi(private_proposal_t *this)
{
	return this->spi;
}

/**
 * Clone a algorithm list
 */
static void clone_algo_list(linked_list_t *list, linked_list_t *clone_list)
{
	algorithm_t *algo, *clone_algo;
	iterator_t *iterator = list->create_iterator(list, TRUE);
	while (iterator->iterate(iterator, (void**)&algo))
	{
		clone_algo = malloc_thing(algorithm_t);
		memcpy(clone_algo, algo, sizeof(algorithm_t));
		clone_list->insert_last(clone_list, (void*)clone_algo);
	}
	iterator->destroy(iterator);
}

/**
 * Implements proposal_t.clone
 */
static proposal_t *clone_(private_proposal_t *this)
{
	private_proposal_t *clone = (private_proposal_t*)proposal_create(this->protocol);
	
	clone_algo_list(this->encryption_algos, clone->encryption_algos);
	clone_algo_list(this->integrity_algos, clone->integrity_algos);
	clone_algo_list(this->prf_algos, clone->prf_algos);
	clone_algo_list(this->dh_groups, clone->dh_groups);
	clone_algo_list(this->esns, clone->esns);
	
	clone->spi = this->spi;
	
	return &clone->public;
}

/**
 * add a algorithm identified by a string to the proposal.
 * TODO: we could use gperf here.
 */
static status_t add_string_algo(private_proposal_t *this, chunk_t alg)
{
	if (strncmp(alg.ptr, "null", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_NULL, 0);
	}
	else if (strncmp(alg.ptr, "aes128", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_AES_CBC, 128);
	}
	else if (strncmp(alg.ptr, "aes192", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_AES_CBC, 192);
	}
	else if (strncmp(alg.ptr, "aes256", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_AES_CBC, 256);
	}
	else if (strncmp(alg.ptr, "3des", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_3DES, 0);
	}
	/* blowfish only uses some predefined key sizes yet */
	else if (strncmp(alg.ptr, "blowfish128", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_BLOWFISH, 128);
	}
	else if (strncmp(alg.ptr, "blowfish192", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_BLOWFISH, 192);
	}
	else if (strncmp(alg.ptr, "blowfish256", alg.len) == 0)
	{
		add_algorithm(this, ENCRYPTION_ALGORITHM, ENCR_BLOWFISH, 256);
	}
	else if (strncmp(alg.ptr, "sha", alg.len) == 0 ||
			 strncmp(alg.ptr, "sha1", alg.len) == 0)
	{
		/* sha means we use SHA for both, PRF and AUTH */
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_HMAC_SHA1_96, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA1, 0);
		}
	}  
	else if (strncmp(alg.ptr, "sha256", alg.len) == 0 ||
			 strncmp(alg.ptr, "sha2_256", alg.len) == 0)
	{
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_HMAC_SHA2_256_128, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_256, 0);
		}
	}
	else if (strncmp(alg.ptr, "sha384", alg.len) == 0 ||
			 strncmp(alg.ptr, "sha2_384", alg.len) == 0)
	{
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_HMAC_SHA2_384_192, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_384, 0);
		}
	}
	else if (strncmp(alg.ptr, "sha512", alg.len) == 0 ||
			 strncmp(alg.ptr, "sha2_512", alg.len) == 0)
	{
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_HMAC_SHA2_512_256, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_512, 0);
		}
	}
	else if (strncmp(alg.ptr, "md5", alg.len) == 0)
	{
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_HMAC_MD5_96, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_MD5, 0);
		}
	}
	else if (strncmp(alg.ptr, "aesxcbc", alg.len) == 0)
	{
		add_algorithm(this, INTEGRITY_ALGORITHM, AUTH_AES_XCBC_96, 0);
		if (this->protocol == PROTO_IKE)
		{
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, AUTH_AES_XCBC_96, 0);
		}
	}
	else if (strncmp(alg.ptr, "modp768", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_768_BIT, 0);
	}
	else if (strncmp(alg.ptr, "modp1024", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_1024_BIT, 0);
	}
	else if (strncmp(alg.ptr, "modp1536", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_1536_BIT, 0);
	}
	else if (strncmp(alg.ptr, "modp2048", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_2048_BIT, 0);
	}
	else if (strncmp(alg.ptr, "modp4096", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_4096_BIT, 0);
	}
	else if (strncmp(alg.ptr, "modp8192", alg.len) == 0)
	{
		add_algorithm(this, DIFFIE_HELLMAN_GROUP, MODP_8192_BIT, 0);
	}
	else
	{
		return FAILED;
	}
	return SUCCESS;
}

/**
 * Implements proposal_t.destroy.
 */
static void destroy(private_proposal_t *this)
{
	this->encryption_algos->destroy_function(this->encryption_algos, free);
	this->integrity_algos->destroy_function(this->integrity_algos, free);
	this->prf_algos->destroy_function(this->prf_algos, free);
	this->dh_groups->destroy_function(this->dh_groups, free);
	this->esns->destroy_function(this->esns, free);
	free(this);
}

/*
 * Describtion in header-file
 */
proposal_t *proposal_create(protocol_id_t protocol)
{
	private_proposal_t *this = malloc_thing(private_proposal_t);
	
	this->public.add_algorithm = (void (*)(proposal_t*,transform_type_t,u_int16_t,size_t))add_algorithm;
	this->public.create_algorithm_iterator = (iterator_t* (*)(proposal_t*,transform_type_t))create_algorithm_iterator;
	this->public.get_algorithm = (bool (*)(proposal_t*,transform_type_t,algorithm_t**))get_algorithm;
	this->public.has_dh_group = (bool (*)(proposal_t*,diffie_hellman_group_t))has_dh_group;
	this->public.select = (proposal_t* (*)(proposal_t*,proposal_t*))select_proposal;
	this->public.get_protocol = (protocol_id_t(*)(proposal_t*))get_protocol;
	this->public.set_spi = (void(*)(proposal_t*,u_int64_t))set_spi;
	this->public.get_spi = (u_int64_t(*)(proposal_t*))get_spi;
	this->public.clone = (proposal_t*(*)(proposal_t*))clone_;
	this->public.destroy = (void(*)(proposal_t*))destroy;
	
	this->spi = 0;
	this->protocol = protocol;
	
	this->encryption_algos = linked_list_create();
	this->integrity_algos = linked_list_create();
	this->prf_algos = linked_list_create();
	this->dh_groups = linked_list_create();
	this->esns = linked_list_create();
	
	return &this->public;
}

/*
 * Describtion in header-file
 */
proposal_t *proposal_create_default(protocol_id_t protocol)
{
	private_proposal_t *this = (private_proposal_t*)proposal_create(protocol);
	
	switch (protocol)
	{
		case PROTO_IKE:
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         128);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         192);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         256);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_3DES,              0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA2_256_128,	0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA1_96,      0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_MD5_96,       0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA2_384_192,	0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA2_512_256,	0);
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_256,      0);
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA1,          0);
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_MD5,           0);
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_384,      0);
			add_algorithm(this, PSEUDO_RANDOM_FUNCTION, PRF_HMAC_SHA2_512,      0);
			add_algorithm(this, DIFFIE_HELLMAN_GROUP,   MODP_2048_BIT, 		    0);
			add_algorithm(this, DIFFIE_HELLMAN_GROUP,   MODP_1536_BIT,          0);
			add_algorithm(this, DIFFIE_HELLMAN_GROUP,   MODP_1024_BIT,          0);
			add_algorithm(this, DIFFIE_HELLMAN_GROUP,   MODP_4096_BIT,          0);
			add_algorithm(this, DIFFIE_HELLMAN_GROUP,   MODP_8192_BIT,          0);
			break;
		case PROTO_ESP:
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         128);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         192);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_AES_CBC,         256);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_3DES,              0);
			add_algorithm(this, ENCRYPTION_ALGORITHM,   ENCR_BLOWFISH,        256);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA1_96,      0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_AES_XCBC_96,       0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_MD5_96,       0);
			add_algorithm(this, EXTENDED_SEQUENCE_NUMBERS, NO_EXT_SEQ_NUMBERS,  0);
			break;
		case PROTO_AH:
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_SHA1_96,      0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_AES_XCBC_96,       0);
			add_algorithm(this, INTEGRITY_ALGORITHM,    AUTH_HMAC_MD5_96,       0);
			add_algorithm(this, EXTENDED_SEQUENCE_NUMBERS, NO_EXT_SEQ_NUMBERS,  0);
			break;
		default:
			break;
	}
	
	return &this->public;
}

/*
 * Describtion in header-file
 */
proposal_t *proposal_create_from_string(protocol_id_t protocol, const char *algs)
{
	private_proposal_t *this = (private_proposal_t*)proposal_create(protocol);
	chunk_t string = {(void*)algs, strlen(algs)};
	chunk_t alg;
	status_t status = SUCCESS;
	
	eat_whitespace(&string);
	if (string.len < 1)
	{
		destroy(this);
		return NULL;
	}
	
	/* get all tokens, separated by '-' */
	while (extract_token(&alg, '-', &string))
	{
		status |= add_string_algo(this, alg);
	}
	if (string.len)
	{
		status |= add_string_algo(this, string);
	}
	if (status != SUCCESS)
	{
		destroy(this);
		return NULL;
	}
	
	if (protocol == PROTO_AH || protocol == PROTO_ESP)
	{
		add_algorithm(this, EXTENDED_SEQUENCE_NUMBERS, NO_EXT_SEQ_NUMBERS, 0);
	}
	return &this->public;
}
