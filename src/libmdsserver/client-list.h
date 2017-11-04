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
#ifndef MDS_LIBMDSSERVER_CLIENT_LIST_H
#define MDS_LIBMDSSERVER_CLIENT_LIST_H


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>



#define CLIENT_LIST_T_VERSION 0

/**
 * Dynamic array of client ID:s
 */
typedef struct client_list
{
	/**
	 * The size of the array
	 */
	size_t capacity;

	/**
	 * The index after the last used index
	 */
	size_t size;

	/**
	 * Stored client ID:s
	 */
	uint64_t *clients;

} client_list_t;



/**
 * Create a client list
 * 
 * @param   this      Memory slot in which to store the new client list
 * @param   capacity  The minimum initial capacity of the client list, 0 for default
 * @return            Non-zero on error, `errno` will have been set accordingly
 */
__attribute__((nonnull))
int client_list_create(client_list_t *restrict this, size_t capacity);

/**
 * Release all resources in a client list, should
 * be done even if `client_list_create` fails
 * 
 * @param  this  The client list
 */
__attribute__((nonnull))
void client_list_destroy(client_list_t *restrict this);

/**
 * Clone a client list
 * 
 * @param   this  The client list to clone
 * @param   out   Memory slot in which to store the new client list
 * @return        Non-zero on error, `errno` will have been set accordingly
 */
__attribute__((nonnull))
int client_list_clone(const client_list_t *restrict this, client_list_t *restrict out);

/**
 * Add a client to the list
 * 
 * @param   this    The list
 * @param   client  The client to add
 * @return          Non-zero on error, `errno` will be set accordingly
 */
__attribute__((nonnull))
int client_list_add(client_list_t *restrict this, uint64_t client);

/**
 * Remove a client from the list, once
 * 
 * @param  this    The list
 * @param  client  The client to remove
 */
__attribute__((nonnull))
void client_list_remove(client_list_t *restrict this, uint64_t client);

/**
 * Calculate the buffer size need to marshal a client list
 * 
 * @param   this  The list
 * @return        The number of bytes to allocate to the output buffer
 */
__attribute__((pure, nonnull))
size_t client_list_marshal_size(const client_list_t *restrict this);

/**
 * Marshals a client list
 * 
 * @param  this  The list
 * @param  data  Output buffer for the marshalled data
 */
__attribute__((nonnull))
void client_list_marshal(const client_list_t *restrict this, char *restrict data);

/**
 * Unmarshals a client list
 * 
 * @param   this  Memory slot in which to store the new client list
 * @param   data  In buffer with the marshalled data
 * @return        Non-zero on error, `errno` will be set accordingly.
 *                Destroy the list on error.
 */
__attribute__((nonnull))
int client_list_unmarshal(client_list_t *restrict this, char *restrict data);


#endif
