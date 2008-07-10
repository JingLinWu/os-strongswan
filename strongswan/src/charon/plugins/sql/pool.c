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

#define _GNU_SOURCE
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include <debug.h>
#include <library.h>
#include <utils/host.h>

/**
 * global database handle
 */
database_t *db;

/**
 * --start/--end addresses of various subcommands
 */
host_t *start = NULL, *end = NULL;

/**
 * create a host from a blob
 */
static host_t *host_create_from_blob(chunk_t blob)
{
	return host_create_from_chunk(blob.len == 4 ? AF_INET : AF_INET6, blob, 0);
}

/**
 * calculate the size of a pool using start and end address chunk
 */
static u_int get_pool_size(chunk_t start, chunk_t end)
{
	u_int *start_ptr, *end_ptr;

	if (start.len < sizeof(u_int) || end.len < sizeof(u_int))
	{
		return 0;	
	}
	start_ptr = (u_int*)(start.ptr + start.len - sizeof(u_int));
	end_ptr = (u_int*)(end.ptr + end.len - sizeof(u_int));
	return ntohl(*end_ptr) -  ntohl(*start_ptr) + 1;
}

/**
 * print usage info
 */
static void usage(void)
{
	printf("\
Usage:\n\
  ipsec pool --status|--add|--del|--resize|--purge [options]\n\
  \n\
  ipsec pool --status\n\
    Show a list of installed pools with statistics.\n\
  \n\
  ipsec pool --add <name> --start <start> --end <end> [--timeout <timeout>]\n\
    Add a new pool to the database.\n\
      name:    Name of the pool, as used in ipsec.conf rightsourceip=%%name\n\
      start:   Start address of the pool\n\
      end:     End address of the pool\n\
      timeout: Lease time in hours, 0 for static leases\n\
  \n\
  ipsec pool --del <name>\n\
    Delete a pool from the database.\n\
      name:   Name of the pool to delete\n\
  \n\
  ipsec pool --resize <name> --end <end>\n\
    Grow or shrink an existing pool.\n\
      name:   Name of the pool to resize\n\
      end:    New end address for the pool\n\
  \n\
  ipsec pool --leases <name> [--filter <filter>] [--utc]\n\
    Show lease information using filters:\n\
      name:   Name of the pool to show leases from\n\
      filter: Filter string containing comma separated key=value filters,\n\
              e.g. id=alice@strongswan.org,addr=1.1.1.1\n\
                  pool:   name of the pool\n\
                  id:     assigned identity of the lease\n\
                  addr:   lease IP address\n\
                  tstamp: UNIX timestamp when lease was valid, as integer\n\
                  status: status of the lease: online|valid|expired\n\
      utc:    Show times in UTC instead of local time\n\
  \n\
  ipsec pool --purge <name>\n\
    Delete expired leases of a pool:\n\
      name:   Name of the pool to purge\n\
  \n");
	exit(0);
}

/**
 * ipsec pool --status - show pool overview
 */
static void status(void)
{
	enumerator_t *pool, *lease;
	bool found = FALSE;
	
	pool = db->query(db, "SELECT id, name, start, end, timeout FROM pools",
					 DB_INT, DB_TEXT, DB_BLOB, DB_BLOB, DB_UINT);
	if (pool)
	{
		char *name;
		chunk_t start_chunk, end_chunk;
		host_t *start, *end;
		u_int id, timeout, online = 0, used = 0, size = 0;
	
		while (pool->enumerate(pool, &id, &name,
							   &start_chunk, &end_chunk, &timeout))
		{
			if (!found)
			{
				printf("%8s %15s %15s %8s %6s %11s %11s\n",
					   "name", "start", "end", "timeout", "size", "online", "leases");
				found = TRUE;
			}
			
			start = host_create_from_blob(start_chunk);
			end = host_create_from_blob(end_chunk);
			size = get_pool_size(start_chunk, end_chunk);
			printf("%8s %15H %15H ", name, start, end);
			if (timeout)
			{
				printf("%7dh ", timeout/3600);
			}
			else
			{
				printf("%8s ", "static");
			}
			printf("%6d ", size);
			/* get number of online hosts */
			lease = db->query(db, "SELECT COUNT(*) FROM leases "
							  "WHERE pool = ? AND released IS NULL",
							  DB_UINT, id, DB_INT);
			if (lease)
			{
				lease->enumerate(lease, &online);
				lease->destroy(lease);
			}
			printf("%5d (%2d%%) ", online, online*100/size);
			/* get number of online or valid lieases */
			lease = db->query(db, "SELECT COUNT(*) FROM leases JOIN pools "
							  "ON leases.pool = pools.id "
							  "WHERE pools.id = ? "
							  "AND (released IS NULL OR released > ? - timeout) ",
							  DB_UINT, id, DB_UINT, time(NULL), DB_UINT);
			if (lease)
			{
				lease->enumerate(lease, &used);
				lease->destroy(lease);
			}
			printf("%5d (%2d%%) ", used, used*100/size);
			
			printf("\n");
			DESTROY_IF(start);
			DESTROY_IF(end);
		}
		pool->destroy(pool);
	}
	if (!found)
	{
		printf("no pools found.\n");
	}
	exit(0);
}

/**
 * ipsec pool --add - add a new pool
 */
static void add(char *name, host_t *start, host_t *end, int timeout)
{
	chunk_t start_addr, end_addr;
	
	start_addr = start->get_address(start);
	end_addr = end->get_address(end);

	if (start_addr.len != end_addr.len ||
		memcmp(start_addr.ptr, end_addr.ptr, start_addr.len) > 0)
	{
		fprintf(stderr, "invalid start/end pair specified.\n");
		exit(-1);
	}
	if (db->execute(db, NULL,
			"INSERT INTO pools (name, start, end, next, timeout) "
			"VALUES (?, ?, ?, ?, ?)",
			DB_TEXT, name, DB_BLOB, start_addr,
			DB_BLOB, end_addr, DB_BLOB, start_addr,
			DB_INT, timeout*3600) != 1)
	{
		fprintf(stderr, "creating pool failed.\n");
		exit(-1);
	}
	exit(0);
}

/**
 * ipsec pool --del - delete a pool
 */
static void del(char *name)
{
	enumerator_t *query;
	u_int id;
	bool found = FALSE;
	
	query = db->query(db, "SELECT id FROM pools WHERE name = ?",
					  DB_TEXT, name, DB_UINT);
	if (!query)
	{
		fprintf(stderr, "deleting pool failed.\n");
		exit(-1);
	}
	while (query->enumerate(query, &id))
	{
		found = TRUE;
		if (db->execute(db, NULL,
				"DELETE FROM pools WHERE id = ?", DB_UINT, id) != 1 ||
			db->execute(db, NULL,
				"DELETE FROM leases WHERE pool = ?", DB_UINT, id) < 0)
		{
			fprintf(stderr, "deleting pool failed.\n");
			query->destroy(query);
			exit(-1);
		}
	}
	query->destroy(query);
	if (!found)
	{
		fprintf(stderr, "pool '%s' not found.\n", name);
		exit(-1);
	}
	exit(0);
}

/**
 * ipsec pool --resize - resize a pool
 */
static void resize(char *name, host_t *end)
{
	enumerator_t *query;
	chunk_t next_addr, end_addr;
	
	end_addr = end->get_address(end);
	
	query = db->query(db, "SELECT next FROM pools WHERE name = ?",
					  DB_TEXT, name, DB_BLOB);
	if (!query || !query->enumerate(query, &next_addr))
	{
		DESTROY_IF(query);
		fprintf(stderr, "resizing pool failed.\n");
		exit(-1);
	}
	if (next_addr.len != end_addr.len ||
		memcmp(end_addr.ptr, next_addr.ptr, end_addr.len) < 0)
	{
		end = host_create_from_blob(next_addr);
		fprintf(stderr, "pool addresses up to %H in use, resizing failed.\n", end);
		end->destroy(end);
		query->destroy(query);
		exit(-1);
	}
	query->destroy(query);

	if (db->execute(db, NULL,
			"UPDATE pools SET end = ? WHERE name = ?",
			DB_BLOB, end_addr, DB_TEXT, name) <= 0)
	{
		fprintf(stderr, "pool '%s' not found.\n", name);
		exit(-1);
	}
	exit(0);
}

/**
 * create the lease query using the filter string
 */
static enumerator_t *create_lease_query(char *filter)
{
	enumerator_t *query;
	identification_t *id = NULL;
	host_t *addr = NULL;
	u_int tstamp = 0;
	bool online = FALSE, valid = FALSE, expired = FALSE;
	char *value, *pos, *pool = NULL;
	enum {
		FIL_POOL = 0,
		FIL_ID,
		FIL_ADDR,
		FIL_TSTAMP,
		FIL_STATE,
	};
	char *const token[] = {
		[FIL_POOL] = "pool",
		[FIL_ID] = "id",
		[FIL_ADDR] = "addr",
		[FIL_TSTAMP] = "tstamp",
		[FIL_STATE] = "status",
		NULL
	};
	
	/* if the filter string contains a distinguished name as a ID, we replace
	 * ", " by "/ " in order to not confuse the getsubopt parser */
	pos = filter;
	while ((pos = strchr(pos, ',')))
	{
		if (pos[1] == ' ')
		{
			pos[0] = '/';
		}
		pos++;
	}
	
	while (filter && *filter != '\0')
	{
		switch (getsubopt(&filter, token, &value))
		{
			case FIL_POOL:
				if (value)
				{
					pool = value;
				}
				break;
			case FIL_ID:
				if (value)
				{
					id = identification_create_from_string(value);
				}
				if (!id)
				{
					fprintf(stderr, "invalid 'id' in filter string.\n");
					exit(-1);
				}
				break;
			case FIL_ADDR:
				if (value)
				{
					addr = host_create_from_string(value, 0);
				}
				if (!addr)
				{
					fprintf(stderr, "invalid 'addr' in filter string.\n");
					exit(-1);
				}
				break;
			case FIL_TSTAMP:
				if (value)
				{
					tstamp = atoi(value);
				}
				if (tstamp == 0)
				{
					online = TRUE;
				}
				break;
			case FIL_STATE:
				if (value)
				{
					if (streq(value, "online"))
					{
						online = TRUE;
					}
					else if (streq(value, "valid"))
					{
						valid = TRUE;
					}
					else if (streq(value, "expired"))
					{
						expired = TRUE;
					}
					else
					{
						fprintf(stderr, "invalid 'state' in filter string.\n");
						exit(-1);
					}
				}
				break;
			default:
				fprintf(stderr, "invalid filter string.\n");
				exit(-1);
				break;
		}
	}
	query = db->query(db,
				"SELECT name, address, identities.type, "
				"identities.data, acquired, released, timeout "
				"FROM leases JOIN pools ON leases.pool = pools.id "
				"JOIN identities ON leases.identity = identities.id "
				"WHERE (? OR name = ?) "
				"AND (? OR (identities.type = ? AND identities.data = ?)) "
				"AND (? OR address = ?) "
				"AND (? OR (? >= acquired AND (? <= released OR released IS NULL))) "
				"AND (? OR released IS NULL) "
				"AND (? OR released > ? - timeout) "
				"AND (? OR released < ? - timeout)",
				DB_INT, pool == NULL, DB_TEXT, pool,
				DB_INT, id == NULL,
					DB_INT, id ? id->get_type(id) : 0,
					DB_BLOB, id ? id->get_encoding(id) : chunk_empty,
				DB_INT, addr == NULL,
					DB_BLOB, addr ? addr->get_address(addr) : chunk_empty,
				DB_INT, tstamp == 0, DB_UINT, tstamp, DB_UINT, tstamp,
				DB_INT, !online,
				DB_INT, !valid, DB_INT, time(NULL),
				DB_INT, !expired, DB_INT, time(NULL),
				DB_TEXT, DB_BLOB, DB_INT, DB_BLOB, DB_UINT, DB_UINT, DB_UINT);
	/* id and addr leak but we can't destroy them until query is destroyed. */
	return query;
}

/**
 * ipsec pool --leases - show lease information of a pool
 */
static void leases(char *filter, bool utc)
{
	enumerator_t *query;
	chunk_t address_chunk, identity_chunk;
	int identity_type;
	char *name;
	u_int acquired, released, timeout;
	host_t *address;
	identification_t *identity;
	bool found = FALSE;
	
	query = create_lease_query(filter);
	if (!query)
	{
		fprintf(stderr, "querying leases failed.\n");
		exit(-1);
	}
	while (query->enumerate(query, &name, &address_chunk, &identity_type,
							&identity_chunk, &acquired, &released, &timeout))
	{
		if (!found)
		{
			int len = utc ? 25 : 21;

			found = TRUE;
			printf("%-8s %-15s %-7s  %-*s %-*s %s\n",
				   "name", "address", "status", len, "start", len, "end", "identity");
		}
		address = host_create_from_blob(address_chunk);
		identity = identification_create_from_encoding(identity_type, identity_chunk);
		
		printf("%-8s %-15H ", name, address);
		if (released == 0)
		{
			printf("%-7s ", "online");
		}
		else if (timeout == 0)
		{
			printf("%-7s ", "static");
		}
		else if (released >= time(NULL) - timeout)
		{
			printf("%-7s ", "valid");
		}
		else
		{
			printf("%-7s ", "expired");
		}
		
		printf(" %#T  ", &acquired, utc);
		if (released)
		{
			printf("%#T  ", &released, utc);
		}
		else
		{
			printf("                      ");
			if (utc)
			{
				printf("    ");
			}
		}
		printf("%D\n", identity);
		DESTROY_IF(address);
		identity->destroy(identity);
	}
	query->destroy(query);
	if (!found)
	{
		fprintf(stderr, "no matching leases found.\n");
		exit(-1);
	}
	exit(0);
}

/**
 * ipsec pool --purge - delete expired leases
 */
static void purge(char *name)
{
	enumerator_t *query;
	u_int id, timeout, purged = 0;
	
	query = db->query(db, "SELECT id, timeout FROM pools WHERE name = ?",
					  DB_TEXT, name, DB_UINT, DB_UINT);
	if (!query)
	{
		fprintf(stderr, "purging pool failed.\n");
		exit(-1);
	}
	/* we have to keep one lease if we purge. It wouldn't be reallocateable
	 * as we move on the "next" address for speedy allocation */
	if (query->enumerate(query, &id, &timeout))
	{
		timeout = time(NULL) - timeout;
		purged = db->execute(db, NULL,
					"DELETE FROM leases WHERE pool = ? "
					"AND released IS NOT NULL AND released < ? AND id NOT IN ("
					" SELECT id FROM leases "
					" WHERE released IS NOT NULL and released < ? "
					" GROUP BY address)",
					DB_UINT, id, DB_UINT, timeout, DB_UINT, timeout);
	}
	query->destroy(query);
	fprintf(stderr, "purged %d leases in pool '%s'.\n", purged, name);
	exit(0);
}

/**
 * atexit handler to close db on shutdown
 */
static void cleanup(void)
{
	db->destroy(db);
	DESTROY_IF(start);
	DESTROY_IF(end);
}

/**
 * Logging hook for library logs, using stderr output
 */
static void dbg_stderr(int level, char *fmt, ...)
{
	va_list args;
	
	if (level <= 1)
	{
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
		va_end(args);
	}
}

int main(int argc, char *argv[])
{
	char *uri, *name = "", *filter = "";
	int timeout = 0;
	bool utc = FALSE;
	enum {
		OP_USAGE,
		OP_STATUS,
		OP_ADD,
		OP_DEL,
		OP_RESIZE,
		OP_LEASES,
		OP_PURGE,
	} operation = OP_USAGE;

	dbg = dbg_stderr;
	library_init(STRONGSWAN_CONF);
	atexit(library_deinit);
	lib->plugins->load(lib->plugins, IPSEC_PLUGINDIR,
		lib->settings->get_str(lib->settings, "pool.load", PLUGINS));
	
	uri = lib->settings->get_str(lib->settings, "charon.plugins.sql.database", NULL);
	if (!uri)
	{
		fprintf(stderr, "database URI charon.plugins.sql.database not set.\n");
		exit(-1);
	}
	db = lib->db->create(lib->db, uri);
	if (!db)
	{
		fprintf(stderr, "opening database failed.\n");
		exit(-1);
	}
	atexit(cleanup);
	
	while (TRUE)
	{
		int c;
		
		struct option long_opts[] = {
			{ "help", no_argument, NULL, 'h' },
		
			{ "utc", no_argument, NULL, 'u' },
			{ "status", no_argument, NULL, 'w' },
			{ "add", required_argument, NULL, 'a' },
			{ "del", required_argument, NULL, 'd' },
			{ "resize", required_argument, NULL, 'r' },
			{ "leases", no_argument, NULL, 'l' },
			{ "purge", required_argument, NULL, 'p' },
			
			{ "start", required_argument, NULL, 's' },
			{ "end", required_argument, NULL, 'e' },
			{ "timeout", required_argument, NULL, 't' },
			{ "filter", required_argument, NULL, 'f' },
			{ 0,0,0,0 }
		};
		
		c = getopt_long(argc, argv, "", long_opts, NULL);
		switch (c)
		{
			case EOF:
	    		break;
			case 'h':
				break;
			case 'w':
				operation = OP_STATUS;
				break;
			case 'u':
				utc = TRUE;
				continue;
			case 'a':
				operation = OP_ADD;
				name = optarg;
				continue;
			case 'd':
				operation = OP_DEL;
				name = optarg;
				continue;
			case 'r':
				operation = OP_RESIZE;
				name = optarg;
				continue;
			case 'l':
				operation = OP_LEASES;
				continue;
			case 'p':
				operation = OP_PURGE;
				name = optarg;
				continue;
			case 's':
				start = host_create_from_string(optarg, 0);
				if (start == NULL)
				{
					fprintf(stderr, "invalid start address: '%s'.\n", optarg);
					operation = OP_USAGE;
					break;
				}
				continue;
			case 'e':
				end = host_create_from_string(optarg, 0);
				if (end == NULL)
				{
					fprintf(stderr, "invalid end address: '%s'.\n", optarg);
					operation = OP_USAGE;
					break;
				}
				continue;
			case 't':
				timeout = atoi(optarg);
				if (timeout == 0 && strcmp(optarg, "0") != 0)
				{
					fprintf(stderr, "invalid timeout '%s'.\n", optarg);
					operation = OP_USAGE;
					break;
				}
				continue;
			case 'f':
				filter = optarg;
				continue;
			default:
				operation = OP_USAGE;
				break;
		}
		break;
	}
	
	switch (operation)
	{
		case OP_USAGE:
			usage();
			break;
		case OP_STATUS:
			status();
			break;
		case OP_ADD:
			if (start == NULL || end == NULL)
			{
				fprintf(stderr, "missing arguments.\n");
				usage();
			}
			add(name, start, end, timeout);
			break;
		case OP_DEL:
			del(name);
			break;
		case OP_RESIZE:
			if (end == NULL)
			{
				fprintf(stderr, "missing arguments.\n");
				usage();
			}
			resize(name, end);
			break;
		case OP_LEASES:
			leases(filter, utc);
			break;
		case OP_PURGE:
			purge(name);
			break;
	}
	exit(0);
}

