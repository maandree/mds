/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_LIBMDSCLIENT_COMM_H
#define MDS_LIBMDSCLIENT_COMM_H


#include <stdint.h>
#include <stddef.h>



/**
 * A connection to the display server
 */
typedef struct libmds_connection
{
  /**
   * The file descriptor of the socket
   * connected to the display server,
   * -1 if not connected
   */
  int socket_fd;
  
  /**
   * The ID of the _previous_ message
   */
  uint32_t message_id;
  
  /**
   * The client ID, `NULL` if anonymous
   */
  char* client_id;
  
} libmds_connection_t;


/**
 * Initialise a connection descriptor with the the default values
 * 
 * @param  this  The connection descriptor
 */
__attribute__((nonnull))
void libmds_connection_initialise(libmds_connection_t* restrict this);

/**
 * Allocate and initialise a connection descriptor
 * 
 * @return  The connection descriptor, `NULL` on error
 * 
 * @throws  ENOMEM  Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
libmds_connection_t* libmds_connection_create(void);

/**
 * Release all resources held by a connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_destroy(libmds_connection_t* restrict this);

/**
 * Release all resources held by a connection descriptor,
 * and release the allocation of the connection descriptor
 * 
 * @param  this  The connection descriptor, may be `NULL`
 */
void libmds_connection_free(libmds_connection_t* restrict this);


#endif

