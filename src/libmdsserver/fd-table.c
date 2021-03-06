/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "fd-table.h"

#include "macros.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>


/**
 * Create a fd table
 * 
 * @param   this              Memory slot in which to store the new fd table
 * @param   initial_capacity  The initial capacity of the table
 * @return                    Non-zero on error, `errno` will have been set accordingly
 */
int
fd_table_create_tuned(fd_table_t *restrict this, size_t initial_capacity)
{
	size_t bitcap;

	this->capacity = initial_capacity ? initial_capacity : 1;
	this->size = 0;

	this->values = NULL;
	this->used = NULL;
	this->value_comparator = NULL;

	/* It is important that both allocations are done with calloc:
	   `this->used` must set all keys as unused at the initial state,
	   `this->values` must be initialised for marshaling and it helps
	   the time overhead of `fd_table_contains_value`. */

	bitcap = (this->capacity + 63) / 64;
	fail_if (xcalloc(this->used,   bitcap,         size_t));
	fail_if (xcalloc(this->values, this->capacity, size_t));

	return 0;
fail:
	return -1;
}


/**
 * Release all resources in a fd table, should
 * be done even if construction fails
 * 
 * @param  this          The fd table
 * @param  keys_freer    Function that frees a key, `NULL` if keys should not be freed
 * @param  values_freer  Function that frees a value, `NULL` if value should not be freed
 */
void
fd_table_destroy(fd_table_t *restrict this, free_func *key_freer, free_func *value_freer)
{
	size_t i;
	if ((key_freer || value_freer) && this->used && this->values) {
		for (i = 0; i < this->capacity; i++) {
			if (this->used[i / 64] & ((uint64_t)1 << (i % 64))) {
				if (key_freer)   key_freer(i);
				if (value_freer) value_freer(this->values[i]);
			}
		}
	}
	free(this->values);
	free(this->used);
}


/**
 * Check whether a value is stored in the table
 * 
 * @param   this   The fd table
 * @param   value  The value
 * @return         Whether the value is stored in the table
 */
int
fd_table_contains_value(const fd_table_t *restrict this, size_t value)
{
	size_t i;
	if (!this->value_comparator) {
		for (i = 0; i < this->capacity; i++)
			if (this->values[i] == value)
				if (this->used[i / 64] & ((uint64_t)1 << (i % 64)))
					return 1;
	} else {
		for (i = 0; i < this->capacity; i++)
			if (this->used[i / 64] & ((uint64_t)1 << (i % 64)))
				if (this->value_comparator(this->values[i], value))
					return 1;
	}
	return 0;
}


/**
 * Check whether a key is used in the table
 * 
 * @param   this  The fd table
 * @param   key   The key
 * @return        Whether the key is used
 */
int
fd_table_contains_key(const fd_table_t *restrict this, int key)
{
	return (size_t)key < this->capacity && (this->used[key / 64] & ((uint64_t)1 << (key % 64)));
}


/**
 * Look up a value in the table
 * 
 * @param   this  The fd table
 * @param   key   The key associated with the value
 * @return        The value associated with the key, 0 if the key was not used
 */
size_t
fd_table_get(const fd_table_t *restrict this, int key)
{
	return fd_table_contains_key(this, key) ? this->values[key] : 0;
}


/**
 * Add an entry to the table
 * 
 * @param   this   The fd table
 * @param   key    The key of the entry to add
 * @param   value  The value of the entry to add
 * @return         The previous value associated with the key, 0 if the key was not used.
 *                 0 will also be returned on error, check the `errno` variable.
 */
size_t
fd_table_put(fd_table_t *restrict this, int key, size_t value)
{
	size_t rc, old_bitcap, new_bitcap, *old_values;
	uint64_t *old_used;

	/* Override current value if the key is already used. */
	if (fd_table_contains_key(this, key)) {
		rc = fd_table_get(this, key);
		this->values[key] = value;
		return rc;
	}

	/* Grow the table if it is too small. */
	errno = 0;
	if ((size_t)key >= this->capacity) {
		old_values = this->values;
		if (xrealloc(this->values, this->capacity << 1, size_t)) {
			this->values = old_values;
			fail_if (1);
		}

		memset(this->values + this->capacity, 0, this->capacity * sizeof(size_t));
      
		old_bitcap = (this->capacity + 63) / 64;
		this->capacity <<= 1;
		new_bitcap = (this->capacity + 63) / 64;

		if (new_bitcap > old_bitcap) {
			old_used = this->used;
			if (xrealloc(this->used, new_bitcap, size_t)) {
				this->used = old_used;
				this->capacity >>= 1;
				fail_if (1);
			}

			memset(this->used + old_bitcap, 0, (new_bitcap - old_bitcap) * sizeof(uint64_t));
		}
	}

	/* Store the entry. */
	this->used[key / 64] |= (uint64_t)1 << (key % 64);
	this->values[key] = value;
	this->size++;
	return 0;
fail:
	return 0;
}


/**
 * Remove an entry in the table
 * 
 * @param   this  The fd table
 * @param   key   The key of the entry to remove
 * @return        The previous value associated with the key, 0 if the key was not used
 */
size_t
fd_table_remove(fd_table_t *restrict this, int key)
{
	size_t rc = fd_table_get(this, key);
	if (rc && fd_table_contains_key(this, key)) {
		this->used[key / 64] &= ~((uint64_t)1 << (key % 64));
		this->size--;
	}
	return rc;
}


/**
 * Remove all entries in the table
 * 
 * @param  this  The fd table
 */
void
fd_table_clear(fd_table_t *restrict this)
{
	size_t bitcap;
	this->size = 0;
	bitcap = (this->capacity + 63) / 64;
	memset(this->used, 0, bitcap * sizeof(uint64_t));
}


/**
 * Calculate the buffer size need to marshal a fd table
 * 
 * @param   this  The fd table
 * @return        The number of bytes to allocate to the output buffer
 */
size_t
fd_table_marshal_size(const fd_table_t *restrict this)
{
	size_t bitcap = (this->capacity + 63) / 64;
	return (this->capacity + 2) * sizeof(size_t) + bitcap * sizeof(uint64_t) + sizeof(int);
}


/**
 * Marshals a fd table
 * 
 * @param  this  The fd table
 * @param  data  Output buffer for the marshalled data
 */
void
fd_table_marshal(const fd_table_t *restrict this, char *restrict data)
{
	size_t bitcap = (this->capacity + 63) / 64;

	buf_set_next(data, int, FD_TABLE_T_VERSION);
	buf_set_next(data, size_t, this->capacity);
	buf_set_next(data, size_t, this->size);

	memcpy(data, this->values, this->capacity * sizeof(size_t));
	buf_next(data, size_t, this->capacity);

	memcpy(data, this->used, bitcap * sizeof(uint64_t));
}


/**
 * Unmarshals a fd table
 * 
 * @param   this      Memory slot in which to store the new fd table
 * @param   data      In buffer with the marshalled data
 * @param   remapper  Function that translates values, `NULL` if not translation takes place
 * @return            Non-zero on error, `errno` will be set accordingly.
 *                    Destroy the table on error.
 */
int
fd_table_unmarshal(fd_table_t *restrict this, char *restrict data, remap_func *remapper)
{
	size_t bitcap;
	size_t i;

	/* buf_get(data, int, 0, FD_TABLE_T_VERSION) */
	buf_next(data, int, 1);

	buf_get_next(data, size_t, this->capacity);
	buf_get_next(data, size_t, this->size);

	this->values           = NULL;
	this->used             = NULL;
	this->value_comparator = NULL;

	fail_if (xmalloc(this->values, this->capacity, size_t));

	bitcap = (this->capacity + 63) / 64;
	fail_if (xmalloc(this->used, bitcap, size_t));

	memcpy(this->values, data, this->capacity * sizeof(size_t));
	buf_next(data, size_t, this->capacity);

	memcpy(this->used, data, bitcap * sizeof(uint64_t));

	if (remapper)
		for (i = 0; i < this->capacity; i++)
			if (this->used[i / 64] & ((uint64_t)1 << (i % 64)))
				this->values[i] = remapper(this->values[i]);

	return 0;
fail:
	return -1;
}
