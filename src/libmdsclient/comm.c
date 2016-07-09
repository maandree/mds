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
#include "comm.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>



#define min(a, b)  ((a) < (b) ? (a) : (b))



/**
 * Initialise a connection descriptor
 * 
 * @param   this  The connection descriptor
 * @return        Zero on success, -1 on error, `ernno`
 *                will have been set accordingly on error
 * 
 * @throws  EAGAIN  See pthread_mutex_init(3)
 * @throws  ENOMEM  See pthread_mutex_init(3)
 * @throws  EPERM   See pthread_mutex_init(3)
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
 * @throws  EAGAIN  See pthread_mutex_init(3)
 * @throws  EPERM   See pthread_mutex_init(3)
 */
libmds_connection_t* libmds_connection_create(void)
{
  libmds_connection_t* rc = malloc(sizeof(libmds_connection_t));
  if (rc == NULL)
    return NULL;
  return libmds_connection_initialise(rc) ? NULL : rc;
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
      close(this->socket_fd); /* TODO Linux closes the filedescriptor on EINTR, but POSIX does not require that. */
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


/**
 * Connect to the display server
 * 
 * @param   this     The connection descriptor, must not be `NULL`
 * @param   display  Pointer to `NULL` to select display be looking at
 *                   the environment. Pointer to a string with the
 *                   address (formatted as the environment variable
 *                   MDS_DISPLAY) if manually specified. The pointer
 *                   itself must not be `NULL`; it will be updated
 *                   with the address if it points to NULL.
 * @return           Zero on success, -1 on error. On error, `display`
 *                   will point to `NULL` if MDS_DISPLAY is not defiend,
 *                   otherwise, `errno` will have been set to describe
 *                   the error.
 * 
 * @throws  EFAULT        If the display server's address is not properly
 *                        formatted, or specifies an unsupported protocol,
 *                        `libmds_parse_display_address` can be used to
 *                        figure out what is wrong.
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 * @throws                Any error specified for socket(2)
 * @throws                Any error specified for connect(2), except EINTR
 */
int libmds_connection_establish(libmds_connection_t* restrict this, const char** restrict display)
{
  libmds_display_address_t addr;
  int saved_errno;
  
  addr.address = NULL;
  
  if (*display == NULL)
    *display = getenv("MDS_DISPLAY");
  
  if ((*display == NULL) || (strchr(*display, ':') == NULL))
    goto efault;
  
  if (libmds_parse_display_address(*display, &addr) < 0)
    goto fail;
  
  if (libmds_connection_establish_address(this, &addr) < 0)
    goto fail;
  
  free(addr.address);
  return 0;
  
 efault:
  free(addr.address);
  return errno = EFAULT, -1;
  
 fail:
  saved_errno = errno;
  free(addr.address);
  return errno = saved_errno, -1;
}


/**
 * Connect to the display server
 * 
 * @param   this     The connection descriptor, must not be `NULL`
 * @param   address  The address to connect to, must not be `NULL`,
 *                   and must be the result of a successful call to
 *                   `libmds_parse_display_address`
 * @return           Zero on success, -1 on error. On error, `display`
 *                   will point to `NULL` if MDS_DISPLAY is not defiend,
 *                   otherwise, `errno` will have been set to describe
 *                   the error.
 * 
 * @throws  EFAULT  `libmds_display_address_t` contains unset parameters.
 * @throws           Any error specified for socket(2)
 * @throws           Any error specified for connect(2), except EINTR
 */
int libmds_connection_establish_address(libmds_connection_t* restrict this,
					const libmds_display_address_t* restrict address)
{
  if (address->domain   < 0)     goto efault;
  if (address->type     < 0)     goto efault;
  if (address->protocol < 0)     goto efault;
  if (address->address == NULL)  goto efault;
  
  this->socket_fd = socket(address->domain, address->type, address->protocol);
  if (this->socket_fd < 0)
    goto fail;
  
  while (connect(this->socket_fd, address->address, address->address_len))
    if (errno != EINTR)
      goto fail;
  
  return 0;
  
 efault:
  errno = EFAULT;
 fail:
  return -1;
}


/**
 * Wrapper for `libmds_connection_send_unlocked` that locks
 * the mutex of the connection
 * 
 * @param   this     The connection descriptor, must not be `NULL`
 * @param   message  The message to send, must not be `NULL`
 * @param   length   The length of the message, should be positive
 * @return           The number of sent bytes. Less than `length` on error,
 *                   `ernno` will have been set accordingly on error
 * 
 * @throws  EACCES        See send(2)
 * @throws  EWOULDBLOCK   See send(2), only if the socket has been modified to nonblocking
 * @throws  EBADF         See send(2)
 * @throws  ECONNRESET    If connection was lost
 * @throws  EDESTADDRREQ  See send(2)
 * @throws  EFAULT        See send(2)
 * @throws  EINVAL        See send(2)
 * @throws  ENOBUFS       See send(2)
 * @throws  ENOMEM        See send(2)
 * @throws  ENOTCONN      See send(2)
 * @throws  ENOTSOCK      See send(2)
 * @throws  EPIPE         See send(2)
 * @throws                See pthread_mutex_lock(3)
 */
size_t libmds_connection_send(libmds_connection_t* restrict this, const char* restrict message, size_t length)
{
  int saved_errno;
  size_t r;
  
  if (libmds_connection_lock(this))
    return 0;
  
  r = libmds_connection_send_unlocked(this, message, length, 1);
  
  saved_errno = errno;
  (void) libmds_connection_unlock(this);
  return errno = saved_errno, r;
}


/**
 * Send a message to the display server, without locking the
 * mutex of the conncetion
 * 
 * @param   this                   The connection descriptor, must not be `NULL`
 * @param   message                The message to send, must not be `NULL`
 * @param   length                 The length of the message, should be positive
 * @param   continue_on_interrupt  Whether to continue sending if interrupted by a signal
 * @return                         The number of sent bytes. Less than `length` on error,
 *                                 `ernno` will have been set accordingly on error
 * 
 * @throws  EACCES        See send(2)
 * @throws  EWOULDBLOCK   See send(2), only if the socket has been modified to nonblocking
 * @throws  EBADF         See send(2)
 * @throws  ECONNRESET    If connection was lost
 * @throws  EDESTADDRREQ  See send(2)
 * @throws  EFAULT        See send(2)
 * @throws  EINTR         If interrupted by a signal, only if `continue_on_interrupt' is zero
 * @throws  EINVAL        See send(2)
 * @throws  ENOBUFS       See send(2)
 * @throws  ENOMEM        See send(2)
 * @throws  ENOTCONN      See send(2)
 * @throws  ENOTSOCK      See send(2)
 * @throws  EPIPE         See send(2)
 */
size_t libmds_connection_send_unlocked(libmds_connection_t* restrict this, const char* restrict message,
				       size_t length, int continue_on_interrupt)
{
  size_t block_size = length;
  size_t sent = 0;
  ssize_t just_sent;
  
  errno = 0;
  while (length > 0)
    if ((just_sent = send(this->socket_fd, message + sent, min(block_size, length), MSG_NOSIGNAL)) < 0)
      {
	if (errno == EMSGSIZE)
	  {
	    block_size >>= 1;
	    if (block_size == 0)
	      return sent;
	  }
	else if ((errno == EINTR) && continue_on_interrupt)
	  continue;
	else
	  return sent;
      }
    else
      {
	sent += (size_t)just_sent;
	length -= (size_t)just_sent;
      }
  
  return sent;
}

