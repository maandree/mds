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
#ifndef MDS_LIBMDSCLIENT_INBOUND_H
#define MDS_LIBMDSCLIENT_INBOUND_H
/* This module is eerily similar to <libmdsserver/mds-message.h>,
 * somethings have been removed, some things have been added. */


#include <stddef.h>
#include <semaphore.h>



/**
 * Message passed between a server and a client or between two of either
 */
typedef struct libmds_message
{
  /**
   * The headers in the message, each element in this list
   * as an unparsed header, it consists of both the header
   * name and its associated value, joined by ": ". A header
   * cannot be `NULL` (unless its memory allocation failed,)
   * but `headers` itself is `NULL` if there are no headers.
   * The "Length" header should be included in this list.
   */
  char** headers;
  
  /**
   * The number of headers in the message
   */
  size_t header_count;
  
  /**
   * The payload of the message, `NULL` if none (of zero-length)
   */
  char* payload;
  
  /**
   * The size of the payload
   */
  size_t payload_size;
  
  /**
   * Internal buffer for the reading function (internal data)
   */
  char* buffer;
  
  /**
   * The size allocated to `buffer` (internal data)
   */
  size_t buffer_size;
  
  /**
   * The number of bytes used in `buffer` (internal data)
   */
  size_t buffer_ptr;
  
  /**
   * The number of bytes read from `buffer` (internal data)
   */
  size_t buffer_off;
  
  /**
   * Zero unless the structure is flattend, otherwise
   * the size of the object (semiinternal data)
   * 
   * Flattened means that all pointers are subpointers
   * of the object itself
   */
  size_t flattened;
  
  /**
   * 0 while reading headers, 1 while reading payload, and 2 when done (internal data)
   */
  int stage;
  
} libmds_message_t;


/**
 * Queue of spooled messages
 */
typedef struct libmds_mspool
{
  /**
   * Array of messages
   */
  libmds_message_t** messages;
  
  /**
   * The number of elements the current
   * allocation of `messages` can hold
   */
  size_t size;
  
  /**
   * Push end
   */
  size_t head;
  
  /**
   * Poll end
   */
  size_t tail;
  
  /**
   * The total size of all spooled messages
   */
  size_t spooled_bytes;
  
  /**
   * Do not spool additional messages if
   * `spooled_bytes` is equal to or exceeds
   * to value
   */
  size_t spool_limit_bytes;
  
  /**
   * Do not spool more than this amount
   * of messages
   */
  size_t spool_limit_messages;
  
  /* POSIX semaphores are lighter weight than XSI semaphores, so we use POSIX here. */
  
  /**
   * Binary semaphore used to lock the spool whilst
   * manipulating it
   */
  sem_t lock;
  
  /**
   * Seamphore used to signal addition of messages,
   * each time a message is spooled, this semaphore
   * in increased, the polling thread decreases the
   * the semaphore before despooling a message,
   * causing it to block when the spool is empty
   */
  sem_t semaphore;
  
} libmds_mspool_t;


/**
 * Message pool for reusable message allocations
 */
typedef struct libmds_mpool
{
  /**
   * Array of messages
   */
  libmds_message_t** messages;
  
  /**
   * The number of elements the current
   * allocation of 'messages` can hold
   */
  size_t size;
  
  /**
   * The tip of the stack
   */
  size_t tip;
  
  /**
   * Binary semaphore used to lock the pool
   * whilst manipulating it
   */
  sem_t lock;
  
} libmds_mpool_t;



/**
 * Initialise a message slot so that it can
 * be used by `libmds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, `errno` will be set accordingly.
 *                Destroy the message on error.
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
int libmds_message_initialise(libmds_message_t* restrict this);

/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
__attribute__((nonnull))
void libmds_message_destroy(libmds_message_t* restrict this);

/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param   this  The message
 * @return        The duplicate, you do not need to call `libmds_message_destroy`
 *                on it before you call `free` on it. However, you cannot use
 *                this is an `libmds_message_t` array (libmds_message_t*), only
 *                in an `libmds_message_t*` array (libmds_message_t**).
 */
__attribute__((nonnull, malloc, warn_unused_result))
libmds_message_t* libmds_message_duplicate(libmds_message_t* restrict this);

/**
 * Read the next message from a file descriptor
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   fd    The file descriptor
 * @return        Non-zero on error or interruption, `errno` will be
 *                set accordingly. Destroy the message on error,
 *                be aware that the reading could have been
 *                interrupted by a signal rather than canonical error.
 *                If -2 is returned `errno` will not have been set,
 *                -2 indicates that the message is malformated,
 *                which is a state that cannot be recovered from.
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 * @throws          Any error specified for recv(3)
 */
__attribute__((nonnull, warn_unused_result))
int libmds_message_read(libmds_message_t* restrict this, int fd);



#endif

