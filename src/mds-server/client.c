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
#include "client.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>



/**
 * Calculate the buffer size need to marshal client information
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t client_marshal_size(const client_t* restrict this)
{
  size_t n = sizeof(ssize_t) + 2 * sizeof(int) + sizeof(uint64_t) + 3 * sizeof(size_t);
  size_t i;
  
  n += mds_message_marshal_size(&(this->message), 1);
  for (i = 0; i < this->interception_conditions_count; i++)
    n += interception_condition_marshal_size(this->interception_conditions + i);
  n += this->send_pending_size * sizeof(char);
  
  return n;
}


/**
 * Marshals client information
 * 
 * @param   this  The client information
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t client_marshal(const client_t* restrict this, char* restrict data)
{
  size_t i, n;
  buf_set_next(data, ssize_t, this->list_entry);
  buf_set_next(data, int, this->socket_fd);
  buf_set_next(data, int, this->open);
  buf_set_next(data, uint64_t, this->id);
  n = mds_message_marshal_size(&(this->message), 1);;
  buf_set_next(data, size_t, n);
  mds_message_marshal(&(this->message), data, 1);
  data += n / sizeof(char);
  buf_set_next(data, size_t, this->interception_conditions_count);
  for (i = 0; i < this->interception_conditions_count; i++)
    data += interception_condition_marshal(this->interception_conditions + i, data) / sizeof(char);
  buf_set_next(data, size_t, this->send_pending_size);
  if (this->send_pending_size > 0)
    memcpy(data, this->send_pending, this->send_pending_size * sizeof(char));
  return client_marshal_size(this);
}


/**
 * Unmarshals client information
 * 
 * @param   this  Memory slot in which to store the new client information
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, errno will be set accordingly, otherwise the number of read bytes
 */
size_t client_unmarshal(client_t* restrict this, char* restrict data)
{
  size_t i, n, rc = sizeof(ssize_t) + 2 * sizeof(int) + sizeof(uint64_t) + 3 * sizeof(size_t);
  this->interception_conditions = NULL;
  this->send_pending = NULL;
  buf_get_next(data, ssize_t, this->list_entry);
  buf_get_next(data, int, this->socket_fd);
  buf_get_next(data, int, this->open);
  buf_get_next(data, uint64_t, this->id);
  buf_get_next(data, size_t, n);
  if (mds_message_unmarshal(&(this->message), data))
    return 0;
  data += n / sizeof(char);
  rc += n;
  buf_get_next(data, size_t, this->interception_conditions_count);
  if (xmalloc(this->interception_conditions, this->interception_conditions_count, interception_condition_t))
    goto fail;
  for (i = 0; i < this->interception_conditions_count; i++)
    {
      n = interception_condition_unmarshal(this->interception_conditions + i, data);
      if (n == 0)
	{
	  this->interception_conditions_count = i - 1;
	  goto fail;
	}
      data += n / sizeof(char);
      rc += n;
    }
  buf_get_next(data, size_t, this->send_pending_size);
  if (this->send_pending_size > 0)
    {
      if (xmalloc(this->send_pending, this->send_pending_size, char))
	goto fail;
      memcpy(this->send_pending, data, this->send_pending_size * sizeof(char));
    }
  return rc;
  
 fail:
  mds_message_destroy(&(this->message));
  for (i = 0; i < this->interception_conditions_count; i++)
    free(this->interception_conditions[i].condition);
  free(this->interception_conditions);
  free(this->send_pending);
  return 0;
}

/**
 * Pretend to unmarshal client information
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t client_unmarshal_skip(char* restrict data)
{
  size_t n, c, rc = sizeof(ssize_t) + 2 * sizeof(int) + sizeof(uint64_t) + 3 * sizeof(size_t);
  buf_next(data, ssize_t, 1);
  buf_next(data, int, 2);
  buf_next(data, uint64_t, 1);
  buf_get_next(data, size_t, n);
  data += n / sizeof(char);
  rc += n;
  buf_get_next(data, size_t, c);
  while (c--)
    {
      n = interception_condition_unmarshal_skip(data);
      data += n / sizeof(char);
      rc += n;
    }
  buf_get_next(data, size_t, n);
  rc += n * sizeof(char);
  return rc;
}

