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
#include "comm.h"

#include <stdlib.h>
#include <unistd.h>



/**
 * Initialise a connection descriptor
 * 
 * @param  this  The connection descriptor
 * @return        Zero on success, -1 on error, `ernno`
 *                will have been set accordingly on error
 */
int libmds_connection_initialise(libmds_connection_t* restrict this)
{
  this->socket_fd = -1;
  this->message_id = UINT32_MAX;
  this->client_id = NULL;
  this->mutex_initialised = 0;
  errno = pthread_mutex_init(&(this->mutex), NULL);
  if (errno)
    return -1;
  this->mutex_initialised = 1;
  return 0;
}


/**
 * Allocate and initialise a connection descriptor
 * 
 * @return  The connection descriptor, `NULL` on error,
 *          `errno` will have been set accordingly on error
 * 
 * @throws  ENOMEM  Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
libmds_connection_t* libmds_connection_create(void)
{
  libmds_connection_t* rc = malloc(sizeof(libmds_connection_t));
  if (rc == NULL)
    return NULL;
  libmds_connection_initialise(rc);
  return rc;
}


/**
 * Release all resources held by a connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_destroy(libmds_connection_t* restrict this)
{
  if (this == NULL)
    return;
  
  if (this->socket_fd >= 0)
    {
      while (close(this->socket_fd))
	{
	  if (errno == EINTR)
	    continue;
	  break; /* errno may be EBADF or EIO. */
	}
      this->socket_fd = -1;
    }
  
  free(this->client_id);
  this->client_id = NULL;
  
  if (this->mutex_initialised)
    {
      this->mutex_initialised = 0;
      pthread_mutex_destroy(&(this->mutex)); /* Can return EBUSY. */
    }
}


/**
 * Release all resources held by a connection descriptor,
 * and release the allocation of the connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_free(libmds_connection_t* restrict this)
{
  libmds_connection_destroy(this);
  free(this);
}


int libmds_connection_send(libmds_connection_t* restrict this, const char* message, size_t length)
{
  int r, saved_errno;
  
  if (libmds_connection_lock(this))
    return -1;
  
  r = libmds_connection_send_unlocked(this, message, length);
  
  saved_errno = errno;
  (void) libmds_connection_unlock(this);
  return errno = saved_errno, r;
}


int libmds_connection_send_unlocked(libmds_connection_t* restrict this, const char* message, size_t length)
{
  return (void) this, (void) message, (void) length, 0; /* TODO */
}

