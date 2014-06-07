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

#include "multicast.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>



/**
 * Initialise a client
 * 
 * The following fields will not be initialised:
 * - message
 * - thread
 * - mutex
 * - modify_mutex
 * - modify_cond
 * 
 * The follow fields will be initialised to `-1`:
 * - list_entry
 * - socket_fd
 * 
 * @param  this  Memory slot in which to store the new client information
 */
void client_initialise(client_t* restrict this)
{
  this->list_entry = -1;
  this->socket_fd = -1;
  this->open = 0;
  this->id = 0;
  this->mutex_created = 0;
  this->interception_conditions = NULL;
  this->interception_conditions_count = 0;
  this->multicasts = NULL;
  this->multicasts_count = 0;
  this->send_pending = NULL;
  this->send_pending_size = 0;
  this->modify_message = NULL;
  this->modify_mutex_created = 0;
  this->modify_cond_created = 0;
}


/**
 * Initialise fields that have to do with threading
 * 
 * This method initialises the following fields:
 * - thread
 * - mutex
 * - modify_mutex
 * - modify_cond
 * 
 * @param   this  The client information
 * @return        Zero on success, -1 on error
 */
int client_initialise_threading(client_t* restrict this)
{
  /* Store the thread so that other threads can kill it. */
  this->thread = pthread_self();
  
  /* Create mutex to make sure two thread to not try to send
     messages concurrently, and other client local actions. */
  if ((errno = pthread_mutex_init(&(this->mutex), NULL)))         return -1;
  this->mutex_created = 1;
  
  /* Create mutex and codition for multicast interception replies. */
  if ((errno = pthread_mutex_init(&(this->modify_mutex), NULL)))  return -1;
  this->modify_mutex_created = 1;
  if ((errno = pthread_cond_init(&(this->modify_cond), NULL)))    return -1;
  this->modify_cond_created = 1;
  
  return 0;
}


/**
 * Release all resources assoicated with a client
 * 
 * @param  this  The client information
 */
void client_destroy(client_t* restrict this)
{
  if (this->interception_conditions != NULL)
    {
      size_t i;
      for (i = 0; i < this->interception_conditions_count; i++)
	free(this->interception_conditions[i].condition);
      free(this->interception_conditions);
    }
  if (this->mutex_created)
    pthread_mutex_destroy(&(this->mutex));
  mds_message_destroy(&(this->message));
  if (this->multicasts != NULL)
    {
      size_t i;
      for (i = 0; i < this->multicasts_count; i++)
	multicast_destroy(this->multicasts + i);
      free(this->multicasts);
    }
  free(this->send_pending);
  if (this->modify_message != NULL)
    {
      mds_message_destroy(this->modify_message);
      free(this->modify_message);
    }
  if (this->modify_mutex_created)
    pthread_mutex_destroy(&(this->modify_mutex));
  if (this->modify_cond_created)
    pthread_cond_destroy(&(this->modify_cond));
  free(this);
}


/**
 * Calculate the buffer size need to marshal client information
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t client_marshal_size(const client_t* restrict this)
{
  size_t i, n = sizeof(ssize_t) + 3 * sizeof(int) + sizeof(uint64_t) + 5 * sizeof(size_t);
  
  n += mds_message_marshal_size(&(this->message));
  for (i = 0; i < this->interception_conditions_count; i++)
    n += interception_condition_marshal_size(this->interception_conditions + i);
  for (i = 0; i < this->multicasts_count; i++)
    n += multicast_marshal_size(this->multicasts + i);
  n += this->send_pending_size * sizeof(char);
  n += this->modify_message == NULL ? 0 : mds_message_marshal_size(this->modify_message);
  
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
  buf_set_next(data, int, CLIENT_T_VERSION);
  buf_set_next(data, ssize_t, this->list_entry);
  buf_set_next(data, int, this->socket_fd);
  buf_set_next(data, int, this->open);
  buf_set_next(data, uint64_t, this->id);
  n = mds_message_marshal_size(&(this->message));
  buf_set_next(data, size_t, n);
  if (n > 0)
    mds_message_marshal(&(this->message), data);
  data += n / sizeof(char);
  buf_set_next(data, size_t, this->interception_conditions_count);
  for (i = 0; i < this->interception_conditions_count; i++)
    data += n = interception_condition_marshal(this->interception_conditions + i, data) / sizeof(char);
  buf_set_next(data, size_t, this->multicasts_count);
  for (i = 0; i < this->multicasts_count; i++)
    data += multicast_marshal(this->multicasts + i, data) / sizeof(char);
  buf_set_next(data, size_t, this->send_pending_size);
  if (this->send_pending_size > 0)
    memcpy(data, this->send_pending, this->send_pending_size * sizeof(char));
  data += this->send_pending_size;
  n = this->modify_message == NULL ? 0 : mds_message_marshal_size(this->modify_message);
  buf_set_next(data, size_t, n);
  if (this->modify_message != NULL)
    mds_message_marshal(this->modify_message, data);
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
  size_t i, n, rc = sizeof(ssize_t) + 3 * sizeof(int) + sizeof(uint64_t) + 5 * sizeof(size_t);
  this->interception_conditions = NULL;
  this->multicasts = NULL;
  this->send_pending = NULL;
  this->mutex_created = 0;
  this->modify_mutex_created = 0;
  this->modify_cond_created = 0;
  this->multicasts_count = 0;
  /* buf_get_next(data, int, CLIENT_T_VERSION); */
  buf_next(data, int, 1);
  buf_get_next(data, ssize_t, this->list_entry);
  buf_get_next(data, int, this->socket_fd);
  buf_get_next(data, int, this->open);
  buf_get_next(data, uint64_t, this->id);
  buf_get_next(data, size_t, n);
  if (n > 0)
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
  buf_get_next(data, size_t, n);
  if (xmalloc(this->multicasts, n, multicast_t))
    goto fail;
  for (i = 0; i < n; i++, this->multicasts_count++)
    {
      size_t m = multicast_unmarshal(this->multicasts + i, data);
      if (m == 0)
	goto fail;
      data += m / sizeof(char);
      rc += m;
    }
  buf_get_next(data, size_t, this->send_pending_size);
  if (this->send_pending_size > 0)
    {
      if (xmalloc(this->send_pending, this->send_pending_size, char))
	goto fail;
      memcpy(this->send_pending, data, this->send_pending_size * sizeof(char));
      data += this->send_pending_size;
      rc += this->send_pending_size * sizeof(char);
    }
  buf_get_next(data, size_t, n);
  if (n > 0)
    mds_message_unmarshal(this->modify_message, data);
  else
    this->modify_message = NULL;
  rc += n * sizeof(char);
  return rc;
  
 fail:
  mds_message_destroy(&(this->message));
  for (i = 0; i < this->interception_conditions_count; i++)
    free(this->interception_conditions[i].condition);
  free(this->interception_conditions);
  for (i = 0; i < this->multicasts_count; i++)
    multicast_destroy(this->multicasts + i);
  free(this->multicasts);
  free(this->send_pending);
  if (this->modify_message != NULL)
    {
      mds_message_destroy(this->modify_message);
      free(this->modify_message);
    }
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
  size_t n, c, rc = sizeof(ssize_t) + 3 * sizeof(int) + sizeof(uint64_t) + 5 * sizeof(size_t);
  buf_next(data, int, 1);
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
  buf_get_next(data, size_t, c);
  while (c--)
    {
      n = multicast_unmarshal_skip(data);
      data += n / sizeof(char);
      rc += n;
    }
  buf_get_next(data, size_t, n);
  data += n;
  rc += n * sizeof(char);
  buf_get_next(data, size_t, n);
  rc += n * sizeof(char);
  return rc;
}

