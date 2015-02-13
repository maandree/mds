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
#include "multicast.h"

#include "interception-condition.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>


/**
 * Initialise a message multicast state
 * 
 * @param  this  The message multicast state
 */
void multicast_initialise(multicast_t* restrict this)
{
  this->interceptions = NULL;
  this->interceptions_count = 0;
  this->interceptions_ptr = 0;
  this->message = NULL;
  this->message_length = 0;
  this->message_ptr = 0;
  this->message_prefix = 0;
}


/**
 * Destroy a message multicast state
 * 
 * @param  this  The message multicast state
 */
void multicast_destroy(multicast_t* restrict this)
{
  free(this->interceptions);
  free(this->message);
}


/**
 * Calculate the buffer size need to marshal a message multicast state
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t multicast_marshal_size(const multicast_t* restrict this)
{
  size_t rc = sizeof(int) + 5 * sizeof(size_t) + this->message_length * sizeof(char);
  size_t i;
  for (i = 0; i < this->interceptions_count; i++)
    rc += queued_interception_marshal_size();
  return rc;
}


/**
 * Marshals a message multicast state
 * 
 * @param   this  The message multicast state
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t multicast_marshal(const multicast_t* restrict this, char* restrict data)
{
  size_t rc = sizeof(int) + 5 * sizeof(size_t);
  size_t i, n;
  buf_set_next(data, int, MULTICAST_T_VERSION);
  buf_set_next(data, size_t, this->interceptions_count);
  buf_set_next(data, size_t, this->interceptions_ptr);
  buf_set_next(data, size_t, this->message_length);
  buf_set_next(data, size_t, this->message_ptr);
  buf_set_next(data, size_t, this->message_prefix);
  for (i = 0; i < this->interceptions_count; i++)
    {
      n = queued_interception_marshal(this->interceptions + i, data);
      data += n / sizeof(char);
      rc += n;
    }
  if (this->message_length > 0)
    {
      memcpy(data, this->message, this->message_length * sizeof(char));
      rc += this->message_length * sizeof(char);
    }
  return rc;
}


/**
 * Unmarshals a message multicast state
 * 
 * @param   this  Memory slot in which to store the new message multicast state
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, `errno` will be set accordingly, otherwise the
 *                number of read bytes. Destroy the client information on error.
 */
size_t multicast_unmarshal(multicast_t* restrict this, char* restrict data)
{
  size_t rc = sizeof(int) + 5 * sizeof(size_t);
  size_t i, n;
  this->interceptions = NULL;
  this->message = NULL;
  /* buf_get_next(data, int, MULTICAST_T_VERSION); */
  buf_next(data, int, 1);
  buf_get_next(data, size_t, this->interceptions_count);
  buf_get_next(data, size_t, this->interceptions_ptr);
  buf_get_next(data, size_t, this->message_length);
  buf_get_next(data, size_t, this->message_ptr);
  buf_get_next(data, size_t, this->message_prefix);
  if (this->interceptions_count > 0)
    fail_if (xmalloc(this->interceptions, this->interceptions_count, queued_interception_t));
  for (i = 0; i < this->interceptions_count; i++)
    {
      n = queued_interception_unmarshal(this->interceptions + i, data);
      data += n / sizeof(char);
      rc += n;
    }
  if (this->message_length > 0)
    {
      fail_if (xmemdup(this->message, data, this->message_length, char));
      rc += this->message_length * sizeof(char);
    }
  return rc;
 fail:
  return 0;
}


/**
 * Pretend to unmarshal a message multicast state
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t multicast_unmarshal_skip(char* restrict data)
{
  size_t interceptions_count = buf_cast(data, size_t, 0);
  size_t message_length = buf_cast(data, size_t, 2);
  size_t rc = sizeof(int) + 5 * sizeof(size_t) + message_length * sizeof(char);
  size_t n;
  while (interceptions_count--)
    {
      n = queued_interception_unmarshal_skip();
      data += n / sizeof(char);
      rc += n;
    }
  return rc;
}

