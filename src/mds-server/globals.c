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
#include "globals.h"


/**
 * The program run state, 1 when running, 0 when shutting down
 */
volatile sig_atomic_t running = 1;


/**
 * The number of running slaves
 */
size_t running_slaves = 0;

/**
 * Mutex for slave data
 */
pthread_mutex_t slave_mutex;

/**
 * Condition for slave data
 */
pthread_cond_t slave_cond;

/**
 * Map from client socket file descriptor to all information (`client_t`)
 */
fd_table_t client_map;

/**
 * List of client information (`client_t`)
 */
linked_list_t client_list;

/**
 * The next free ID for a client
 */
uint64_t next_client_id = 1;

/**
 * The next free ID for a message modifications
 */
uint64_t next_modify_id = 1;

/**
 * Mutex for message modification
 */
pthread_mutex_t modify_mutex;

/**
 * Condition for message modification
 */
pthread_cond_t modify_cond;

/**
 * Map from modification ID to waiting client
 */
hash_table_t modify_map;
