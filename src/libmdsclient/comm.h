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
#ifndef MDS_LIBMDSCLIENT_COMM_H
#define MDS_LIBMDSCLIENT_COMM_H


#include "address.h"

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>



/**
 * A connection to the display server
 */
typedef struct libmds_connection
{
  /**
   * The file descriptor of the socket
   * connected to the display server,
   * -1 if not connected
   */
  int socket_fd;
  
  /**
   * The ID of the _previous_ message
   */
  uint32_t message_id;
  
  /**
   * The client ID, `NULL` if anonymous
   */
  char* client_id;
  
  /**
   * Mutex used to hinder concurrent modification
   * and concurrent message passing
   * 
   * This mutex is a fast mutex, a thread may not
   * lock it more than once
   */
  pthread_mutex_t mutex;
  
  /**
   * Whether `mutex` is initialised
   */
  int mutex_initialised;
  
} libmds_connection_t;


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
__attribute__((nonnull))
int libmds_connection_initialise(libmds_connection_t* restrict this);

/**
 * Allocate and initialise a connection descriptor
 * 
 * @return  The connection descriptor, `NULL` on error,
 *          `errno` will have been set accordingly on error
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 * @throws  EAGAIN  See pthread_mutex_init(3)
 * @throws  EPERM   See pthread_mutex_init(3)
 */
libmds_connection_t* libmds_connection_create(void);

/**
 * Release all resources held by a connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_destroy(libmds_connection_t* restrict this);

/**
 * Release all resources held by a connection descriptor,
 * and release the allocation of the connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_free(libmds_connection_t* restrict this);

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
 *                        `libmds_parse_display_adress` can be used to
 *                        figure out what is wrong.
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 * @throws                Any error specified for socket(2)
 * @throws                Any error specified for connect(2), except EINTR
 */
__attribute__((nonnull))
int libmds_connection_establish(libmds_connection_t* restrict this, const char** restrict display);

/**
 * Connect to the display server
 * 
 * @param   this     The connection descriptor, must not be `NULL`
 * @param   address  The address to connect to, must not be `NULL`,
 *                   and must be the result of a successful call to
 *                   `libmds_parse_display_adress`
 * @return           Zero on success, -1 on error. On error, `display`
 *                   will point to `NULL` if MDS_DISPLAY is not defiend,
 *                   otherwise, `errno` will have been set to describe
 *                   the error.
 * 
 * @throws  Any error specified for socket(2)
 * @throws  Any error specified for connect(2), except EINTR
 */
__attribute__((nonnull))
int libmds_connection_establish_address(libmds_connection_t* restrict this,
					const libmds_display_address_t* restrict address);

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
__attribute__((nonnull))
size_t libmds_connection_send(libmds_connection_t* restrict this, const char* restrict message, size_t length);

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
__attribute__((nonnull))
size_t libmds_connection_send_unlocked(libmds_connection_t* restrict this, const char* restrict message,
				       size_t length, int continue_on_interrupt);

/**
 * Lock the connection descriptor for being modified,
 * or used to send data to the display, by another thread
 * 
 * @param   this:libmds_connection_t*  The connection descriptor, must not be `NULL`
 * @return  :int                       Zero on success, -1 on error, `errno`
 *                                     will have been set accordingly on error
 * 
 * @throws  See pthread_mutex_lock(3)
 */
#define libmds_connection_lock(this) \
  (errno = pthread_mutex_lock(&((this)->mutex)), (errno ? 0 : -1))

/**
 * Lock the connection descriptor for being modified,
 * or used to send data to the display, by another thread
 * 
 * @param   this:libmds_connection_t*  The connection descriptor, must not be `NULL`
 * @return  :int                       Zero on success, -1 on error, `errno`
 *                                     will have been set accordingly on error
 * 
 * @throws  See pthread_mutex_trylock(3)
 */
#define libmds_connection_trylock(this) \
  (errno = pthread_mutex_trylock(&((this)->mutex)), (errno ? 0 : -1))

/**
 * Lock the connection descriptor for being modified,
 * or used to send data to the display, by another thread
 * 
 * @param   this:libmds_connection_t*                 The connection descriptor, must not be `NULL`
 * @param   deadline:const struct timespec *restrict  The absolute `CLOCK_REALTIME` time when the
 *                                                    function shall fail, must not be `NULL`
 * @return  :int                                      Zero on success, -1 on error, `errno`
 *                                                    will have been set accordingly on error
 * 
 * @throws  See pthread_mutex_timedlock(3)
 */
#define libmds_connection_timedlock(this, deadline)  \
  (errno = pthread_mutex_timedlock(&((this)->mutex), deadline), (errno ? 0 : -1))

/**
 * Undo the action of `libmds_connection_lock`, `libmds_connection_trylock`
 * or `libmds_connection_timedlock`
 * 
 * @param   this:libmds_connection_t*  The connection descriptor, must not be `NULL`
 * @return  :int                       Zero on success, -1 on error, `errno`
 *                                     will have been set accordingly on error
 * 
 * @throws  See pthread_mutex_unlock(3)
 */
#define libmds_connection_unlock(this)  \
  (errno = pthread_mutex_unlock(&((this)->mutex)), (errno ? 0 : -1))

/**
 * Arguments for `libmds_compose` to compose the `Client ID`-header
 * 
 * @param  this: libmds_connection_t*  The connection descriptor, must not be `NULL`
 */
#define LIBMDS_HEADER_CLIENT_ID(this)  \
  "?Client ID: %s", (this)->client_id != NULL, (this)->client_id

/**
 * Arguments for `libmds_compose` to compose the `Message ID`-header
 * 
 * @param  this: libmds_connection_t*  The connection descriptor, must not be `NULL`
 */
#define LIBMDS_HEADER_MESSAGE_ID(this)  \
  "Message ID: %"PRIu32, (this)->message_id

/**
 * Arguments for `libmds_compose` to compose the standard headers:
 * `Client ID` and `Message ID`
 * 
 * @param  this: libmds_connection_t*  The connection descriptor, must not be `NULL`
 */
#define LIBMDS_HEADERS_STANDARD(this)  \
  LIBMDS_HEADER_CLIENT_ID(this), LIBMDS_HEADER_MESSAGE_ID(this)


#endif

