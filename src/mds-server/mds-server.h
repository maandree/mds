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
#ifndef MDS_MDS_SERVER_H
#define MDS_MDS_SERVER_H

#include "client.h"
#include "multicast.h"

#include <stddef.h>


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Outout data
 */
void* slave_loop(void* data);

/**
 * Send the next message in a clients multicast queue
 * 
 * @param  client  The client
 */
void send_multicast_queue(client_t* client);

/**
 * Send the messages that are in a clients reply queue
 * 
 * @param  client  The client
 */
void send_reply_queue(client_t* client);

/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param   client  The client has sent a message
 * @return          Normally zero, but 1 if exited because of re-exec or termination
 */
int message_received(client_t* client);

/**
 * Queue a message for multicasting
 * 
 * @param  message  The message
 * @param  length   The length of the message
 * @param  sender   The original sender of the message
 */
void queue_message_multicast(char* message, size_t length, client_t* sender);

/**
 * Receive a full message and update open status if the client closes
 * 
 * @param   client  The client
 * @return          Zero on success, -2 on failure, otherwise -1
 */
int fetch_message(client_t* client);

/**
 * Exec into the mdsinitrc script
 * 
 * @param  args  The arguments to the child process
 */
void run_initrc(char** args);

/**
 * Marshal the server's state into a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int marshal_server(int fd);

/**
 * Unmarshal the server's state from a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int unmarshal_server(int fd);

/**
 * Create, start and detache a slave thread
 * 
 * @param   thread     The address at where to store the thread
 * @param   socket_fd  The file descriptor of the slave's socket
 * @return             Zero on success, -1 on error, error message will have been printed
 */
int create_slave(pthread_t* thread_slot, int socket_fd);


#endif

