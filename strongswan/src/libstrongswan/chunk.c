/*
 * Copyright (C) 2005-2006 Martin Willi
 * Copyright (C) 2005 Jan Hutter
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
 * $Id: chunk.c 3868 2008-04-24 13:26:22Z martin $
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "chunk.h"

#include <debug.h>
#include <printf_hook.h>

/**
 * Empty chunk.
 */
chunk_t chunk_empty = { NULL, 0 };

/**
 * Described in header.
 */
chunk_t chunk_create(u_char *ptr, size_t len)
{
	chunk_t chunk = {ptr, len};
	return chunk;
}

/**
 * Described in header.
 */
chunk_t chunk_create_clone(u_char *ptr, chunk_t chunk)
{
	chunk_t clone = chunk_empty;
	
	if (chunk.ptr && chunk.len > 0)
	{
		clone.ptr = ptr;
		clone.len = chunk.len;
		memcpy(clone.ptr, chunk.ptr, chunk.len);
	}
	
	return clone;
}

/**
 * Decribed in header.
 */
size_t chunk_length(const char* mode, ...)
{
	va_list chunks;
	size_t length = 0;
	
	va_start(chunks, mode);
	while (TRUE)
	{
		switch (*mode++)
		{
			case 'm':
			case 'c':
			{
				chunk_t ch = va_arg(chunks, chunk_t);
				length += ch.len;
				continue;
			}
			default:
				break;
		}
		break;
	}
	va_end(chunks);
	return length;
}

/**
 * Decribed in header.
 */
chunk_t chunk_create_cat(u_char *ptr, const char* mode, ...)
{
	va_list chunks;
	chunk_t construct = chunk_create(ptr, 0);
	
	va_start(chunks, mode);
	while (TRUE)
	{
		bool free_chunk = FALSE;
		switch (*mode++)
		{
			case 'm':
			{
				free_chunk = TRUE;
			}
			case 'c':
			{
				chunk_t ch = va_arg(chunks, chunk_t);
				memcpy(ptr, ch.ptr, ch.len); 
				ptr += ch.len;
				construct.len += ch.len;
				if (free_chunk)
				{
					free(ch.ptr);
				}
				continue;
			}
			default:
				break;
		}
		break;
	}
	va_end(chunks);
	
	return construct;
}

/**
 * Decribed in header.
 */
void chunk_split(chunk_t chunk, const char *mode, ...)
{
	va_list chunks;
	size_t len;
	chunk_t *ch;
	
	va_start(chunks, mode);
	while (TRUE)
	{
		if (*mode == '\0')
		{
			break;
		}
		len = va_arg(chunks, size_t);
		ch = va_arg(chunks, chunk_t*);
		/* a null chunk means skip len bytes */
		if (ch == NULL)
		{
			chunk = chunk_skip(chunk, len);
			continue;
		}
		switch (*mode++)
		{
			case 'm':
			{
				ch->len = min(chunk.len, len);
				if (ch->len)
				{
					ch->ptr = chunk.ptr;
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			case 'a':
			{
				ch->len = min(chunk.len, len);
				if (ch->len)
				{
					ch->ptr = malloc(ch->len);
					memcpy(ch->ptr, chunk.ptr, ch->len);
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			case 'c':
			{
				ch->len = min(ch->len, chunk.len);
				ch->len = min(ch->len, len);
				if (ch->len)
				{
					memcpy(ch->ptr, chunk.ptr, ch->len);
				}
				else
				{
					ch->ptr = NULL;
				}
				chunk = chunk_skip(chunk, ch->len);
				continue;
			}
			default:
				break;
		}
		break;
	}
	va_end(chunks);
}

/**
 * Described in header.
 */
bool chunk_write(chunk_t chunk, char *path, mode_t mask, bool force)
{
	mode_t oldmask;
	FILE *fd;
	bool good = FALSE;

	if (!force && access(path, F_OK) == 0)
	{
		DBG1("  file '%s' already exists", path);
		return FALSE;
	}
	oldmask = umask(mask);
	fd = fopen(path, "w");
	if (fd)
	{
		if (fwrite(chunk.ptr, sizeof(u_char), chunk.len, fd) == chunk.len)
		{
			good = TRUE;
		}
		else
		{
			DBG1("  writing to file '%s' failed: %s", path, strerror(errno));
		}
		fclose(fd);
		return TRUE;
	}
	else
	{
		DBG1("  could not open file '%s': %s", path, strerror(errno));
	}
	umask(oldmask);
	return good;
}


/** hex conversion digits */
static char hexdig_upper[] = "0123456789ABCDEF";
static char hexdig_lower[] = "0123456789abcdef";

/**
 * Described in header.
 */
chunk_t chunk_to_hex(chunk_t chunk, char *buf, bool uppercase)
{
	int i, len;;
	char *hexdig = hexdig_lower;
	
	if (uppercase)
	{
		hexdig = hexdig_upper;
	}
	
	len = chunk.len * 2;
	if (!buf)
	{
		buf = malloc(len + 1);
	}
	buf[len] = '\0';
	
	for (i = 0; i < chunk.len; i++)
	{
		buf[i*2]   = hexdig[(chunk.ptr[i] >> 4) & 0xF];
		buf[i*2+1] = hexdig[(chunk.ptr[i]     ) & 0xF];
	}
	return chunk_create(buf, len);
}

/**
 * convert a signle hex character to its binary value
 */
static char hex2bin(char hex)
{
	switch (hex)
	{
		case '0' ... '9':
			return hex - '0';
		case 'A' ... 'F':
			return hex - 'A' + 10;
		case 'a' ... 'f':
			return hex - 'a' + 10;
		default:
			return 0;
	}
}

/**
 * Described in header.
 */
chunk_t chunk_from_hex(chunk_t hex, char *buf)
{
	int i, len;
	
	len = hex.len / 2;
	if (!buf)
	{
		buf = malloc(len);
	}
	for (i = 0; i < len; i++)
	{
		buf[i] =  hex2bin(*hex.ptr++) << 4;
		buf[i] |= hex2bin(*hex.ptr++);
	}
	return chunk_create(buf, len);
}

/** base 64 conversion digits */
static char b64digits[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Described in header.
 */
chunk_t chunk_to_base64(chunk_t chunk, char *buf)
{
	int i, len;
	char *pos;
	
	len = chunk.len + ((3 - chunk.len % 3) % 3);
	if (!buf)
	{
		buf = malloc(len * 4 / 3 + 1);
	}
	pos = buf;
	for (i = 0; i < len; i+=3)
	{
		*pos++ = b64digits[chunk.ptr[i] >> 2];
		if (i+1 >= chunk.len)
		{
			*pos++ = b64digits[(chunk.ptr[i] & 0x03) << 4];
			*pos++ = '=';
			*pos++ = '=';
			break;
		}
		*pos++ = b64digits[((chunk.ptr[i] & 0x03) << 4) | (chunk.ptr[i+1] >> 4)];
		if (i+2 >= chunk.len)
		{
			*pos++ = b64digits[(chunk.ptr[i+1] & 0x0F) << 2];
			*pos++ = '=';
			break;
		}
		*pos++ = b64digits[((chunk.ptr[i+1] & 0x0F) << 2) | (chunk.ptr[i+2] >> 6)];
		*pos++ = b64digits[chunk.ptr[i+2] & 0x3F];
	}
	*pos = '\0';
	return chunk_create(buf, len * 4 / 3);
}

/**
 * convert a base 64 digit to its binary form (inversion of b64digits array)
 */
static int b642bin(char b64)
{
	switch (b64)
	{
		case 'A' ... 'Z':
			return b64 - 'A';
		case 'a' ... 'z':
			return ('Z' - 'A' + 1) + b64 - 'a';
		case '0' ... '9':
			return ('Z' - 'A' + 1) + ('z' - 'a' + 1) + b64 - '0';
		case '+':
		case '-':
			return 62;
		case '/':
		case '_':
			return 63;
		case '=':
			return 0;
		default:
			return -1;
	}
}

/**
 * Described in header.
 */
chunk_t chunk_from_base64(chunk_t base64, char *buf)
{
	u_char *pos, byte[4];
	int i, j, len, outlen;
	
	len = base64.len / 4 * 3;
	if (!buf)
	{
		buf = malloc(len);
	}
	pos = base64.ptr;
	outlen = 0;
	for (i = 0; i < len; i+=3)
	{
		outlen += 3;
		for (j = 0; j < 4; j++)
		{
			if (*pos == '=')
			{
				outlen--;
			}
			byte[j] = b642bin(*pos++);
		}
		buf[i] = (byte[0] << 2) | (byte[1] >> 4);
		buf[i+1] = (byte[1] << 4) | (byte[2] >> 2);
		buf[i+2] = (byte[2] << 6) | (byte[3]);
	}
	return chunk_create(buf, outlen);
}

/**
 * Described in header.
 */
void chunk_free(chunk_t *chunk)
{
	free(chunk->ptr);
	chunk->ptr = NULL;
	chunk->len = 0;
}

/**
 * Described in header.
 */
void chunk_clear(chunk_t *chunk)
{
	memset(chunk->ptr, 0, chunk->len);
	chunk_free(chunk);
}

/**
 * Described in header.
 */
chunk_t chunk_skip(chunk_t chunk, size_t bytes)
{
	if (chunk.len > bytes)
	{
		chunk.ptr += bytes;
		chunk.len -= bytes;
		return chunk;
	}
	return chunk_empty;
}

/**
 * Described in header.
 */
int chunk_compare(chunk_t a, chunk_t b)
{
	int compare_len = a.len - b.len;
	int len = (compare_len < 0)? a.len : b.len;

	if (compare_len != 0 || len == 0)
	{
		return compare_len;
	}
	return memcmp(a.ptr, b.ptr, len);
};

/**
 * Described in header.
 */
bool chunk_equals(chunk_t a, chunk_t b)
{
	return a.ptr != NULL  && b.ptr != NULL &&
			a.len == b.len && memeq(a.ptr, b.ptr, a.len);
}

/**
 * output handler in printf() for chunks
 */
static int chunk_print(FILE *stream, const struct printf_info *info,
					   const void *const *args)
{
	chunk_t *chunk = *((chunk_t**)(args[0]));
	bool first = TRUE;
	chunk_t copy = *chunk;
	int written = 0;
	printf_hook_functions_t mem = mem_get_printf_hooks();
	
	if (!info->alt)
	{
		const void *new_args[] = {&chunk->ptr, &chunk->len};
		return mem.print(stream, info, new_args);
	}
	
	while (copy.len > 0)
	{
		if (first)
		{
			first = FALSE;
		}
		else
		{
			written += fprintf(stream, ":");
		}
		written += fprintf(stream, "%02x", *copy.ptr++);
		copy.len--;
	}
	return written;
}

/**
 * arginfo handler for printf() mem ranges
 */
static int chunk_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
	if (n > 0)
	{
		argtypes[0] = PA_POINTER;
	}
	return 1;
}

/**
 * return printf hook functions for a chunk
 */
printf_hook_functions_t chunk_get_printf_hooks()
{
	printf_hook_functions_t hooks = {chunk_print, chunk_arginfo};
	
	return hooks;
}

