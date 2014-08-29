/**
 * mds — A micro-display server
 * Copyright © 2014  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_LIBMDSSERVER_HASH_TABLE_H
#define MDS_LIBMDSSERVER_HASH_TABLE_H


#include "table-common.h"



#define HASH_TABLE_T_VERSION  0

/**
 * Hash table entry
 */
typedef struct hash_entry
{
  /**
   * A key
   */
  size_t key;
  
  /**
   * The value associated with the key
   */
  size_t value;
  
  /**
   * The truncated hash value of the key
   */
  size_t hash;
  
  /**
   * The next entry in the bucket
   */
  struct hash_entry* next;
  
} hash_entry_t;


/**
 * Value lookup table based on hash value, that do not support
 */
typedef struct hash_table
{
  /**
   * The table's capacity, i.e. the number of buckets
   */
  size_t capacity;
  
  /**
   * Entry buckets
   */
  hash_entry_t** buckets;
  
  /**
   * When, in the ratio of entries comparied to the capacity, to grow the table
   */
  float load_factor;
  
  /**
   * When, in the number of entries, to grow the table
   */
  size_t threshold;
  
  /**
   * The number of entries stored in the table
   */
  size_t size;
  
  /**
   * Check whether two values are equal
   * 
   * If this function pointer is `NULL`, the identity is used
   * 
   * Be aware, this variable cannot be marshalled
   */
  compare_func* value_comparator;
  
  /**
   * Check whether two keys are equal
   * 
   * If this function pointer is `NULL`, the identity is used
   * 
   * Be aware, this variable cannot be marshalled
   */
  compare_func* key_comparator;
  
  /**
   * Calculate the hash of a key
   * 
   * If this function pointer is `NULL`, the identity hash is used
   * 
   * Be aware, this variable cannot be marshalled
   * 
   * @param   key  The key
   * @return       The hash of the key
   */
  hash_func* hasher;
  
} hash_table_t;



/**
 * Create a hash table
 * 
 * @param   this              Memory slot in which to store the new hash table
 * @param   initial_capacity  The initial capacity of the table
 * @param   load_factor       The load factor of the table, i.e. when to grow the table
 * @return                    Non-zero on error, `errno` will have been set accordingly
 */
int hash_table_create_fine_tuned(hash_table_t* restrict this, size_t initial_capacity, float load_factor);

/**
 * Create a hash table
 * 
 * @param   this:hash_table_t*       Memory slot in which to store the new hash table
 * @param   initial_capacity:size_t  The initial capacity of the table
 * @return  :int                     Non-zero on error, `errno` will have been set accordingly
 */
#define hash_table_create_tuned(this, initial_capacity)  \
  hash_table_create_fine_tuned(this, initial_capacity, 0.75f)

/**
 * Create a hash table
 * 
 * @param   this:hash_table_t*  Memory slot in which to store the new hash table
 * @return  :int                Non-zero on error, `errno` will have been set accordingly
 */
#define hash_table_create(this)  \
  hash_table_create_tuned(this, 16)

/**
 * Release all resources in a hash table, should
 * be done even if construction fails
 * 
 * @param  this          The hash table
 * @param  keys_freer    Function that frees a key, `NULL` if keys should not be freed
 * @param  values_freer  Function that frees a value, `NULL` if value should not be freed
 */
void hash_table_destroy(hash_table_t* restrict this, free_func* key_freer, free_func* value_freer);

/**
 * Check whether a value is stored in the table
 * 
 * @param   this   The hash table
 * @param   value  The value
 * @return         Whether the value is stored in the table
 */
int hash_table_contains_value(const hash_table_t* restrict this, size_t value) __attribute__((pure));

/**
 * Check whether a key is used in the table
 * 
 * @param   this  The hash table
 * @param   key   The key
 * @return        Whether the key is used
 */
int hash_table_contains_key(const hash_table_t* restrict this, size_t key) __attribute__((pure));

/**
 * Look up a value in the table
 * 
 * @param   this  The hash table
 * @param   key   The key associated with the value
 * @return        The value associated with the key, 0 if the key was not used
 */
size_t hash_table_get(const hash_table_t* restrict this, size_t key);

/**
 * Look up an entry in the table
 * 
 * @param   this  The hash table
 * @param   key   The key associated with the value
 * @return        The entry associated with the key, `NULL` if the key was not used
 */
hash_entry_t* hash_table_get_entry(const hash_table_t* restrict this, size_t key);

/**
 * Add an entry to the table
 * 
 * @param   this   The hash table
 * @param   key    The key of the entry to add
 * @param   value  The value of the entry to add
 * @return         The previous value associated with the key, 0 if the key was not used.
 *                 0 will also be returned on error, check the `errno` variable.
 */
size_t hash_table_put(hash_table_t* restrict this, size_t key, size_t value);

/**
 * Remove an entry in the table
 * 
 * @param   this  The hash table
 * @param   key   The key of the entry to remove
 * @return        The previous value associated with the key, 0 if the key was not used
 */
size_t hash_table_remove(hash_table_t* restrict this, size_t key);

/**
 * Remove all entries in the table
 * 
 * @param  this  The hash table
 */
void hash_table_clear(hash_table_t* restrict this);

/**
 * Wrapper for `for` keyword that iterates over entry element in a hash table
 * 
 * @param  this:hash_table_t    The hash table
 * @param  i:size_t             The variable to store the buckey index in at each iteration
 * @param  entry:hash_entry_t*  The variable to store the entry in at each iteration
 */
#define foreach_hash_table_entry(this, i, entry)  \
  for (i = 0; i < (this).capacity; i++)           \
    for (entry = (this).buckets[i]; entry != NULL; entry = entry->next)

/**
 * Calculate the buffer size need to marshal a hash table
 * 
 * @param   this  The hash table
 * @return        The number of bytes to allocate to the output buffer
 */
size_t hash_table_marshal_size(const hash_table_t* restrict this) __attribute__((pure));

/**
 * Marshals a hash table
 * 
 * @param  this  The hash table
 * @param  data  Output buffer for the marshalled data
 */
void hash_table_marshal(const hash_table_t* restrict this, char* restrict data);

/**
 * Unmarshals a hash table
 * 
 * @param   this      Memory slot in which to store the new hash table
 * @param   data      In buffer with the marshalled data
 * @param   remapper  Function that translates values, `NULL` if not translation takes place
 * @return            Non-zero on error, errno will be set accordingly.
 *                    Destroy the table on error.
 */
int hash_table_unmarshal(hash_table_t* restrict this, char* restrict data, remap_func* remapper);


#endif

