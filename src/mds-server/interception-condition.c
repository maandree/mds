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
#include "interception-condition.h"

#include <libmdsserver/macros.h>

#include <string.h>
#include <stdlib.h>



/**
 * Calculate the buffer size need to marshal an interception condition
 * 
 * @param   this  The interception condition
 * @return        The number of bytes to allocate to the output buffer
 */
size_t interception_condition_marshal_size(const interception_condition_t* restrict this)
{
  return sizeof(size_t) + sizeof(int64_t) + 2 * sizeof(int) + (strlen(this->condition) + 1) * sizeof(char);
}

/**
 * Marshals an interception condition
 * 
 * @param   this  The interception condition
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t interception_condition_marshal(const interception_condition_t* restrict this, char* restrict data)
{
  size_t n = (strlen(this->condition) + 1) * sizeof(char);
  buf_set_next(data, int, INTERCEPTION_CONDITION_T_VERSION);
  buf_set_next(data, size_t, this->header_hash);
  buf_set_next(data, int64_t, this->priority);
  buf_set_next(data, int, this->modifying);
  memcpy(data, this->condition, n);
  return sizeof(size_t) + sizeof(int64_t) + 2 * sizeof(int) + n;
}


/**
 * Unmarshals an interception condition
 * 
 * @param   this  Memory slot in which to store the new interception condition
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, `errno` will be set accordingly, otherwise the number of read bytes
 */
size_t interception_condition_unmarshal(interception_condition_t* restrict this, char* restrict data)
{
  size_t n;
  this->condition = NULL;
  /* buf_get_next(data, int, INTERCEPTION_CONDITION_T_VERSION); */
  buf_next(data, int, 1);
  buf_get_next(data, size_t, this->header_hash);
  buf_get_next(data, int64_t, this->priority);
  buf_get_next(data, int, this->modifying);
  n = (strlen(data) + 1) * sizeof(char);
  fail_if ((this->condition = malloc(n)) == NULL);
  memcpy(this->condition, data, n);
  return sizeof(size_t) + sizeof(int64_t) + 2 * sizeof(int) + n;
 fail:
  return 0;
}


/**
 * Pretend to an interception condition
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t interception_condition_unmarshal_skip(char* restrict data)
{
  size_t n = sizeof(size_t) + sizeof(int64_t) + 2 * sizeof(int);
  buf_next(data, int, 1);
  buf_next(data, size_t, 1);
  buf_next(data, int64_t, 1);
  buf_next(data, int, 1);
  n += (strlen(data) + 1) * sizeof(char);
  return n;
}

