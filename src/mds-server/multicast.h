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
#ifndef MDS_MDS_SERVER_MULTICAST_H
#define MDS_MDS_SERVER_MULTICAST_H


#include "queued-interception.h"


#define MULTICAST_T_VERSION  0

/**
 * Message multicast state
 */
typedef struct multicast
{
  /**
   * Queue of clients that is listening this message
   */
  struct queued_interception* interceptions;
  
  /**
   * The number of clients in `interceptions`
   */
  size_t interceptions_count;
  
  /**
   * The index of the current/next client in `interceptions` to whom to send the message
   */
  size_t interceptions_ptr;
  
  /**
   * The message to send
   */
  char* message;
  
  /**
   * The length of `message`
   */
  size_t message_length;
  
  /**
   * How much of the message that has already been sent to the current recipient
   */
  size_t message_ptr;
  
  /**
   * How much of the message to skip if the recipient is not a modifier
   */
  size_t message_prefix;
  
} multicast_t;


/**
 * Initialise a message multicast state
 * 
 * @param  this  The message multicast state
 */
void multicast_initialise(multicast_t* restrict this);

/**
 * Destroy a message multicast state
 * 
 * @param  this  The message multicast state
 */
void multicast_destroy(multicast_t* restrict this);

/**
 * Calculate the buffer size need to marshal a message multicast state
 * 
 * @param   this  The client information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t multicast_marshal_size(const multicast_t* restrict this) __attribute__((pure));

/**
 * Marshals a message multicast state
 * 
 * @param   this  The message multicast state
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t multicast_marshal(const multicast_t* restrict this, char* restrict data);

/**
 * Unmarshals a message multicast state
 * 
 * @param   this  Memory slot in which to store the new message multicast state
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, errno will be set accordingly, otherwise the number of read bytes.
 *                Destroy the message multicast state on error.
 */
size_t multicast_unmarshal(multicast_t* restrict this, char* restrict data);

/**
 * Pretend to unmarshal a message multicast state
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t multicast_unmarshal_skip(char* restrict data) __attribute__((pure));



#endif

