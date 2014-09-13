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
#ifndef MDS_MDS_SERVER_QUEUED_INTERCEPTION_H
#define MDS_MDS_SERVER_QUEUED_INTERCEPTION_H


#include "client.h"

#include <stdint.h>


#define QUEUED_INTERCEPTION_T_VERSION  0

/**
 * A queued interception
 */
typedef struct queued_interception
{
  /**
   * The intercepting client
   */
  struct client* client;
  
  /**
   * The interception priority
   */
  int64_t priority;
  
  /**
   * Whether the messages may get modified by the client
   */
  int modifying;
  
  /**
   * The file descriptor of the intercepting client's socket (used for unmarshalling)
   */
  int socket_fd;
  
} queued_interception_t;


/**
 * Calculate the buffer size need to marshal a queued interception
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t queued_interception_marshal_size(void) __attribute__((const));

/**
 * Marshals a queued interception
 * 
 * @param   this  The queued interception
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t queued_interception_marshal(const queued_interception_t* restrict this, char* restrict data);

/**
 * Unmarshals a queued interception
 * 
 * @param   this  Memory slot in which to store the new queued interception
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, `errno` will be set accordingly, otherwise the number of read bytes.
 */
size_t queued_interception_unmarshal(queued_interception_t* restrict this, char* restrict data);

/**
 * Pretend to unmarshal a queued interception
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t queued_interception_unmarshal_skip(void) __attribute__((const));


#endif

