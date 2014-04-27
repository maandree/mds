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
#include "mds-message.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


/**
 * Initialsie a message slot so that it can
 * be used by `mds_message_read`.
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, errno will be set accordingly.
 *                Destroy the message on error.
 */
int mds_message_initialise(mds_message_t* this)
{
  this->headers = NULL;
  this->header_count = 0;
  this->payload = NULL;
  this->payload_size = 0;
  this->payload_ptr = 0;
  this->buffer_size = 128;
  this->buffer_ptr = 0;
  this->buffer = malloc(this->buffer_size * sizeof(char));
  this->stage = 0;
  if (this->buffer == NULL)
    return -1;
  return 0;
}


/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
void mds_message_destroy(mds_message_t* this)
{
  if (this->headers != NULL)
    {
      size_t i;
      for (i = 0; i < this->header_count; i++)
	if (this->headers[i] != NULL)
	  free(this->headers[i]);
      free(this->headers);
    }
  
  if (this->payload != NULL)
    free(this->payload);
  
  if (this->buffer != NULL)
    free(this->buffer);
}


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
int mds_message_read(mds_message_t* this, int fd)
{
  size_t header_commit_buffer = 0;
  
  if (this->stage == 2)
    {
      if (this->headers != NULL)
	{
	  size_t i;
	  for (i = 0; i < this->header_count; i++)
	    if (this->headers[i] != NULL)
	      free(this->headers[i]);
	  free(this->headers);
	}
      this->header_count = 0;
      
      if (this->payload != NULL)
	free(this->payload);
      this->payload_size = 0;
      this->payload_ptr = 0;
      
      this->stage = 0;
    }
  
  for (;;)
    {
      size_t n, i;
      ssize_t got;
      char* p;
      
      if (this->stage == 0)
	while ((p = memchr(this->buffer, '\n', this->buffer_ptr)) != NULL)
	  {
	    size_t len = (size_t)(p - this->buffer) + 1;
	    char* header;
	    
	    if (len == 0)
	      {
		memmove(this->buffer, this->buffer + 1, this->buffer_ptr -= 1);
		for (i = 0; i < this->header_count; i++)
		  if (strstr(this->headers[i], "Length: ") == this->headers[i])
		    {
		      header = this->headers[i] + strlen("Length: ");
		      this->payload_size = (size_t)atoll(header);
		      for (; *header; header++)
			if ((*header < '0') || ('9' < *header))
			  return -2; /* Malformated value, enters unrecoverable state. */
		      break;
		    }
		this->stage = 1;
		
		if (this->payload_size > 0)
		  {
		    this->payload = malloc(this->payload_size * sizeof(char));
		    if (this->payload == NULL)
		      return -1;
		  }
		break;
	      }
	    
	    if (header_commit_buffer == 0)
	      {
		header_commit_buffer = 8;
		if (this->headers == NULL)
		  {
		    this->headers = malloc(header_commit_buffer * sizeof(char*));
		    if (this->headers == NULL)
		      return -1;
		  }
		else
		  {
		    char** old_headers = this->headers;
		    n = this->header_count + header_commit_buffer;
		    this->headers = realloc(this->headers, n * sizeof(char*));
		    if (this->headers == NULL)
		      {
			this->headers = old_headers;
			return -1;
		      }
		  }
	      }
	    
	    header = malloc(len * sizeof(char));
	    if (header == NULL)
	      return -1;
	    memcpy(header, this->buffer, len);
	    header[len - 1] = '\0';
	    memmove(this->buffer, this->buffer + len, this->buffer_ptr -= len);
	    if ((p = memchr(header, ':', len)) == NULL)
	      {
		/* Buck you, rawmemchr should not segfault the program. */
		free(header);
		return -2;
	      }
	    if (p[1] != ' ') /* Also an invalid format. */
	      {
		free(header);
		return -2;
	      }
	    this->headers[this->header_count++] = header;
	    header_commit_buffer -= 1;
	  }
      
      if ((this->stage == 1) && (this->payload_ptr > 0))
	{
	  size_t need = this->payload_size - this->payload_ptr;
	  if (this->buffer_ptr <= need)
	    memcpy(this->payload + this->payload_ptr, this->buffer, this->buffer_ptr);
	  else
	    {
	      memcpy(this->payload + this->payload_ptr, this->buffer, need);
	      memmove(this->buffer, this->buffer + need, this->buffer_ptr - need);
	    }
	  this->payload_ptr += this->buffer_ptr;
	  this->buffer_ptr = 0;
	  if (this->payload_ptr == this->payload_size)
	    {
	      this->stage = 3;
	      break;
	    }
	}
      
      n = this->buffer_size - this->buffer_ptr;
      
      if (n < 128)
	{
	  char* old_buffer = this->buffer;
	  this->buffer_size <<= 1;
	  this->buffer = realloc(this->buffer, this->buffer_size * sizeof(char));
	  if (this->buffer == NULL)
	    {
	      this->buffer = old_buffer;
	      this->buffer_size >>= 1;
	      return -1;
	    }
	  n = this->buffer_size - this->buffer_ptr;
	}
      
      errno = 0;
      got = read(fd, this->buffer + this->buffer_ptr, n);
      if (errno)
	{
	  this->buffer_ptr += (size_t)(got < 0 ? 0 : got);
	  return -1;
	}
    }
  
  return 0;
}


/**
 * Get the required allocation size for `data` of the
 * function `mds_message_marshal`
 * 
 * @param   this            The message
 * @param   include_buffer  Whether buffer should be marshalled (state serialisation, not communication)
 * @return                  The size of the message when marshalled
 */
size_t mds_message_marshal_size(mds_message_t* this, int include_buffer)
{
  size_t rc = this->header_count + this->payload_size;
  size_t i;
  for (i = 0; i < this->header_count; i++)
    rc += strlen(this->headers[i]);
  rc *= sizeof(char);
  rc += (include_buffer ? 4 : 2) * sizeof(size_t);
  rc += (include_buffer ? 1 : 0) * sizeof(int);
  return rc;
}


/**
 * Marshal a message, this can be used both when serialising
 * the servers state or to get the byte stream to send to
 * the recipient of the message
 * 
 * @param  this            The message
 * @param  data            Output buffer for the marshalled data
 * @param  include_buffer  Whether buffer should be marshalled (state serialisation, not communication)
 */
void mds_message_marshal(mds_message_t* this, char* data, int include_buffer)
{
  size_t i, n;
  
  ((size_t*)data)[0] = this->header_count;
  ((size_t*)data)[1] = this->payload_size;
  if (include_buffer)
    {
      ((size_t*)data)[2] = this->payload_ptr;
      ((size_t*)data)[3] = this->buffer_ptr;
    }
  data += (include_buffer ? 4 : 2) * sizeof(size_t) / sizeof(char);
  
  if (include_buffer)
    {
      ((int*)data)[0] = this->stage;
      data += sizeof(int) / sizeof(char);
    }
  
  for (i = 0; i < this->header_count; i++)
    {
      n = strlen(this->headers[i]) + 1;
      memcpy(data, this->headers[i], n * sizeof(char));
      data += n;
    }
  
  memcpy(data, this->payload, this->payload_size * sizeof(char));
  
  if (include_buffer)
    {
      data += this->payload_size;
      memcpy(data, this->buffer, this->buffer_ptr * sizeof(char));
    }
}


/**
 * Unmarshal a message, it is assumed that the buffer is marshalled
 * 
 * @param  this  Memory slot in which to store the new message
 * @param  data  In buffer with the marshalled data
 * @return       Non-zero on error, errno will be set accordingly.
 *               Destroy the message on error.
 */
int mds_message_unmarshal(mds_message_t* this, char* data)
{
  size_t i, n, header_count;
  
  header_count = ((size_t*)data)[0];
  this->header_count = 0;
  this->payload_size = ((size_t*)data)[1];
  this->payload_ptr = ((size_t*)data)[2];
  this->buffer_ptr = ((size_t*)data)[3];
  this->buffer_size = this->buffer_ptr;
  this->headers = NULL;
  this->payload = NULL;
  this->buffer = NULL;
  
  data += 4 * sizeof(size_t) / sizeof(char);
  
  this->stage = ((int*)data)[0];
  data += sizeof(int) / sizeof(char);
  
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
#if __WORDSIZE == 64
      this->buffer_size |= this->buffer_size >> 32;
#endif
      this->buffer_size += 1;
    }
  this->buffer_size <<= 7;
  
  if (header_count > 0)
    {
      this->headers = malloc(header_count * sizeof(char*));
      if (this->headers == NULL)
	return -1;
    }
  
  if (this->payload_size > 0)
    {
      this->payload = malloc(this->payload_size * sizeof(char));
      if (this->payload == NULL)
	return -1;
    }
  
  this->buffer = malloc(this->buffer_size * sizeof(char));
  if (this->buffer == NULL)
    return -1;
  
  for (i = 0; i < this->header_count; i++)
    {
      n = strlen(data) + 1;
      this->headers[i] = malloc(n * sizeof(char));
      if (this->headers[i] == NULL)
	return -1;
      memcpy(this->headers[i], data, n * sizeof(char));
      data += n;
      this->header_count++;
    }
  
  memcpy(this->payload, data, this->payload_size * sizeof(char));
  data += this->payload_size;
  
  memcpy(this->buffer, data, this->buffer_ptr * sizeof(char));
  
  return 0;
}
