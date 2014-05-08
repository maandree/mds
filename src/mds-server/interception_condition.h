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
#ifndef MDS_MDS_SERVER_INTERCEPTION_CONDITION_H
#define MDS_MDS_SERVER_INTERCEPTION_CONDITION_H


#include <stddef.h>
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
   * The interception priority. The client should be
   * consistent with the priority for conditions that
   * are not mutually exclusive.
   */
  int64_t priority;
  
  /**
   * Whether the messages may get modified by the client
   */
  int modifying;
  
} interception_condition_t;


/**
 * Calculate the buffer size need to marshal an interception condition
 * 
 * @param   this  The interception condition
 * @return        The number of bytes to allocate to the output buffer
 */
size_t interception_condition_marshal_size(const interception_condition_t* restrict this) __attribute__((pure));

/**
 * Marshals an interception condition
 * 
 * @param   this  The interception condition
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t interception_condition_marshal(const interception_condition_t* restrict this, char* restrict data);

/**
 * Unmarshals an interception condition
 * 
 * @param   this  Memory slot in which to store the new interception condition
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, errno will be set accordingly, otherwise the number of read bytes.
 *                Destroy the interception condition on error.
 */
size_t interception_condition_unmarshal(interception_condition_t* restrict this, char* restrict data);


#endif

