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
#ifndef MDS_MDS_SERVER_CLIENT_H
#define MDS_MDS_SERVER_CLIENT_H


#include "interception-condition.h"
#include "multicast.h"

#include <libmdsserver/mds-message.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>



#define CLIENT_T_VERSION 0

/**
 * Client information structure
 */
typedef struct client
{
	/**
	 * The client's entry in the list of clients
	 */
	ssize_t list_entry;

	/**
	 * The socket file descriptor for the socket connected to the client
	 */
	int socket_fd;

	/**
	 * Whether the socket is open
	 */
	int open;

	/**
	 * Message read buffer for the client
	 */
	struct mds_message message;

	/**
	 * The read thread for the client
	 */
	pthread_t thread;

	/**
	 * The client's ID
	 */
	uint64_t id;

	/**
	 * Mutex for sending data and other
	 * actions that only affacts this client
	 */
	pthread_mutex_t mutex;

	/**
	 * Whether `mutex` has been initialised
	 */
	int mutex_created;

	/**
	 * The messages interception conditions conditions
	 * for the client
	 */
	struct interception_condition *interception_conditions;

	/**
	 * The number of interception conditions
	 */
	size_t interception_conditions_count;

	/**
	 * Pending multicast messages
	 */
	struct multicast *multicasts;

	/**
	 * The number of pending multicast messages
	 */
	size_t multicasts_count;

	/**
	 * Messages pending to be sent (concatenated)
	 */
	char *send_pending;

	/**
	 * The character length of the messages pending to be sent
	 */
	size_t send_pending_size;

	/**
	 * Pending reply to the multicast interception
	 */
	struct mds_message *modify_message;

	/**
	 * Mutex for `modify_message` 
	 */
	pthread_mutex_t modify_mutex;

	/**
	 * Condidition for `modify_message` 
	 */
	pthread_cond_t modify_cond;

	/**
	 * Whether `modify_mutex` has been initialised
	 */
	int modify_mutex_created;

	/**
	 * Whether `modify_cond` has been initialised
	 */
	int modify_cond_created;

} client_t;



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
__attribute__((nonnull))
void client_initialise(client_t *restrict this);

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
__attribute__((nonnull))
int client_initialise_threading(client_t *restrict this);

/**
 * Release all resources assoicated with a client
 * 
 * @param  this  The client information
 */
__attribute__((nonnull))
void client_destroy(client_t *restrict this);

/**
 * Calculate the buffer size need to marshal client information
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
__attribute__((pure, nonnull))
size_t client_marshal_size(const client_t *restrict this);

/**
 * Marshals client information
 * 
 * @param   this  The client information
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
__attribute__((nonnull))
size_t client_marshal(const client_t *restrict this, char *restrict data);

/**
 * Unmarshals client information
 * 
 * @param   this  Memory slot in which to store the new client information
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, `errno` will be set accordingly, otherwise the
 *                number of read bytes. Destroy the client information on error.
 */
__attribute__((nonnull))
size_t client_unmarshal(client_t *restrict this, char *restrict data);

/**
 * Pretend to unmarshal client information
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
__attribute__((pure, nonnull))
size_t client_unmarshal_skip(char *restrict data);


#endif
