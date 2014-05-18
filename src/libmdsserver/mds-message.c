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

#include "macros.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>


/**
 * Initialise a message slot so that it can
 * be used by `mds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, errno will be set accordingly.
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
  if (xmalloc(this->buffer, this->buffer_size, char))
    return -1;
  return 0;
}


/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
void mds_message_destroy(mds_message_t* restrict this)
{
  if (this->headers != NULL)
    {
      size_t i;
      xfree(this->headers, this->header_count);
    }
  
  free(this->payload);
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
int mds_message_read(mds_message_t* restrict this, int fd)
{
  size_t header_commit_buffer = 0;
  
  /* If we are at stage 2, we are done and it is time to start over.
     This is important because the function coul have been interrupted. */
  if (this->stage == 2)
    {
      if (this->headers != NULL)
	{
	  size_t i;
	  xfree(this->headers, this->header_count);
	  this->headers = NULL;
	}
      this->header_count = 0;
      
      free(this->payload);
      this->payload = NULL;
      this->payload_size = 0;
      this->payload_ptr = 0;
      
      this->stage = 0;
    }
  
  /* Read from file descriptor until we have a full message. */
  for (;;)
    {
      size_t n, i;
      ssize_t got;
      char* p;
      
      /* Stage 0: headers. */
      if (this->stage == 0)
	/* Read all headers that we have stored into the read buffer. */
	while ((p = memchr(this->buffer, '\n', this->buffer_ptr * sizeof(char))) != NULL)
	  {
	    size_t len = (size_t)(p - this->buffer) + 1;
	    char* header;
	    
	    /* We have found an empty line, i.e. the end of the headers.*/
	    if (len == 1)
	      {
		/* Remove the \n (end of empty line) we found from the buffer. */
		memmove(this->buffer, this->buffer + 1, (this->buffer_ptr -= 1) * sizeof(char));
		
		/* Get the length of the payload. */
		for (i = 0; i < this->header_count; i++)
		  if (strstr(this->headers[i], "Length: ") == this->headers[i])
		    {
		      /* Store the message length. */
		      header = this->headers[i] + strlen("Length: ");
		      this->payload_size = (size_t)atoll(header);
		      
		      /* Do not except a length that is not correctly formated. */
		      for (; *header; header++)
			if ((*header < '0') || ('9' < *header))
			  return -2; /* Malformated value, enters unrecoverable state. */
		      
		      /* Stop searching for the ‘Length’ header, we have found and parsed it. */
		      break;
		    }
		
		/* Allocate the payload buffer. */
		if (this->payload_size > 0)
		  if (xmalloc(this->payload, this->payload_size, char))
		    return -1;
		
		/* Mark end of stage, next stage is getting the payload. */
		this->stage = 1;
		
		/* Stop searching for headers. */
		break;
	      }
	    
	    /* Head have found a header. */
	    
	    /* One every eighth heaer found with this function call,
	       we prepare the header list for eight more headers so
	       that it does not need to be reallocated again and again. */
	    if (header_commit_buffer == 0)
	      {
		char** old_headers = this->headers;
		header_commit_buffer = 8;
		n = this->header_count + header_commit_buffer;
		if (xrealloc(this->headers, n, char*))
		  {
		    this->headers = old_headers;
			return -1;
		  }
	      }
	    
	    /* Allocat the header.*/
	    if (xmalloc(header, len, char))
	      return -1;
	    /* Copy the header data into the allocated header, */
	    memcpy(header, this->buffer, len * sizeof(char));
	    /* and NUL-terminate it. */
	    header[len - 1] = '\0';
	    
	    /* Remove the header data from the read buffer. */
	    memmove(this->buffer, this->buffer + len, (this->buffer_ptr -= len) * sizeof(char));
	    
	    /* Make sure the the header syntex is correct so the the
	       program does not need to care about it. */
	    if ((p = memchr(header, ':', len * sizeof(char))) == NULL)
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
	    
	    /* Store the header in the heaer list. */
	    this->headers[this->header_count++] = header;
	    header_commit_buffer -= 1;
	  }
      
      /* Stage 1: payload. */
      if ((this->stage == 1) && (this->payload_size == 0))
	{
	  this->stage = 2;
	  return 0;
	}
      if ((this->stage == 1) && (this->payload_size > 0))
	{
	  /* How much of the payload that has not yet been filled. */
	  size_t need = this->payload_size - this->payload_ptr;
	  if (this->buffer_ptr <= need)
	    /* If we have everything that we need, just copy it into the payload buffer. */
	    memcpy(this->payload + this->payload_ptr, this->buffer, this->buffer_ptr * sizeof(char));
	  else
	    {
	      /* Otherwise, copy what we have, and remove it from the the read buffer. */
	      memcpy(this->payload + this->payload_ptr, this->buffer, need * sizeof(char));
	      memmove(this->buffer, this->buffer + need, (this->buffer_ptr - need) * sizeof(char));
	    }
	  
	  /* Keep track of how much we have read. */
	  this->payload_ptr += this->buffer_ptr;
	  if (this->buffer_ptr <= need)
	    /* Reset the read pointer as the read buffer is empty now. */
	    this->buffer_ptr = 0;
	  else
	    /* But if we head left over data that is for the next message,
	       just move the pointer back as much as we read. (Which is
	       actually what we do even we did not have excess data.) */
	    this->buffer_ptr -= need;
	  
	  /* If we have filled the payload, make the end of this stage,
	     i.e. that the message is complete, and return with success. */
	  if (this->payload_ptr == this->payload_size)
	    {
	      this->stage = 2;
	      return 0;
	    }
	}
      
      /* If stage 1 was not completed. */
      
      /* Figure out how much space we have left in the read buffer. */
      n = this->buffer_size - this->buffer_ptr;
      
      /* If we do not have too much left, grow the buffer, */
      if (n < 128)
	{
	  char* old_buffer = this->buffer;
	  if (xrealloc(this->buffer, this->buffer_size <<= 1, char))
	    {
	      this->buffer = old_buffer;
	      this->buffer_size >>= 1;
	      return -1;
	    }
	  
	  /* and recalculate how much space we have left. */
	  n = this->buffer_size - this->buffer_ptr;
	}
      
      /* Then read from the file descriptor. */
      errno = 0;
      got = recv(fd, this->buffer + this->buffer_ptr, n, 0);
      this->buffer_ptr += (size_t)(got < 0 ? 0 : got);
      if (errno)
	return -1;
      if (got == 0)
	{
	  errno = ECONNRESET;
	  return -1;
	}
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
  rc += 4 * sizeof(size_t) + 1 * sizeof(int);
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
  
  memcpy(data, this->payload, this->payload_size * sizeof(char));
  
  buf_next(data, char, this->payload_size);
  memcpy(data, this->buffer, this->buffer_ptr * sizeof(char));
}


/**
 * Unmarshal a message for state deserialisation
 * 
 * @param  this  Memory slot in which to store the new message
 * @param  data  In buffer with the marshalled data
 * @return       Non-zero on error, errno will be set accordingly.
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
  
  /* Make sure that the pointers are NULL so that they are
     not freed without being allocated when the message is
     destroyed if this function fails. */
  this->headers = NULL;
  this->payload = NULL;
  this->buffer  = NULL;
  
  buf_get_next(data, int, this->stage);
  
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
  
  /* Allocate header list, payload and read buffer. */
  
  if (header_count > 0)
    if (xmalloc(this->headers, header_count, char*))
      return -1;
  
  if (this->payload_size > 0)
    if (xmalloc(this->payload, this->payload_size, char))
      return -1;
  
  if (xmalloc(this->buffer, this->buffer_size, char))
    return -1;
  
  /* Fill the header list, payload and read buffer. */
  
  for (i = 0; i < this->header_count; i++)
    {
      n = strlen(data) + 1;
      if (xmalloc(this->headers[i], n, char))
	return -1;
      memcpy(this->headers[i], data, n * sizeof(char));
      buf_next(data, char, n);
      this->header_count++;
    }
  
  memcpy(this->payload, data, this->payload_size * sizeof(char));
  buf_next(data, char, this->payload_size);
  
  memcpy(this->buffer, data, this->buffer_ptr * sizeof(char));
  
  return 0;
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

