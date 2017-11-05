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
#ifndef MDS_MDS_REGISTRY_SLAVE_H
#define MDS_MDS_REGISTRY_SLAVE_H


#include <libmdsserver/hash-table.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>



#define SLAVE_T_VERSION 0

/**
 * Slave information, a thread waiting for protocols to become available
 */
typedef struct slave {
	/**
	 * Set of protocols for which to wait that they become available
	 */
	hash_table_t *wait_set;

	/**
	 * The ID of the waiting client
	 */
	uint64_t client;

	/**
	 * The ID of the waiting client
	 */
	char *client_id;

	/**
	 * The ID of the message that triggered the waiting
	 */
	char *message_id;

	/**
	 * The slave's node in the linked list of slaves
	 */
	ssize_t node;

	/**
	 * Whether the client has been closed
	 */
	volatile sig_atomic_t closed;

	/**
	 * The slave thread
	 */
	pthread_t thread;

	/**
	 * The time slave should die if its condition
	 * has not be meet at that time
	 */
	struct timespec dethklok;

	/**
	 * Whether `dethklok` should apply
	 */
	int timed;
} slave_t;



/**
 * Start a slave thread with an already created slave
 * 
 * @param   slave  The slave
 * @return         Non-zero on error, `errno` will be set accordingly
 */
__attribute__((nonnull))
int start_created_slave(slave_t *restrict slave);

/**
 * Start a slave thread
 * 
 * @param   wait_set         Set of protocols for which to wait that they become available
 * @param   recv_client_id   The ID of the waiting client
 * @param   recv_message_id  The ID of the message that triggered the waiting
 * @return                   Non-zero on error
 */
__attribute__((nonnull))
int start_slave(hash_table_t *restrict wait_set, const char *restrict recv_client_id,
                const char *restrict recv_message_id);

/**
 * Close all slaves associated with a client
 * 
 * @param  client  The client's ID
 */
void close_slaves(uint64_t client);

/**
 * Notify slaves that a protocol has become available
 * 
 * @param   command  The protocol
 * @return           Non-zero on error, `ernno`will be set accordingly
 */
__attribute__((nonnull))
int advance_slaves(char *command);

/**
 * Create a slave
 * 
 * @param   wait_set         Set of protocols for which to wait that they become available
 * @param   recv_client_id   The ID of the waiting client
 * @param   recv_message_id  The ID of the message that triggered the waiting
 * @return                   The slave, `NULL` on error, `errno` will be set accordingly
 */
__attribute__((nonnull))
slave_t *slave_create(hash_table_t *restrict wait_set, const char *restrict recv_client_id,
                      const char *restrict recv_message_id);


/**
 * Initialise a slave
 * 
 * @param  this  Memory slot in which to store the new slave information
 */
__attribute__((nonnull))
void slave_initialise(slave_t *restrict this);

/**
 * Release all resources assoicated with a slave
 * 
 * @param  this  The slave information
 */
void slave_destroy(slave_t *restrict this);

/**
 * Calculate the buffer size need to marshal slave information
 * 
 * @param   this  The slave information
 * @return        The number of bytes to allocate to the output buffer
 */
__attribute__((pure, nonnull))
size_t slave_marshal_size(const slave_t *restrict this);

/**
 * Marshals slave information
 * 
 * @param   this  The slave information
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
__attribute__((nonnull))
size_t slave_marshal(const slave_t *restrict this, char *restrict data);

/**
 * Unmarshals slave information
 * 
 * @param   this  Memory slot in which to store the new slave information
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, `errno` will be set accordingly, otherwise the
 *                number of read bytes. Destroy the slave information on error.
 */
__attribute__((nonnull))
size_t slave_unmarshal(slave_t *restrict this, char *restrict data);

/**
 * Pretend to unmarshal slave information
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
__attribute__((pure, nonnull))
size_t slave_unmarshal_skip(char *restrict data);


#endif
