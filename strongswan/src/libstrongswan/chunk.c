/**
 * @file chunk.c
 *
 * @brief Pointer/lenght abstraction and its functions.
 *
 */

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
 */

#include <stdio.h>
#include <sys/stat.h>

#include "chunk.h"

#include <debug.h>
#include <printf_hook.h>
#include <utils/randomizer.h>

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
bool chunk_write(chunk_t chunk, const char *path, const char *label, mode_t mask, bool force)
{
	mode_t oldmask;
	FILE *fd;

	if (!force)
	{
		fd = fopen(path, "r");
		if (fd)
		{
			fclose(fd);
			DBG1("  %s file '%s' already exists", label, path);
			return FALSE;
		}
	}

	/* set umask */
	oldmask = umask(mask);

	fd = fopen(path, "w");

	if (fd)
	{
		fwrite(chunk.ptr, sizeof(u_char), chunk.len, fd);
		fclose(fd);
		DBG1("  written %s file '%s' (%u bytes)", label, path, chunk.len);
		umask(oldmask);
		return TRUE;
	}
	else
	{
		DBG1("  could not open %s file '%s' for writing", label, path);
		umask(oldmask);
		return FALSE;
	}
}

/** hex conversion digits */
static char hexdig_upper[] = "0123456789ABCDEF";
static char hexdig_lower[] = "0123456789abcdef";

/**
 * Described in header.
 */
char *chunk_to_hex(chunk_t chunk, bool uppercase)
{
	int i;
	char *str;
	char *hexdig = hexdig_lower;
	
	if (uppercase)
	{
		hexdig = hexdig_upper;
	}
	
	str = malloc(chunk.len * 2 + 1);
	str[chunk.len * 2] = '\0';
	
	for (i = 0; i < chunk.len; i ++)
	{
		str[i*2]   = hexdig[(chunk.ptr[i] >> 4) & 0xF];
		str[i*2+1] = hexdig[(chunk.ptr[i]     ) & 0xF];
	}
	return str;
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
void chunk_free_randomized(chunk_t *chunk)
{
	if (chunk->ptr)
	{
		if (chunk->len > 0)
		{
			randomizer_t *randomizer = randomizer_create();

			randomizer->get_pseudo_random_bytes(randomizer,
												chunk->len, chunk->ptr);
			randomizer->destroy(randomizer);
		};
		free(chunk->ptr);
		chunk->ptr = NULL;
	}
	chunk->len = 0;
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
 * Described in header.
 */
bool chunk_equals_or_null(chunk_t a, chunk_t b)
{
	if (a.ptr == NULL || b.ptr == NULL)
		return TRUE;
	return a.len == b.len && memeq(a.ptr, b.ptr, a.len);
}

/**
 * Number of bytes per line to dump raw data
 */
#define BYTES_PER_LINE 16

/**
 * output handler in printf() for byte ranges
 */
static int print_bytes(FILE *stream, const struct printf_info *info,
					   const void *const *args)
{
	char *bytes = *((void**)(args[0]));
	int len = *((size_t*)(args[1]));
	
	char buffer[BYTES_PER_LINE * 3];
	char ascii_buffer[BYTES_PER_LINE + 1];
	char *buffer_pos = buffer;
	char *bytes_pos  = bytes;
	char *bytes_roof = bytes + len;
	int line_start = 0;
	int i = 0;
	int written = 0;
	
	written += fprintf(stream, "=> %d bytes @ %p", len, bytes);
	
	while (bytes_pos < bytes_roof)
	{
		*buffer_pos++ = hexdig_upper[(*bytes_pos >> 4) & 0xF];
		*buffer_pos++ = hexdig_upper[ *bytes_pos       & 0xF];

		ascii_buffer[i++] =
				(*bytes_pos > 31 && *bytes_pos < 127) ? *bytes_pos : '.';

		if (++bytes_pos == bytes_roof || i == BYTES_PER_LINE) 
		{
			int padding = 3 * (BYTES_PER_LINE - i);
			int written;
			
			while (padding--)
			{
				*buffer_pos++ = ' ';
			}
			*buffer_pos++ = '\0';
			ascii_buffer[i] = '\0';
			
			written += fprintf(stream, "\n%4d: %s  %s",
							   line_start, buffer, ascii_buffer);

			
			buffer_pos = buffer;
			line_start += BYTES_PER_LINE;
			i = 0;
		}
		else
		{
			*buffer_pos++ = ' ';
		}
	}
	return written;
}

/**
 * output handler in printf() for chunks
 */
static int print_chunk(FILE *stream, const struct printf_info *info,
					   const void *const *args)
{
	chunk_t *chunk = *((chunk_t**)(args[0]));
	bool first = TRUE;
	chunk_t copy = *chunk;
	int written = 0;
	
	if (!info->alt)
	{
		const void *new_args[] = {&chunk->ptr, &chunk->len};
		return print_bytes(stream, info, new_args);
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
 * register printf() handlers
 */
static void __attribute__ ((constructor))print_register()
{
	register_printf_function(PRINTF_CHUNK, print_chunk, arginfo_ptr);
	register_printf_function(PRINTF_BYTES, print_bytes, arginfo_ptr_int);
}
