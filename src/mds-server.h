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


#include <libmdsserver/mds-message.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>



/**
 * A condition for a message being intercepted by a client
 */
typedef struct interception_condition
{
  /**
   * The header of messages to intercept, optionally with a value,
   * empty (most not be NULL) for all messages.
   */
  char* condition;
  
  /**
   * The hash of the header of messages to intercept
   */
  size_t header_hash;
  
  /**
   * The interception priority
   */
  int64_t priority;
  
  /**
   * Whether the messages may get modified by the client
   */
  int modifying;
  
} interception_condition_t;


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
  mds_message_t message;
  
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
   * The messages interception conditions conditions
   * for the client
   */
  interception_condition_t* interception_conditions;
  
  /**
   * The number of interception conditions
   */
  size_t interception_conditions_count;
  
} client_t;


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Outout data
 */
void* slave_loop(void* data);

/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param  client  The client has sent a message
 */
void message_received(client_t* client);

/**
 * Add an interception condition for a client
 * 
 * @param  client     The client
 * @param  condition  The header, optionally with value, to look for, or empty (not `NULL`) for all messages
 * @param  priority   Interception priority
 * @param  modifying  Whether the client may modify the messages
 * @param  stop       Whether the condition should be removed rather than added
 */
void add_intercept_condition(client_t* client, char* condition, int64_t priority, int modifying, int stop);

/**
 * Multicast a message
 * 
 * @param  message  The message
 * @param  length   The length of the message
 */
void multicast_message(char* message, size_t length);

/**
 * Exec into the mdsinitrc script
 * 
 * @param  args  The arguments to the child process
 */
void run_initrc(char** args);

/**
 * Called with the signal SIGUSR1 is caught.
 * This function should cue a re-exec of the program.
 * 
 * @param  signo  The caught signal
 */
void sigusr1_trap(int signo);

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


#endif

