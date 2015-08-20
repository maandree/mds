/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_LIBMDSSERVER_HASH_LIST_H
#define MDS_LIBMDSSERVER_HASH_LIST_H


#include "macros.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>




/**
 * The default initial capacity
 */
#ifndef HASH_LIST_DEFAULT_INITIAL_CAPACITY
# define HASH_LIST_DEFAULT_INITIAL_CAPACITY  128
#endif


#define HASH_LIST_HASH(key)  (this->hasher != NULL ? this->hasher(key) : (size_t)key)



#define HASH_LIST_T_VERSION  0


/**
 * Create a subclass of hash_list
 * 
 * @param  T        The replacement text for `hash_list`
 * @param  KEY_T    The datatype of the keys
 * @param  CKEY_T   `const` version of `KEY_T`
 * @param  VALUE_T  The datatype of the values
 */
#define CREATE_HASH_LIST_SUBCLASS(T, KEY_T, CKEY_T, VALUE_T)\
\
\
/**
 * Slot for a value in a hash list
 */\
struct T##_entry;\
\
/**
 * Slot for a value in a hash list
 */\
struct T;\
\
\
\
/**
 * The datatype of the value
 */\
typedef VALUE_T T##_value_t;\
\
/**
 * Function-type for the function responsible for freeing the key and value of an entry
 * 
 * @param  entry  The entry, will never be `NULL`, any only used entries will be passed
 */\
typedef void T##_entry_free_func(struct T##_entry* entry);\
\
/**
 * Function-type for the function responsible for hashing keys
 * 
 * @param   key  The key, will never be `NULL`
 * @return       The hash of the key
 */\
typedef size_t T##_key_hash_func(CKEY_T key);\
\
\
\
/**
 * Comparing keys
 * 
 * @param   key_a  The first key, will never be `NULL`
 * @param   key_b  The second key, will never be `NULL`
 * @return         Whether the keys are equal
 */\
static inline int T##_key_comparer(CKEY_T key_a, CKEY_T key_b);\
\
/**
 * Determine the marshal-size of an entry's key and value
 * 
 * @param   entry  The entry, will never be `NULL`, any only used entries will be passed
 * @return         The marshal-size of the entry's key and value
 */\
static inline size_t T##_submarshal_size(const struct T##_entry* entry);\
\
/**
 * Marshal an entry's key and value
 * 
 * @param   entry  The entry, will never be `NULL`, any only used entries will be passed
 * @return         The marshal-size of the entry's key and value
 */\
static inline size_t T##_submarshal(const struct T##_entry* entry, char* restrict data);\
\
/**
 * Unmarshal an entry's key and value
 * 
 * @param   entry  The entry, will never be `NULL`, any only used entries will be passed
 * @return         The number of read bytes, zero on error
 */\
static inline size_t T##_subunmarshal(struct T##_entry* entry, char* restrict data);\
\
\
\
/**
 * Slot for a value in a hash list
 */\
typedef struct T##_entry\
{\
  /**
   * The key of the entry, `NULL` if the slot is unused
   */\
  KEY_T key;\
  \
  /**
   * Hash of `key`, undefined but initialised if the slot is unused
   */\
  size_t key_hash;\
  \
  /**
   * The value of the entry
   */\
  T##_value_t value;\
  \
} T##_entry_t;\
\
\
/**
 * The data structure of the hash list
 */\
typedef struct T\
{\
  /**
   * The number of allocated slots
   */\
  size_t allocated;\
  \
  /**
   * The number of unused slot that
   * has previously be used
   */\
  size_t unused;\
  \
  /**
   * The number of slots that have
   * been used, even if no longer used
   */\
  size_t used;\
  \
  /**
   * The slots
   */\
  T##_entry_t* slots;\
  \
  /**
   * Function used to free keys and values of entries
   * 
   * The freeing is not used if this function pointer is `NULL`
   * 
   * Be aware, this variable cannot be marshalled
   */\
  T##_entry_free_func* freer;\
  \
  /**
   * Function used to calculate the hash of a key
   * 
   * If this function pointer is `NULL`, the identity hash is used
   * 
   * Be aware, this variable cannot be marshalled
   */\
  T##_key_hash_func* hasher;\
  \
} T##_t;\
\
\
\
/**
 * Create a hash list
 * 
 * @param   this      Memory slot in which to store the new hash list
 * @param   capacity  The minimum initial capacity of the hash list, 0 for default
 * @return            Non-zero on error, `errno` will have been set accordingly
 */\
static inline int __attribute__((unused))\
T##_create(T##_t* restrict this, size_t capacity)\
{\
  if (capacity == 0)\
    capacity = HASH_LIST_DEFAULT_INITIAL_CAPACITY;\
  \
  this->freer = NULL;\
  this->hasher = NULL;\
  this->allocated = 0;\
  this->unused = 0;\
  this->used = 0;\
  \
  this->slots = malloc(capacity * sizeof(T##_t));\
  if (this->slots == NULL)\
    return -1;\
  \
  this->allocated = capacity;\
  return 0;\
}\
\
\
/**
 * Release all resources in a hash list
 * 
 * @param  this  The hash list
 */\
static inline void __attribute__((unused))\
T##_destroy(T##_t* restrict this)\
{\
  size_t i, n;\
  if (this->freer != NULL)\
    for (i = 0, n = this->used; i < n; i++)\
      if (this->slots[i].key)\
	this->freer(this->slots + i);\
  this->used = 0;\
  this->unused = 0;\
  this->allocated = 0;\
  free(this->slots);\
  this->slots = NULL;\
}\
\
\
/**
 * Clone a hash list
 * 
 * @param   this  The hash list to clone
 * @param   out   Memory slot in which to store the new hash list
 * @return        Non-zero on error, `errno` will have been set accordingly
 */\
static inline int __attribute__((unused))\
T##_clone(const T##_t* restrict this, T##_t* restrict out)\
{\
  if (T##_create(out, this->allocated) < 0)\
    return -1;\
  out->used = this->used;\
  out->unused = this->unused;\
  memcpy(out->slots, this->slots, this->used * sizeof(T##_entry_t));\
}\
\
\
/**
 * Pack the list so that there are no reusable
 * positions, and reduce the capacity to the
 * smallest capacity that can be used.
 * This method has linear time complexity and
 * constant memory complexity.
 * 
 * @param   this  The list
 * @return        Non-zero on error, `errno` will have
 *                been set accordingly. Errors are non-fatal.
 */\
static inline int __attribute__((unused))\
T##_pack(T##_t* restrict this)\
{\
  size_t i, j, n;\
  T##_entry_t* slots = this->slots;\
  \
  if (this->unused > 0)\
    {\
      for (i = 0, j = 0, n = this->used; i < n; i++)\
	if (slots[i].key != NULL)\
	  slots[j++] = slots[i];\
      \
      this->used -= this->unused;\
      this->unused = 0;\
    }\
  \
  if (this->used < this->allocated)\
    {\
      slots = realloc(slots, this->used * sizeof(T##_entry_t));\
      if (slots == NULL)\
	return -1;\
      this->slots = slots;\
      this->allocated = this->used;\
    }\
  \
  return 0;\
}\
\
\
/**
 * Look up a value in a hash list
 * 
 * @param   this   The hash list
 * @param   key    The key associated with the value, must not be `NULL`
 * @param   value  Output parameter for the value
 * @return         Whether the key was found, error is impossible
 */\
static inline int __attribute__((unused))\
T##_get(const T##_t* restrict this, CKEY_T key, T##_value_t* restrict value)\
{\
  size_t i, n, hash = HASH_LIST_HASH(key);\
  for (i = 0, n = this->used; i < n; i++)\
    if ((this->slots[i].key_hash == hash) && this->slots[i].key)\
      if (T##_key_comparer(this->slots[i].key, key))\
	return *value = this->slots[i].value, 1;\
  return 0;\
}\
\
\
/**
 * Remove an entry from a hash list
 * 
 * @param  this  The hash list
 * @param  key   The key of the entry to remove, must not be `NULL`
 */\
static inline void __attribute__((unused))\
T##_remove(T##_t* restrict this, CKEY_T key)\
{\
  size_t i, n, hash = HASH_LIST_HASH(key);\
  T##_entry_t* slots = this->slots;\
  for (i = 0, n = this->used; i < n; i++)\
    if ((slots[i].key_hash == hash) && (slots[i].key != NULL))\
      if (T##_key_comparer(slots[i].key, key))\
	{\
	  if (this->freer != NULL)\
	    this->freer(slots + i);\
	  slots[i].key = NULL;\
	  this->unused++;\
	  if ((this->unused >> 1) >= this->used)\
	    T##_pack(this);\
	  return;\
	}\
}\
\
\
/**
 * Add an entry to a hash list
 * 
 * @param   this   The hash list
 * @param   key    The key of the entry to add, must not be `NULL`
 * @param   value  Pointer to the value of the entry to add,
 *                 `NULL` if the entry should be removed instead
 * @return         Non-zero on error, `errno` will have been set accordingly
 */\
static inline int __attribute__((unused))\
T##_put(T##_t* restrict this, KEY_T key, const T##_value_t* restrict value)\
{\
  size_t i, n, empty, hash;\
  T##_entry_t* slots = this->slots;\
  \
  /* Remove entry if no value is passed */\
  if (value == NULL)\
    {\
      T##_remove(this, key);\
      return 0;\
    }\
  \
  /* Find an unused slot or the current slot */\
  empty = this->used, hash = HASH_LIST_HASH(key);\
  for (i = 0, n = this->used; i < n; i++)\
    if (slots[i].key == NULL)\
      empty = i;\
    else if (slots[i].key_hash == hash)\
      if (T##_key_comparer(slots[i].key, key))\
	{\
	  if (this->freer != NULL)\
	    this->freer(slots + i);\
	  goto put;\
	}\
  \
  /* Grow slot allocation is required */\
  if (empty == this->allocated)\
    {\
      if (empty >= SIZE_MAX >> 1)\
	return errno = ENOMEM, -1;\
      slots = realloc(slots, (empty << 1) * sizeof(T##_entry_t));\
      if (slots == NULL)\
	return -1;\
      this->slots = slots;\
      this->allocated <<= 1;\
    }\
  \
  /* Store entry */\
  i = empty;\
 put:\
  slots[i].key = key;\
  slots[i].key_hash = hash;\
  slots[i].value = *value;\
  return 0;\
}\
\
\
/**
 * Calculate the buffer size need to marshal a hash list
 * 
 * @param   this  The hash table
 * @return        The number of bytes to allocate to the output buffer
 */\
static inline size_t __attribute__((unused))\
T##_marshal_size(const T##_t* restrict this)\
{\
  size_t i, n = this->used;\
  size_t rc = sizeof(int) + 3 * sizeof(size_t);\
  for (i = 0; i < n; i++)\
    if (this->slots[i].key != NULL)\
      rc += T##_submarshal_size(this->slots + i);\
  return rc + n * sizeof(char) + (n - this->unused) * sizeof(size_t);\
}\
\
\
/**
 * Marshals a hash list
 * 
 * @param  this  The hash list
 * @param  data  Output buffer for the marshalled data
 */\
static inline void __attribute__((unused))\
T##_marshal(const T##_t* restrict this, char* restrict data)\
{\
  size_t wrote, i, n = this->used;\
  \
  buf_set_next(data, int, HASH_LIST_T_VERSION);\
  buf_set_next(data, size_t, this->allocated);\
  buf_set_next(data, size_t, this->unused);\
  buf_set_next(data, size_t, this->used);\
  \
  for (i = 0; i < n; i++)\
    if (this->slots[i].key != NULL)\
      {\
	buf_set_next(data, char, 1);\
	buf_set_next(data, size_t, this->slots[i].key_hash);\
	wrote = T##_submarshal(this->slots + i, data);\
	data += wrote / sizeof(char);\
      }\
    else\
      buf_set_next(data, char, 0);\
}\
\
\
/**
 * Unmarshals a hash list
 * 
 * @param   this      Memory slot in which to store the new hash list
 * @param   data      In buffer with the marshalled data
 * @return            Non-zero on error, `errno` will be set accordingly.
 *                    Destroy the table on error.
 */\
static inline int __attribute__((unused))\
T##_unmarshal(T##_t* restrict this, char* restrict data)\
{\
  size_t i, n, got;\
  char used;\
  \
  this->freer = NULL;\
  this->hasher = NULL;\
  \
  /* buf_get(data, int, 0, HASH_LIST_T_VERSION); */\
  buf_next(data, int, 1);\
  \
  buf_get_next(data, size_t, this->allocated);\
  buf_get_next(data, size_t, this->unused);\
  buf_get_next(data, size_t, this->used);\
  \
  this->slots = calloc(this->allocated, sizeof(T##_entry_t));\
  if (this->slots == NULL)\
    return -1;\
  \
  for (i = 0, n = this->used; i < n; i++)\
    {\
      buf_get_next(data, char, used);\
      if (used == 0)\
	continue;\
      buf_set_next(data, size_t, this->slots[i].key_hash);\
      got = T##_subunmarshal(this->slots + i, data);\
      if (got == 0)\
	return -1;\
      data += got / sizeof(char);\
    }\
  \
  return 0;\
}


/**
 * Wrapper for `for` keyword that iterates over entry element in a hash list
 * 
 * @param  this:hash_list_t         The hash lsit
 * @param  i:size_t                 The variable to store the buckey index in at each iteration
 * @param  entry:hash_list_enty_t*  The variable to store the entry in at each iteration
 */
#define foreach_hash_list_entry(this, i, entry)	\
  for (i = 0; i < (this).used; i++)		\
    if (entry = (this).slots, entry->keys != NULL)


#endif

