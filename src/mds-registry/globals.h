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
#ifndef MDS_MDS_REGISTRY_GLOBALS_H
#define MDS_MDS_REGISTRY_GLOBALS_H


#include <libmdsserver/mds-message.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/linked-list.h>

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>


#define MDS_REGISTRY_VARS_VERSION  0



/**
 * Value of the ‘Message ID’ header for the next message
 */
extern uint32_t message_id;

/**
 * Buffer for received messages
 */
extern mds_message_t received;

/**
 * Whether the server is connected to the display
 */
extern int connected;

/**
 * Protocol registry table
 */
extern hash_table_t reg_table;

/**
 * Reusable buffer for data to send
 */
extern char* send_buffer;

/**
 * The size of `send_buffer`
 */
extern size_t send_buffer_size;

/**
 * Used to temporarily store the old value when reallocating heap-allocations
 */
extern char* old;

/**
 * The number of running slaves
 */
extern size_t running_slaves;

/**
 * List of running slaves
 */
extern linked_list_t slave_list;

/**
 * Mutex for slave data
 */
extern pthread_mutex_t slave_mutex;

/**
 * Condition for slave data
 */
extern pthread_cond_t slave_cond;


#endif

