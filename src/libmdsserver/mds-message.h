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
#ifndef MDS_LIBMDSSERVER_MDS_MESSAGE_H
#define MDS_LIBMDSSERVER_MDS_MESSAGE_H


#include <stddef.h>


#define MDS_MESSAGE_T_VERSION  0

/**
 * Message passed between a server and a client or between two of either
 */
typedef struct mds_message
{
  /**
   * The headers in the message, each element in this list
   * as an unparsed header, it consists of both the header
   * name and its associated value, joined by ": ". A header
   * cannot be `NULL` (unless its memory allocation failed,)
   * but `headers` itself is NULL if there are no headers.
   * The "Length" should be included in this list.
   */
  char** headers;
  
  /**
   * The number of headers in the message
   */
  size_t header_count;
  
  /**
   * The payload of the message, `NULL` if none (of zero-length)
   */
  char* payload;
  
  /**
   * The size of the payload
   */
  size_t payload_size;
  
  /**
   * How much of the payload that has been stored (internal data)
   */
  size_t payload_ptr;
  
  /**
   * Internal buffer for the reading function (internal data)
   */
  char* buffer;
  
  /**
   * The size allocated to `buffer` (internal data)
   */
  size_t buffer_size;
  
  /**
   * The number of bytes used in `buffer` (internal data)
   */
  size_t buffer_ptr; /* TODO: would it be better to use a circular buffer? */
  
  /**
   * 0 while reading headers, 1 while reading payload, and 2 when done (internal data)
   */
  int stage;
  
} mds_message_t;


/**
 * Initialise a message slot so that it can
 * be used by `mds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, errno will be set accordingly.
 *                Destroy the message on error.
 */
int mds_message_initialise(mds_message_t* restrict this);

/**
 * Zero initialise a message slot
 * 
 * @param  this  Memory slot in which to store the new message
 */
void mds_message_zero_initialise(mds_message_t* restrict this);

/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
void mds_message_destroy(mds_message_t* restrict this);

/**
 * Read the next message from a file descriptor
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   fd    The file descriptor
 * @return        Non-zero on error or interruption, errno will be
 *                set accordingly. Destroy the message on error,
 *                be aware that the reading could have been
 *                interrupted by a signal rather than canonical error.
 *                If -2 is returned errno will not have been set,
 *                -2 indicates that the message is malformated,
 *                which is a state that cannot be recovered from.
 */
int mds_message_read(mds_message_t* restrict this, int fd);

/**
 * Get the required allocation size for `data` of the
 * function `mds_message_marshal`
 * 
 * @param   this  The message
 * @return        The size of the message when marshalled
 */
size_t mds_message_marshal_size(const mds_message_t* restrict this) __attribute__((pure));

/**
 * Marshal a message for state serialisation
 * 
 * @param  this  The message
 * @param  data  Output buffer for the marshalled data
 */
void mds_message_marshal(const mds_message_t* restrict this, char* restrict data);

/**
 * Unmarshal a message for state deserialisation
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   data  In buffer with the marshalled data
 * @return        Non-zero on error, errno will be set accordingly.
 *                Destroy the message on error.
 */
int mds_message_unmarshal(mds_message_t* restrict this, char* restrict data);

/**
 * Get the required allocation size for `data` of the
 * function `mds_message_compose`
 * 
 * @param   this  The message
 * @return        The size of the message when marshalled
 */
size_t mds_message_compose_size(const mds_message_t* restrict this) __attribute__((pure));

/**
 * Marshal a message for communication
 * 
 * @param  this  The message
 * @param  data  Output buffer for the marshalled data
 */
void mds_message_compose(const mds_message_t* restrict this, char* restrict data);


#endif

