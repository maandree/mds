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
#ifndef MDS_MDS_SERVER_GLOBALS_H
#define MDS_MDS_SERVER_GLOBALS_H

#include "../mds-base.h" /* Include here so other do not need to. */


#include <libmdsserver/linked-list.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/fd-table.h>

#include <signal.h>
#include <pthread.h>
#include <stdint.h>



#define MDS_SERVER_VARS_VERSION 0



/**
 * The program run state, 1 when running, 0 when shutting down
 */
extern volatile sig_atomic_t running;


/**
 * The number of running slaves
 */
extern size_t running_slaves;

/**
 * Mutex for slave data
 */
extern pthread_mutex_t slave_mutex;

/**
 * Condition for slave data
 */
extern pthread_cond_t slave_cond;

/**
 * Map from client socket file descriptor to all information (`client_t`)
 */
extern fd_table_t client_map;

/**
 * List of client information (`client_t`)
 */
extern linked_list_t client_list;

/**
 * The next free ID for a client
 */
extern uint64_t next_client_id;

/**
 * The next free ID for a message modifications
 */
extern uint64_t next_modify_id;

/**
 * Mutex for message modification
 */
extern pthread_mutex_t modify_mutex;

/**
 * Condition for message modification
 */
extern pthread_cond_t modify_cond;

/**
 * Map from modification ID to waiting client
 */
extern hash_table_t modify_map;


#endif
