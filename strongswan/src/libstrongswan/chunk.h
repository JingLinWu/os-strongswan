/**
 * @file chunk.h
 *
 * @brief Pointer/length abstraction and its functions.
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

#ifndef CHUNK_H_
#define CHUNK_H_

#include <string.h>
#include <stdarg.h>

#include <library.h>

typedef struct chunk_t chunk_t;

/**
 * General purpose pointer/length abstraction.
 */
struct chunk_t {
	/** Pointer to start of data */
	u_char *ptr;
	/** Length of data in bytes */
	size_t len;
};

/**
 * A { NULL, 0 }-chunk handy for initialization.
 */
extern chunk_t chunk_empty;

/**
 * Create a new chunk pointing to "ptr" with length "len"
 */
chunk_t chunk_create(u_char *ptr, size_t len);

/**
 * Create a clone of a chunk pointing to "ptr"
 */
chunk_t chunk_create_clone(u_char *ptr, chunk_t chunk);

/**
 * Calculate length of multiple chunks
 */
size_t chunk_length(const char *mode, ...);

/**
 * Concatenate chunks into a chunk pointing to "ptr",
 * "mode" is a string of "c" (copy) and "m" (move), which says
 * how to handle to chunks in "..."
 */
chunk_t chunk_create_cat(u_char *ptr, const char* mode, ...);

/**
 * Split up a chunk into parts, "mode" is a string of "a" (alloc),
 * "c" (copy) and "m" (move). Each letter say for the corresponding chunk if
 * it should get allocated on heap, copied into existing chunk, or the chunk
 * should point into "chunk". The length of each part is an argument before
 * each target chunk. E.g.:
 * chunk_split(chunk, "mcac", 3, &a, 7, &b, 5, &c, d.len, &d);
 */
void chunk_split(chunk_t chunk, const char *mode, ...);

/**
  * Write the binary contents of a chunk_t to a file
  */
bool chunk_write(chunk_t chunk, const char *path, const char *label, mode_t mask, bool force);

/**
 * convert a chunk to an allocated hex string 
 */
char *chunk_to_hex(chunk_t chunk, bool uppercase);

/**
 * Free contents of a chunk
 */
void chunk_free(chunk_t *chunk);

/**
 * Overwrite the contents of a chunk with pseudo-random bytes and free them
 */
void chunk_free_randomized(chunk_t *chunk);

/**
 * Initialize a chunk to point to buffer inspectable by sizeof()
 */
#define chunk_from_buf(str) { str, sizeof(str) }

/**
 * Initialize a chunk to point to a thing
 */
#define chunk_from_thing(thing) chunk_create((char*)&(thing), sizeof(thing))

/**
 * Allocate a chunk on the heap
 */
#define chunk_alloc(bytes) chunk_create(malloc(bytes), bytes)

/**
 * Allocate a chunk on the stack
 */
#define chunk_alloca(bytes) chunk_create(alloca(bytes), bytes)

/**
 * Clone a chunk on heap
 */
#define chunk_clone(chunk) chunk_create_clone(malloc(chunk.len), chunk)

/**
 * Clone a chunk on stack
 */
#define chunk_clonea(chunk) chunk_create_clone(alloca(chunk.len), chunk)

/**
 * Concatenate chunks into a chunk on heap
 */
#define chunk_cat(mode, ...) chunk_create_cat(malloc(chunk_length(mode, __VA_ARGS__)), mode, __VA_ARGS__)

/**
 * Concatenate chunks into a chunk on stack
 */
#define chunk_cata(mode, ...) chunk_create_cat(alloca(chunk_length(mode, __VA_ARGS__)), mode, __VA_ARGS__)

/**
 * Skip n bytes in chunk (forward pointer, shorten length)
 */
chunk_t chunk_skip(chunk_t chunk, size_t bytes);

/**
 *  Compare two chunks, returns zero if a equals b
 *  or negative/positive if a is small/greater than b
 */
int chunk_compare(chunk_t a, chunk_t b);

/**
 * Compare two chunks for equality,
 * NULL chunks are never equal.
 */
bool chunk_equals(chunk_t a, chunk_t b);

/**
 * Compare two chunks for equality,
 * NULL chunks are always equal.
 */
bool chunk_equals_or_null(chunk_t a, chunk_t b);

#endif /* CHUNK_H_ */
