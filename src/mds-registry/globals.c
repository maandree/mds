/**
 * mds — A micro-display server
 * Copyright © 2014  Mattias Andrée (maandree@member.fsf.org)
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
 * Value of the ‘Message ID’ header for the next message
 */
uint32_t message_id = 2;

/**
 * Buffer for received messages
 */
mds_message_t received;

/**
 * Whether the server is connected to the display
 */
int connected = 1;

/**
 * Protocol registry table
 */
hash_table_t reg_table;

/**
 * Reusable buffer for data to send
 */
char* send_buffer = NULL;

/**
 * The size of `send_buffer`
 */
size_t send_buffer_size = 0;

/**
 * Used to temporarily store the old value when reallocating heap-allocations
 */
char* old;

/**
 * The master thread
 */
pthread_t master_thread;

/**
 * The number of running slaves
 */
size_t running_slaves = 0;

/**
 * List of running slaves
 */
linked_list_t slave_list;

/**
 * Mutex for slave data
 */
pthread_mutex_t slave_mutex;

/**
 * Condition for slave data
 */
pthread_cond_t slave_cond;

