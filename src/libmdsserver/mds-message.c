/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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
#include "mds-message.h"

#include "macros.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>


#define try(INSTRUCTION)   if ((r = INSTRUCTION) < 0)  return r


/**
 * Initialise a message slot so that it can
 * be used by `mds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, `errno` will be set accordingly.
 *                Destroy the message on error.
 */
int mds_message_initialise(mds_message_t* restrict this)
{
  this->headers = NULL;
  this->header_count = 0;
  this->payload = NULL;
  this->payload_size = 0;
  this->payload_ptr = 0;
  this->buffer_size = 128;
  this->buffer_ptr = 0;
  this->stage = 0;
  fail_if (xmalloc(this->buffer, this->buffer_size, char));
  return 0;
 fail:
  return -1;
}


/**
 * Zero initialise a message slot
 * 
 * @param  this  Memory slot in which to store the new message
 */
void mds_message_zero_initialise(mds_message_t* restrict this)
{
  this->headers = NULL;
  this->header_count = 0;
  this->payload = NULL;
  this->payload_size = 0;
  this->payload_ptr = 0;
  this->buffer = NULL;
  this->buffer_size = 0;
  this->buffer_ptr = 0;
  this->stage = 0;
}


/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
void mds_message_destroy(mds_message_t* restrict this)
{
  size_t i;
  if (this->headers != NULL)
    xfree(this->headers, this->header_count);
  
  free(this->payload), this->payload = NULL;
  free(this->buffer),  this->buffer  = NULL;
}


/**
 * Extend the header list's allocation
 * 
 * @param   this    The message
 * @param   extent  The number of additional entries
 * @return          Zero on success, -1 on error
 */
int mds_message_extend_headers(mds_message_t* restrict this, size_t extent)
{
  char** new_headers = this->headers;
  fail_if (xrealloc(new_headers, this->header_count + extent, char*));
  this->headers = new_headers;
  return 0;
 fail:
  return -1;
}


/**
 * Extend the read buffer by way of doubling
 * 
 * @param   this  The message
 * @return        Zero on success, -1 on error
 */
__attribute__((nonnull))
static int mds_message_extend_buffer(mds_message_t* restrict this)
{
  char* new_buf = this->buffer;
  fail_if (xrealloc(new_buf, this->buffer_size << 1, char));
  this->buffer = new_buf;
  this->buffer_size <<= 1;
  return 0;
 fail:
  return -1;
}


/**
 * Reset the header list and the payload
 * 
 * @param  this  The message
 */
__attribute__((nonnull))
static void reset_message(mds_message_t* restrict this)
{
  size_t i;
  if (this->headers != NULL)
    xfree(this->headers, this->header_count);
  this->header_count = 0;
  
  free(this->payload);
  this->payload = NULL;
  this->payload_size = 0;
  this->payload_ptr = 0;
}


/**
 * Read the headers the message and determine, and store, its payload's length
 * 
 * @param   this  The message
 * @return        Zero on success, negative on error (malformated message: unrecoverable state)
 */
__attribute__((pure, nonnull))
static int get_payload_length(mds_message_t* restrict this)
{
  char* header;
  size_t i;
  
  for (i = 0; i < this->header_count; i++)
    if (strstr(this->headers[i], "Length: ") == this->headers[i])
      {
	/* Store the message length. */
	header = this->headers[i] + strlen("Length: ");
	this->payload_size = atoz(header);
	
	/* Do not except a length that is not correctly formated. */
	for (; *header; header++)
	  if ((*header < '0') || ('9' < *header))
	    return -2; /* Malformated value, enters unrecoverable state. */
	
	/* Stop searching for the ‘Length’ header, we have found and parsed it. */
	break;
      }
  
  return 0;
}


/**
 * Verify that a header is correctly formatted
 * 
 * @param   header  The header, must be NUL-terminated
 * @param   length  The length of the header
 * @return          Zero if valid, negative if invalid (malformated message: unrecoverable state)
 */
__attribute__((pure, nonnull))
static int validate_header(const char* header, size_t length)
{
  char* p = memchr(header, ':', length * sizeof(char));
  
  if (verify_utf8(header, 0) < 0)
    /* Either the string is not UTF-8, or your are under an UTF-8 attack,
       let's just call this unrecoverable because the client will not correct. */
    return -2;
  
  if ((p == NULL) || /* Buck you, rawmemchr should not segfault the program. */
      (p[1] != ' ')) /* Also an invalid format. ' ' is mandated after the ':'. */
    return -2;
  
  return 0;
}


/**
 * Remove the beginning of the read buffer
 * 
 * @param  this        The message
 * @param  length      The number of characters to remove  
 * @param  update_ptr  Whether to update the buffer pointer
 */
__attribute__((nonnull))
static void unbuffer_beginning(mds_message_t* restrict this, size_t length, int update_ptr)
{
  memmove(this->buffer, this->buffer + length, (this->buffer_ptr - length) * sizeof(char));
  if (update_ptr)
    this->buffer_ptr -= length;
}


/**
 * Remove the header–payload delimiter from the buffer,
 * get the payload's size and allocate the payload
 * 
 * @param   this  The message
 * @return        The return value follows the rules of `mds_message_read`
 */
__attribute__((nonnull))
static int initialise_payload(mds_message_t* restrict this)
{
  /* Remove the \n (end of empty line) we found from the buffer. */
  unbuffer_beginning(this, 1, 1);
  
  /* Get the length of the payload. */
  if (get_payload_length(this) < 0)
    return -2; /* Malformated value, enters unrecoverable state. */
  
  /* Allocate the payload buffer. */
  if (this->payload_size > 0)
    fail_if (xmalloc(this->payload, this->payload_size, char));
  
  return 0;
 fail:
  return -1;
}


/**
 * Create a header from the buffer and store it
 * 
 * @param   this    The message
 * @param   length  The length of the header, including LF-termination
 * @return          The return value follows the rules of `mds_message_read`
 */
__attribute__((nonnull))
static int store_header(mds_message_t* restrict this, size_t length)
{
  char* header;
  
  /* Allocate the header. */
  fail_if (xmalloc(header, length, char)); /* Last char is a LF, which is substituted with NUL. */
  /* Copy the header data into the allocated header, */
  memcpy(header, this->buffer, length * sizeof(char));
  /* and NUL-terminate it. */
  header[length - 1] = '\0';
  
  /* Remove the header data from the read buffer. */
  unbuffer_beginning(this, length, 1);
  
  /* Make sure the the header syntax is correct so that
     the program does not need to care about it. */
  if (validate_header(header, length))
    {
      free(header);
      return -2;
    }
  
  /* Store the header in the header list. */
  this->headers[this->header_count++] = header;
  
  return 0;
 fail:
  return -1;
}


/**
 * Continue reading from the socket into the buffer
 * 
 * @param   this  The message
 * @param   fd    The file descriptor of the socket
 * @return        The return value follows the rules of `mds_message_read`
 */
__attribute__((nonnull))
static int continue_read(mds_message_t* restrict this, int fd)
{
  size_t n;
  ssize_t got;
  int r;
  
  /* Figure out how much space we have left in the read buffer. */
  n = this->buffer_size - this->buffer_ptr;
  
  /* If we do not have too much left, */
  if (n < 128)
    {
      /* grow the buffer, */
      try (mds_message_extend_buffer(this));
      
      /* and recalculate how much space we have left. */
      n = this->buffer_size - this->buffer_ptr;
    }
  
  /* Then read from the socket. */
  errno = 0;
  got = recv(fd, this->buffer + this->buffer_ptr, n, 0);
  this->buffer_ptr += (size_t)(got < 0 ? 0 : got);
  fail_if (errno);
  if (got == 0)
    fail_if ((errno = ECONNRESET));
  
  return 0;
 fail:
  return -1;
}


/**
 * Read the next message from a file descriptor of the socket
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   fd    The file descriptor of the socket
 * @return        Non-zero on error or interruption, `errno` will be
 *                set accordingly. Destroy the message on error,
 *                be aware that the reading could have been
 *                interrupted by a signal rather than canonical error.
 *                If -2 is returned `errno` will not have been set,
 *                -2 indicates that the message is malformated,
 *                which is a state that cannot be recovered from.
 */
int mds_message_read(mds_message_t* restrict this, int fd)
{
  size_t header_commit_buffer = 0;
  int r;
  
  /* If we are at stage 2, we are done and it is time to start over.
     This is important because the function could have been interrupted. */
  if (this->stage == 2)
    {
      reset_message(this);
      this->stage = 0;
    }
  
  /* Read from file descriptor until we have a full message. */
  for (;;)
    {
      char* p;
      size_t length;
      
      /* Stage 0: headers. */
      /* Read all headers that we have stored into the read buffer. */
      while ((this->stage == 0) &&
	     ((p = memchr(this->buffer, '\n', this->buffer_ptr * sizeof(char))) != NULL))
	if ((length = (size_t)(p - this->buffer)))
	  {
	    /* We have found a header. */
	    
	    /* On every eighth header found with this function call,
	       we prepare the header list for eight more headers so
	       that it does not need to be reallocated again and again. */
	    if (header_commit_buffer == 0)
	      try (mds_message_extend_headers(this, header_commit_buffer = 8));
	    
	    /* Create and store header. */
	    try (store_header(this, length + 1));
	    header_commit_buffer -= 1;
	  }
	else
	  {
	    /* We have found an empty line, i.e. the end of the headers. */
	    
	    /* Remove the header–payload delimiter from the buffer,
	       get the payload's size and allocate the payload. */
	    try (initialise_payload(this));
	    
	    /* Mark end of stage, next stage is getting the payload. */
	    this->stage = 1;
	  }
      
      
      /* Stage 1: payload. */
      if ((this->stage == 1) && (this->payload_size > 0))
	{
	  /* How much of the payload that has not yet been filled. */
	  size_t need = this->payload_size - this->payload_ptr;
	  /* How much we have of that what is needed. */
	  size_t move = min(this->buffer_ptr, need);
	  
	  /* Copy what we have, and remove it from the the read buffer. */
	  memcpy(this->payload + this->payload_ptr, this->buffer, move * sizeof(char));
	  unbuffer_beginning(this, move, 1);
	  
	  /* Keep track of how much we have read. */
	  this->payload_ptr += move;
	}
      if ((this->stage == 1) && (this->payload_ptr == this->payload_size))
	{
	  /* If we have filled the payload (or there was no payload),
	     mark the end of this stage, i.e. that the message is
	     complete, and return with success. */
	  this->stage = 2;
	  return 0;
	}
      
      
      /* If stage 1 was not completed. */
      
      /* Continue reading from the socket into the buffer. */
      try (continue_read(this, fd));
    }
}


/**
 * Get the required allocation size for `data` of the
 * function `mds_message_marshal`
 * 
 * @param   this  The message
 * @return        The size of the message when marshalled
 */
size_t mds_message_marshal_size(const mds_message_t* restrict this)
{
  size_t rc = this->header_count + this->payload_size;
  size_t i;
  for (i = 0; i < this->header_count; i++)
    rc += strlen(this->headers[i]);
  rc *= sizeof(char);
  rc += 4 * sizeof(size_t) + 2 * sizeof(int);
  return rc;
}


/**
 * Marshal a message for state serialisation
 * 
 * @param  this  The message
 * @param  data  Output buffer for the marshalled data
 */
void mds_message_marshal(const mds_message_t* restrict this, char* restrict data)
{
  size_t i, n;
  
  buf_set_next(data, int, MDS_MESSAGE_T_VERSION);
  
  buf_set_next(data, size_t, this->header_count);
  buf_set_next(data, size_t, this->payload_size);
  buf_set_next(data, size_t, this->payload_ptr);
  buf_set_next(data, size_t, this->buffer_ptr);
  buf_set_next(data, int, this->stage);
  
  for (i = 0; i < this->header_count; i++)
    {
      n = strlen(this->headers[i]) + 1;
      memcpy(data, this->headers[i], n * sizeof(char));
      buf_next(data, char, n);
    }
  
  memcpy(data, this->payload, this->payload_ptr * sizeof(char));
  buf_next(data, char, this->payload_ptr);
  
  memcpy(data, this->buffer, this->buffer_ptr * sizeof(char));
}


/**
 * Unmarshal a message for state deserialisation
 * 
 * @param  this  Memory slot in which to store the new message
 * @param  data  In buffer with the marshalled data
 * @return       Non-zero on error, `errno` will be set accordingly.
 *               Destroy the message on error.
 */
int mds_message_unmarshal(mds_message_t* restrict this, char* restrict data)
{
  size_t i, n, header_count;
  
  /* buf_get(data, int, 0, MDS_MESSAGE_T_VERSION); */
  buf_next(data, int, 1);
  
  this->header_count = 0;
  buf_get_next(data, size_t, header_count);
  buf_get_next(data, size_t, this->payload_size);
  buf_get_next(data, size_t, this->payload_ptr);
  buf_get_next(data, size_t, this->buffer_size = this->buffer_ptr);
  buf_get_next(data, int, this->stage);
  
  /* Make sure that the pointers are NULL so that they are
     not freed without being allocated when the message is
     destroyed if this function fails. */
  this->headers = NULL;
  this->payload = NULL;
  this->buffer  = NULL;
  
  /* To 2-power-multiple of 128 bytes. */
  this->buffer_size >>= 7;
  if (this->buffer_size == 0)
    this->buffer_size = 1;
  else
    {
      this->buffer_size -= 1;
      this->buffer_size |= this->buffer_size >> 1;
      this->buffer_size |= this->buffer_size >> 2;
      this->buffer_size |= this->buffer_size >> 4;
      this->buffer_size |= this->buffer_size >> 8;
      this->buffer_size |= this->buffer_size >> 16;
#if SIZE_MAX == UINT64_MAX
      this->buffer_size |= this->buffer_size >> 32;
#endif
      this->buffer_size += 1;
    }
  this->buffer_size <<= 7;
  
  /* Allocate header list, payload and read buffer. */
  
  if (header_count > 0)
    fail_if (xmalloc(this->headers, header_count, char*));
  
  if (this->payload_size > 0)
    fail_if (xmalloc(this->payload, this->payload_size, char));
  
  fail_if (xmalloc(this->buffer, this->buffer_size, char));
  
  /* Fill the header list, payload and read buffer. */
  
  for (i = 0; i < header_count; i++)
    {
      n = strlen(data) + 1;
      fail_if (xmemdup(this->headers[i], data, n, char));
      buf_next(data, char, n);
      this->header_count++;
    }
  
  memcpy(this->payload, data, this->payload_ptr * sizeof(char));
  buf_next(data, char, this->payload_ptr);
  
  memcpy(this->buffer, data, this->buffer_ptr * sizeof(char));
  
  return 0;
  
 fail:
  return -1;
}


/**
 * Get the required allocation size for `data` of the
 * function `mds_message_compose`
 * 
 * @param   this  The message
 * @return        The size of the message when marshalled
 */
size_t mds_message_compose_size(const mds_message_t* restrict this)
{
  size_t rc = 1 + this->payload_size;
  size_t i;
  for (i = 0; i < this->header_count; i++)
    rc += strlen(this->headers[i]) + 1;
  return rc * sizeof(char);
}


/**
 * Marshal a message for communication
 * 
 * @param  this  The message
 * @param  data  Output buffer for the marshalled data
 */
void mds_message_compose(const mds_message_t* restrict this, char* restrict data)
{
  size_t i, n;
  
  for (i = 0; i < this->header_count; i++)
    {
      n = strlen(this->headers[i]);
      memcpy(data, this->headers[i], n * sizeof(char));
      data += n;
      buf_set_next(data, char, '\n');
    }
  buf_set_next(data, char, '\n');
  
  if (this->payload_size > 0)
    memcpy(data, this->payload, this->payload_size * sizeof(char));
}

  
#undef try

