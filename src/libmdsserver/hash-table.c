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
#include "hash-table.h"

#include "macros.h"

#include <stdlib.h>
#include <errno.h>


/**
 * Test if a key matches the key in a bucket
 * 
 * @param  T  The instance of the hash table
 * @param  B  The bucket
 * @param  K  The key
 * @param  H  The hash of the key
 */
#define TEST_KEY(T, B, K, H) \
  ((B->key == K) || (T->key_comparator && (B->hash == H) && T->key_comparator(B->key, K)))


/**
 * Calculate the hash of a key
 * 
 * @param   this  The hash table
 * @param   key   The key to hash
 * @return        The hash of the key
 */
static inline size_t __attribute__((const)) hash(const hash_table_t* restrict this, size_t key)
{
  return this->hasher ? this->hasher(key) : key;
}


/**
 * Truncates the hash of a key to constrain it to the buckets
 * 
 * @param   this  The hash table
 * @param   key   The key to hash
 * @return        A non-negative value less the the table's capacity
 */
static inline size_t __attribute__((pure)) truncate_hash(const hash_table_t* restrict this, size_t hash)
{
  return hash % this->capacity;
}


/**
 * Grow the table
 * 
 * @param   this  The hash table
 * @return        Non zero on error, errno will be set accordingly
 */
static int rehash(hash_table_t* restrict this)
{
  hash_entry_t** old_buckets = this->buckets;
  size_t old_capacity = this->capacity;
  size_t i = old_capacity, index;
  hash_entry_t* bucket;
  hash_entry_t* destination;
  hash_entry_t* next;
  
  this->buckets = calloc((old_capacity * 2 + 1), sizeof(hash_entry_t*));
  if (this->buckets == NULL)
    return -1;
  this->capacity = old_capacity * 2 + 1;
  this->threshold = (size_t)((float)(this->capacity) * this->load_factor);
  
  while (i)
    {
      bucket = this->buckets[--i];
      while (bucket)
	{
	  index = truncate_hash(this, bucket->hash);
	  if ((destination = this->buckets[index]))
	    {
	      next = destination->next;
	      while (next)
		{
		  destination = next;
		  next = destination->next;
		}
	      destination->next = bucket;
	    }
	  else
	    this->buckets[index] = bucket;
	  
	  next = bucket->next;
	  bucket->next = NULL;
	  bucket = next;
	}
    }
  
  free(old_buckets);
  return 0;
}


/**
 * Create a hash table
 * 
 * @param   this              Memory slot in which to store the new hash table
 * @param   initial_capacity  The initial capacity of the table
 * @param   load_factor       The load factor of the table, i.e. when to grow the table
 * @return                    Non-zero on error, `errno` will have been set accordingly
 */
int hash_table_create_fine_tuned(hash_table_t* restrict this, size_t initial_capacity, float load_factor)
{
  this->buckets = NULL;
  
  this->capacity = initial_capacity ? initial_capacity : 1;
  this->buckets = calloc(this->capacity, sizeof(hash_entry_t*));
  if (this->buckets == NULL)
    return -1;
  this->load_factor = load_factor;
  this->threshold = (size_t)((float)(this->capacity) * load_factor);
  this->size = 0;
  this->value_comparator = NULL;
  this->key_comparator = NULL;
  this->hasher = NULL;
  
  return 0;
}


/**
 * Release all resources in a hash table, should
 * be done even if construction fails
 * 
 * @param  this          The hash table
 * @param  keys_freer    Function that frees a key, `NULL` if keys should not be freed
 * @param  values_freer  Function that frees a value, `NULL` if value should not be freed
 */
void hash_table_destroy(hash_table_t* restrict this, free_func* key_freer, free_func* value_freer)
{
  size_t i = this->capacity;
  hash_entry_t* bucket;
  hash_entry_t* last;
  
  if (this->buckets != NULL)
    {
      while (i)
	{
	  bucket = this->buckets[--i];
	  while (bucket)
	    {
	      if (key_freer != NULL)
		key_freer(bucket->key);
	      if (value_freer != NULL)
		value_freer(bucket->value);
	      bucket = (last = bucket)->next;
	      free(last);
	    }
	}
      free(this->buckets);
    }
}


/**
 * Check whether a value is stored in the table
 * 
 * @param   this   The hash table
 * @param   value  The value
 * @return         Whether the value is stored in the table
 */
int hash_table_contains_value(const hash_table_t* restrict this, size_t value)
{
  size_t i = this->capacity;
  hash_entry_t* restrict bucket;
  
  while (i)
    {
      bucket = this->buckets[--i];
      while (bucket != NULL)
	{
	  if (bucket->value == value)
	    return 1;
	  if (this->value_comparator && this->value_comparator(bucket->value, value))
	    return 1;
	  bucket = bucket->next;
	}
    }
  
  return 0;
}


/**
 * Check whether a key is used in the table
 * 
 * @param   this  The hash table
 * @param   key   The key
 * @return        Whether the key is used
 */
int hash_table_contains_key(const hash_table_t* restrict this, size_t key)
{
  size_t key_hash = hash(this, key);
  size_t index = truncate_hash(this, key_hash);
  hash_entry_t* restrict bucket = this->buckets[index];
  
  while (bucket)
    {
      if (TEST_KEY(this, bucket, key, key_hash))
	return 1;
      bucket = bucket->next;
    }
  
  return 0;
}


/**
 * Look up a value in the table
 * 
 * @param   this  The hash table
 * @param   key   The key associated with the value
 * @return        The value associated with the key, 0 if the key was not used
 */
size_t hash_table_get(const hash_table_t* restrict this, size_t key)
{
  size_t key_hash = hash(this, key);
  size_t index = truncate_hash(this, key_hash);
  hash_entry_t* restrict bucket = this->buckets[index];
  
  while (bucket)
    {
      if (TEST_KEY(this, bucket, key, key_hash))
	return bucket->value;
      bucket = bucket->next;
    }
  
  return 0;
}


/**
 * Add an entry to the table
 * 
 * @param   this   The hash table
 * @param   key    The key of the entry to add
 * @param   value  The value of the entry to add
 * @return         The previous value associated with the key, 0 if the key was not used.
 *                 0 will also be returned on error, check the `errno` variable.
 */
size_t hash_table_put(hash_table_t* restrict this, size_t key, size_t value)
{
  size_t key_hash = hash(this, key);
  size_t index = truncate_hash(this, key_hash);
  hash_entry_t* restrict bucket = this->buckets[index];
  size_t rc;
  
  while (bucket)
    if (TEST_KEY(this, bucket, key, key_hash))
      {
	rc = bucket->value;
	bucket->value = value;
	return rc;
      }
    else
      bucket = bucket->next;
  
  if (++(this->size) > this->threshold)
    {
      errno = 0;
      if (rehash(this))
	return 0;
      index = truncate_hash(this, key_hash);
    }
  
  errno = 0;
  bucket = malloc(sizeof(hash_entry_t));
  if (bucket == NULL)
    return 0;
  bucket->value = value;
  bucket->key = key;
  bucket->hash = key_hash;
  bucket->next = this->buckets[index];
  this->buckets[index] = bucket;
  
  return 0;
}


/**
 * Remove an entry in the table
 * 
 * @param   this  The hash table
 * @param   key   The key of the entry to remove
 * @return        The previous value associated with the key, 0 if the key was not used
 */
size_t hash_table_remove(hash_table_t* restrict this, size_t key)
{
  size_t key_hash = hash(this, key);
  size_t index = truncate_hash(this, key_hash);
  hash_entry_t* bucket = this->buckets[index];
  hash_entry_t* last = NULL;
  size_t rc;
  
  while (bucket)
    {
      if (TEST_KEY(this, bucket, key, key_hash))
	{
	  if (last == NULL)
	    this->buckets[index] = bucket->next;
	  else
	    last->next = bucket->next;
	  this->size--;
	  rc = bucket->value;
	  free(bucket);
	  return rc;
	}
      last = bucket;
      bucket = bucket->next;
    }
  
  return 0;
}


/**
 * Remove all entries in the table
 * 
 * @param  this  The hash table
 */
void hash_table_clear(hash_table_t* restrict this)
{
  hash_entry_t** buf;
  hash_entry_t* bucket;
  size_t i, ptr;
  
  if (this->size)
    {
      buf = alloca((this->size + 1) * sizeof(hash_entry_t*));
      i = this->capacity;
      while (i)
	{
	  bucket = this->buckets[--i];
	  ptr = 0;
	  buf[ptr++] = bucket;
	  while (bucket)
	    {
	      bucket = bucket->next;
	      buf[ptr++] = bucket;
	    }
	  while (ptr)
	    free(buf[--ptr]);
	  this->buckets[i] = NULL;
	}
      this->size = 0;
    }
}


/**
 * Calculate the buffer size need to marshal a hash table
 * 
 * @param   this  The hash table
 * @return        The number of bytes to allocate to the output buffer
 */
size_t hash_table_marshal_size(const hash_table_t* restrict this)
{
  size_t n = this->capacity;
  size_t rc = 3 * sizeof(size_t) + sizeof(float) + n * sizeof(size_t);
  size_t i, m = 0;
  
  for (i = 0; i < n; i++)
    {
      hash_entry_t* restrict bucket = this->buckets[i];
      while (bucket != NULL)
	{
	  bucket = bucket->next;
	  m++;
	}
    }
  
  return rc + m * 3 * sizeof(size_t) + sizeof(int);
}


/**
 * Marshals a hash table
 * 
 * @param  this  The hash table
 * @param  data  Output buffer for the marshalled data
 */
void hash_table_marshal(const hash_table_t* restrict this, char* restrict data)
{
  size_t i, n = this->capacity;
  
  buf_set(data, int, 0, HASH_TABLE_T_VERSION);
  buf_next(data, int, 1);
  
  buf_set(data, size_t, 0, this->capacity);
  buf_next(data, size_t, 1);
  buf_set(data, float, 0, this->load_factor);
  buf_next(data, float, 1);
  buf_set(data, size_t, 0, this->threshold);
  buf_set(data, size_t, 1, this->size);
  buf_next(data, size_t, 2);
  
  for (i = 0; i < n; i++)
    {
      hash_entry_t* restrict bucket = this->buckets[i];
      size_t m = 0;
      while (bucket != NULL)
	{
	  buf_set(data, size_t, 1 + m * 3 + 0, bucket->key);
	  buf_set(data, size_t, 1 + m * 3 + 1, bucket->value);
	  buf_set(data, size_t, 1 + m * 3 + 2, bucket->hash);
	  bucket = bucket->next;
	  m++;
	}
      buf_set(data, size_t, 0, m);
      buf_next(data, size_t, 1 + m * 3);
    }
}


/**
 * Unmarshals a hash table
 * 
 * @param   this      Memory slot in which to store the new hash table
 * @param   data      In buffer with the marshalled data
 * @param   remapper  Function that translates values, `NULL` if not translation takes place
 * @return            Non-zero on error, errno will be set accordingly.
 *                    Destroy the table on error.
 */
int hash_table_unmarshal(hash_table_t* restrict this, char* restrict data, remap_func* remapper)
{
  size_t i, n;
  
  /* buf_get(data, int, 0, HASH_TABLE_T_VERSION); */
  buf_next(data, int, 1);
  
  this->value_comparator = NULL;
  this->key_comparator   = NULL;
  this->hasher           = NULL;
  
  buf_get(data, size_t, 0, this->capacity = n);
  buf_next(data, size_t, 1);
  buf_get(data, float, 0, this->load_factor);
  buf_next(data, float, 1);
  buf_get(data, size_t, 0, this->threshold);
  buf_get(data, size_t, 1, this->size);
  buf_next(data, size_t, 2);
  
  this->buckets = calloc(this->capacity, sizeof(hash_entry_t*));
  if (this->buckets == NULL)
    return -1;
  
  for (i = 0; i < n; i++)
    {
      size_t m;
      hash_entry_t* restrict bucket;
      buf_get(data, size_t, 0, m);
      buf_next(data, size_t, 1);
      
      this->buckets[i] = bucket = malloc(sizeof(hash_entry_t));
      if (bucket == NULL)
	return -1;
      
      while (m--)
	{
	  if (m == 0)
	    bucket->next = NULL;
	  else
	    {
	      bucket->next = malloc(sizeof(hash_entry_t));
	      if (bucket->next == NULL)
		return -1;
	    }
	  buf_get(data, size_t, 0, bucket->key);
	  buf_get(data, size_t, 1, bucket->value);
	  if (remapper != NULL)
	    bucket->value = remapper(bucket->value);
	  buf_get(data, size_t, 2, bucket->hash);
	  buf_next(data, size_t, 3);
	}
    }
  
  return 0;
}

