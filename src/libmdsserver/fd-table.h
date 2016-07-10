/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_LIBMDSSERVER_FD_TABLE_H
#define MDS_LIBMDSSERVER_FD_TABLE_H


#include "table-common.h"

#include <stdint.h>



#define FD_TABLE_T_VERSION  0

/**
 * Value lookup table optimised for file descriptors as keys
 */
typedef struct fd_table
{
  /**
   * The table's capacity, i.e. how many entries that can be stored,
   * in total, before its internal table needs to grow
   */
  size_t capacity;
  
  /**
   * The number of entries stored in the table
   */
  size_t size;
  
  /**
   * Map from keys to values
   */
  size_t* values;
  
  /**
   * Map from keys to whether that are in used, bit-packed
   */
  uint64_t* used;
  
  /**
   * Check whether two values are equal
   * 
   * If this function pointer is `NULL`, the identity is used
   * 
   * Be aware, this variable cannot be marshalled
   */
  compare_func* value_comparator;
  
} fd_table_t;



/**
 * Create a fd table
 * 
 * @param   this              Memory slot in which to store the new fd table
 * @param   initial_capacity  The initial capacity of the table
 * @return                    Non-zero on error, `errno` will have been set accordingly
 */
__attribute__((nonnull))
int fd_table_create_tuned(fd_table_t* restrict this, size_t initial_capacity);

/**
 * Create a fd table
 * 
 * @param   this:fd_table_t*  Memory slot in which to store the new fd table
 * @return  :int              Non-zero on error, `errno` will have been set accordingly
 */
#define fd_table_create(this)  \
  fd_table_create_tuned(this, 16)

/**
 * Release all resources in a fd table, should
 * be done even if construction fails
 * 
 * @param  this          The fd table
 * @param  keys_freer    Function that frees a key, `NULL` if keys should not be freed
 * @param  values_freer  Function that frees a value, `NULL` if value should not be freed
 */
__attribute__((nonnull(1)))
void fd_table_destroy(fd_table_t* restrict this, free_func* key_freer, free_func* value_freer);

/**
 * Check whether a value is stored in the table
 * 
 * @param   this   The fd table
 * @param   value  The value
 * @return         Whether the value is stored in the table
 */
__attribute__((pure, nonnull))
int fd_table_contains_value(const fd_table_t* restrict this, size_t value);

/**
 * Check whether a key is used in the table
 * 
 * @param   this  The fd table
 * @param   key   The key
 * @return        Whether the key is used
 */
__attribute__((pure, nonnull))
int fd_table_contains_key(const fd_table_t* restrict this, int key);

/**
 * Look up a value in the table
 * 
 * @param   this  The fd table
 * @param   key   The key associated with the value
 * @return        The value associated with the key, 0 if the key was not used
 */
__attribute__((pure, nonnull))
size_t fd_table_get(const fd_table_t* restrict this, int key);

/**
 * Add an entry to the table
 * 
 * @param   this   The fd table
 * @param   key    The key of the entry to add
 * @param   value  The value of the entry to add
 * @return         The previous value associated with the key, 0 if the key was not used.
 *                 0 will also be returned on error, check the `errno` variable.
 */
__attribute__((nonnull))
size_t fd_table_put(fd_table_t* restrict this, int key, size_t value);

/**
 * Remove an entry in the table
 * 
 * @param   this  The fd table
 * @param   key   The key of the entry to remove
 * @return        The previous value associated with the key, 0 if the key was not used
 */
__attribute__((nonnull))
size_t fd_table_remove(fd_table_t* restrict this, int key);

/**
 * Remove all entries in the table
 * 
 * @param  this  The fd table
 */
__attribute__((nonnull))
void fd_table_clear(fd_table_t* restrict this);

/**
 * Calculate the buffer size need to marshal a fd table
 * 
 * @param   this  The fd table
 * @return        The number of bytes to allocate to the output buffer
 */
__attribute__((pure, nonnull))
size_t fd_table_marshal_size(const fd_table_t* restrict this);

/**
 * Marshals a fd table
 * 
 * @param  this  The fd table
 * @param  data  Output buffer for the marshalled data
 */
__attribute__((nonnull))
void fd_table_marshal(const fd_table_t* restrict this, char* restrict data);

/**
 * Unmarshals a fd table
 * 
 * @param   this      Memory slot in which to store the new fd table
 * @param   data      In buffer with the marshalled data
 * @param   remapper  Function that translates values, `NULL` if not translation takes place
 * @return            Non-zero on error, `errno` will be set accordingly.
 *                    Destroy the table on error.
 */
__attribute__((nonnull(1, 2)))
int fd_table_unmarshal(fd_table_t* restrict this, char* restrict data, remap_func* remapper);


#endif

