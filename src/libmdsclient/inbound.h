/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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
	 * cannot be `NULL` but `headers` itself is `NULL` if there
	 * are no headers. The "Length" header is included in this list.
	 */
	char **headers;

	/**
	 * The number of headers in the message
	 */
	size_t header_count;

	/**
	 * The payload of the message, `NULL` if none (of zero-length)
	 */
	char *payload;

	/**
	 * The size of the payload
	 */
	size_t payload_size;

	/**
	 * Internal buffer for the reading function (internal data)
	 */
	char *buffer;

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
	 * the size of the object (internal data)
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
	 * Array of messages (internal data)
	 */
	libmds_message_t **messages;

	/**
	 * The number of elements the current
	 * allocation of `messages` can hold (internal data)
	 */
	size_t size;

	/**
	 * Push end (internal data)
	 */
	size_t head;

	/**
	 * Poll end (internal data)
	 */
	size_t tail;

	/**
	 * The total size of all spooled messages
	 * (internal data)
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

	/**
	 * If non-zero, `wait_semaphore` shall
	 * be incremented when a message is
	 * polled (internal data)
	 */
	int please_post;

	/* POSIX semaphores are lighter weight than XSI semaphores, so we use POSIX here. */

	/**
	 * Binary semaphore used to lock the spool whilst
	 * manipulating it (internal data)
	 */
	sem_t lock;

	/**
	 * Semaphore used to signal addition of messages.
	 * Each time a message is spooled, this semaphore
	 * is increased, the polling thread decreases the
	 * semaphore before despooling a message, causing
	 * it to block when the spool is empty
	 * (internal data)
	 */
	sem_t semaphore;

	/**
	 * If the spool is full, the semaphore is acquired
	 * so that the spool function blocks, it is then
	 * posted when a message is polled so that the
	 * spool function may try again (internal data)
	 */
	sem_t wait_semaphore;

} libmds_mspool_t;


/**
 * Message pool (stack) for reusable message allocations
 */
typedef struct libmds_mpool
{
	/**
	 * Array of messages (internal data)
	 */
	libmds_message_t **messages;

	/**
	 * The number of elements the current
	 * allocation of 'messages` can hold (internal data)
	 */
	size_t size;

	/**
	 * The tip of the stack (internal data)
	 */
	volatile size_t tip;

	/**
	 * Binary semaphore used to lock the pool
	 * whilst manipulating it (internal data)
	 */
	sem_t lock;

} libmds_mpool_t;



/**
 * Initialise a message slot so that it can
 * be used by `libmds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Zero on success, -1 error, `errno` will be set
 *                accordingly
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
int libmds_message_initialise(libmds_message_t *restrict this);

/**
 * Release all resources in a message
 * 
 * @param  this  The message
 */
__attribute__((nonnull))
void libmds_message_destroy(libmds_message_t *restrict this);

/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param   this  The message
 * @param   pool  Message allocation pool, may be `NULL`
 * @return        The duplicate, you do not need to call `libmds_message_destroy`
 *                on it before you call `free` on it. However, you cannot use
 *                this is an `libmds_message_t` array (libmds_message_t*), only
 *                in an `libmds_message_t*` array (libmds_message_t**).
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull(1), malloc, warn_unused_result))
libmds_message_t *libmds_message_duplicate(libmds_message_t *restrict this, libmds_mpool_t *restrict pool);

/**
 * Read the next message from a file descriptor
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   fd    The file descriptor
 * @return        Zero on success, -1 on error or interruption, `errno`
 *                will be set accordingly. Destroy the message on error,
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
int libmds_message_read(libmds_message_t *restrict this, int fd);



/**
 * Initialise a message spool
 * 
 * @param   this  The message spool
 * @return        Zero on success, -1 on error, `errno` will be set accordingly
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
int libmds_mspool_initialise(libmds_mspool_t *restrict this);

/**
 * Destroy a message spool, deallocate its resources
 * 
 * @param  this  The message spool
 */
__attribute__((nonnull))
void libmds_mspool_destroy(libmds_mspool_t *restrict this);

/**
 * Spool a message
 * 
 * @param   this     The message spool
 * @param   message  The message to spool, must be flat (created with `libmds_message_duplicate`)
 * @return           Zero on success, -1 on error, `errno` will be set accordingly
 * 
 * @throws  EINTR   If interrupted
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
int libmds_mspool_spool(libmds_mspool_t *restrict this, libmds_message_t *restrict message);

/**
 * Poll a message from a spool, wait if empty
 * 
 * @param   this  The message spool
 * @return        A spooled message, `NULL`on error, `errno` will be set accordingly
 * 
 * @throws  EINTR  If interrupted
 */
__attribute__((nonnull, warn_unused_result, malloc))
libmds_message_t *libmds_mspool_poll(libmds_mspool_t *restrict this);

/**
 * Poll a message from a spool, wait for a limited time
 * or return unsuccessfully if empty
 * 
 * @param   this      The message spool
 * @param   deadline  The CLOCK_REALTIME time the function must return,
 *                    `NULL` to return immediately if it would block
 * @return            A spooled message, `NULL`on error, `errno` will be set accordingly
 * 
 * @throws  EINTR      If interrupted
 * @throws  EAGAIN     If `deadline` is `NULL` and the spool is empty
 * @throws  EINVAL     If `deadline->tv_nsecs` is outside [0, 1 milliard[
 * @throws  ETIMEDOUT  If the time specified `deadline` passed and the spool was till empty
 */
__attribute__((nonnull(1), warn_unused_result, malloc))
libmds_message_t *libmds_mspool_poll_try(libmds_mspool_t *restrict this,
                                         const struct timespec *restrict deadline);



/**
 * Initialise a pool of reusable message allocations
 * 
 * @param   this  The message allocation pool
 * @param   size  The number of allocations that may be pooled
 * @return        Zero on success, -1 on error, `errno` will be set accordingly
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
int libmds_mpool_initialise(libmds_mpool_t *restrict this, size_t size);

/**
 * Destroy a pool of reusable message allocations,
 * deallocate its resources and pooled allocations
 * 
 * @param  this  The message allocation pool
 */
__attribute__((nonnull))
void libmds_mpool_destroy(libmds_mpool_t *restrict this);

/**
 * Add a message allocation to a pool
 * 
 * @param   this     The message allocation pool
 * @param   message  Message allocation to pool, must be flat (created with
 *                   `libmds_message_duplicate` or fetched with `libmds_mspool_poll`
 *                   or `libmds_mspool_poll_try`)
 * @return           Zero on success, -1 on error, `errno` will be set accordingly
 */
__attribute__((nonnull, warn_unused_result))
int libmds_mpool_offer(libmds_mpool_t *restrict this, libmds_message_t *restrict message);

/**
 * Fetch a message allocation from a pool
 * 
 * @param   this  The message allocation pool
 * @return        An offered message allocation, `NULL` on error or if none
 *                are available. If `NULL` is returned, `errno` is set to zero,
 *                if the pool was empty, otherwise `errno` will describe the error.
 */
__attribute__((nonnull, warn_unused_result, malloc))
libmds_message_t *libmds_mpool_poll(libmds_mpool_t *restrict this);



#endif
