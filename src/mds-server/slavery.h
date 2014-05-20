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
#ifndef MDS_MDS_SERVER_SLAVERY_H
#define MDS_MDS_SERVER_SLAVERY_H


#include "client.h"

#include <pthread.h>


/**
 * Receive a full message and update open status if the client closes
 * 
 * @param   client  The client
 * @return          Zero on success, -2 on failure, otherwise -1
 */
int fetch_message(client_t* client);

/**
 * Create, start and detache a slave thread
 * 
 * @param   thread    The address at where to store the thread
 * @param   slave_fd  The file descriptor of the slave's socket
 * @return            Zero on success, -1 on error, error message will have been printed
 */
int create_slave(pthread_t* thread_slot, int slave_fd);

/**
 * Initialise a client, except for threading
 * 
 * @param   client_fd  The file descriptor of the client's socket
 * @return             The client information, NULL on error
 */
client_t* initialise_client(int client_fd);


#endif

