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
#include "inbound.h"
/* Some optimisations have been attempted. Verify that this implementation
 * works, then update the implementation in libmdsserver. */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>


#define try(INSTRUCTION)    if ((r = INSTRUCTION) < 0)  return r
#define static_strlen(str)  (sizeof(str) / sizeof(char) - 1)



/**
 * Initialise a message slot so that it can
 * be used by `mds_message_read`
 * 
 * @param   this  Memory slot in which to store the new message
 * @return        Non-zero on error, `errno` will be set accordingly.
 *                Destroy the message on error.
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_message_initialise(libmds_message_t* restrict this)
{
  this->headers = NULL;
  this->header_count = 0;
  this->payload = NULL;
  this->payload_size = 0;
  this->buffer_size = 128;
  this->buffer_ptr = 0;
  this->stage = 0;
  this->buffer = malloc(this->buffer_size * sizeof(char));
  return this->buffer == NULL ? -1 : 0;
}


/**
 * Release all resources in a message, should
 * be done even if initialisation fails
 * 
 * @param  this  The message
 */
void libmds_message_destroy(libmds_message_t* restrict this)
{
  free(this->headers), this->headers = NULL;
  free(this->buffer),  this->buffer  = NULL;
}


/**
 * Check whether a NUL-terminated string is encoded in UTF-8
 * 
 * @param   string              The string
 * @param   allow_modified_nul  Whether Modified UTF-8 is allowed, which allows a two-byte encoding for NUL
 * @return                      Zero if good, -1 on encoding error
 */
__attribute__((nonnull, warn_unused_result))
static int verify_utf8(const char* string, int allow_modified_nul) /* Cannibalised from <libmdsserver/util.h> */
{
  static long BYTES_TO_MIN_BITS[] = {0, 0,  8, 12, 17, 22, 37};
  static long BYTES_TO_MAX_BITS[] = {0, 7, 11, 16, 21, 26, 31};
  long bytes = 0, read_bytes = 0, bits = 0, c, character;
  
  /*                                                      min bits  max bits
    0.......                                                 0         7
    110..... 10......                                        8        11
    1110.... 10...... 10......                              12        16
    11110... 10...... 10...... 10......                     17        21
    111110.. 10...... 10...... 10...... 10......            22        26
    1111110. 10...... 10...... 10...... 10...... 10......   27        31
   */
  
  while ((c = (long)(*string++)))
    if (read_bytes == 0)
      {
	/* First byte of the character. */
	
	if ((c & 0x80) == 0x00)
	  /* Single-byte character. */
	  continue;
	
	if ((c & 0xC0) == 0x80)
	  /* Single-byte character marked as multibyte, or
	     a non-first byte in a multibyte character. */
	  return -1;
	
	/* Multibyte character. */
	while ((c & 0x80))
	  bytes++, c <<= 1;
	read_bytes = 1;
	character = c & 0x7F;
	if (bytes > 6)
	  /* 31-bit characters can be encoded with 6-bytes,
	     and UTF-8 does not cover higher code points. */
	  return -1;
      }
    else
      {
	/* Not first byte of the character. */
	
	if ((c & 0xC0) != 0x80)
	  /* Beginning of new character before a
	     multibyte character has ended. */
	  return -1;
	
	character = (character << 6) | (c & 0x7F);
	
	if (++read_bytes < bytes)
	  /* Not at last byte yet. */
	  continue;
	
	/* Check that the character is not unnecessarily long. */
	while (character)
	  character >>= 1, bits++;
	bits = ((bits == 0) && (bytes == 2) && allow_modified_nul) ? 8 : bits;
	if ((bits < BYTES_TO_MIN_BITS[bytes]) || (BYTES_TO_MAX_BITS[bytes] < bits))
	  return -1;
	
	read_bytes = bytes = bits = 0;
      }
  
  /* Make sure we did not stop at the middle of a multibyte character. */
  return read_bytes == 0 ? 0 : -1;
}



/**
 * Extend the header list's allocation
 * 
 * @param   this    The message
 * @param   extent  The number of additional entries
 * @return          Zero on success, -1 on error
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
static int extend_headers(libmds_message_t* restrict this, size_t extent)
{
  char** new_headers = realloc(this->headers, (this->header_count + extent) * sizeof(char*));
  if (new_headers == NULL)
    return -1;
  this->headers = new_headers;
  return 0;
}


/**
 * Extend the read buffer by way of doubling
 * 
 * @param   this   The message
 * @param   shift  The number of bits to shift buffer size
 * @return         Zero on success, -1 on error
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
static int extend_buffer(libmds_message_t* restrict this, int shift)
{
  size_t i, n = this->header_count;
  char* new_buf = realloc(this->buffer, (this->buffer_size << shift) * sizeof(char));
  if (new_buf == NULL)
    return -1;
  if (new_buf != this->buffer)
    for (i = 0; i < n; i++)
      this->headers[i] = new_buf + (size_t)(this->headers[i] - this->buffer);
  this->buffer = new_buf;
  this->buffer_size <<= shift;
  return 0;
}

/**
 * Reset the header list and the payload
 * 
 * @param  this  The message
 */
__attribute__((nonnull))
static void reset_message(libmds_message_t* restrict this)
{
  size_t overrun = this->buffer_ptr - this->buffer_off;
  
  if (overrun)
    memmove(this->buffer, this->buffer + this->buffer_off, overrun * sizeof(char));
  this->buffer_ptr -= this->buffer_off;
  this->buffer_off = 0;
  
  free(this->headers);
  this->headers = NULL;
  this->header_count = 0;
  
  this->payload = NULL;
  this->payload_size = 0;
}


/**
 * Read the headers the message and determine, and store, its payload's length
 * 
 * @param   this  The message
 * @return        Zero on success, negative on error (malformated message: unrecoverable state)
 */
__attribute__((pure, nonnull, warn_unused_result))
static int get_payload_length(libmds_message_t* restrict this)
{
  char* header;
  size_t i;
  
  for (i = 0; i < this->header_count; i++)
    if (strstr(this->headers[i], "Length: ") == this->headers[i])
      {
	/* Store the message length. */
	header = this->headers[i] + static_strlen("Length: ");
	this->payload_size = (size_t)atoll(header);
	
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
__attribute__((pure, nonnull, warn_unused_result))
static int validate_header(const char* header, size_t length)
{
  char* p = memchr(header, ':', length * sizeof(char));
  
  if (verify_utf8(header, 0) < 0)
    /* Either the string is not UTF-8, or your are under an UTF-8 attack,
       lets just call this unrecoverable because the client will not correct. */
    return -2;
  
  if ((p == NULL) || /* Buck you, rawmemchr should not segfault the program. */
      (p[1] != ' ')) /* Also an invalid format. ' ' is mandated after the ':'. */
    return -2;
  
  return 0;
}


/**
 * Remove the header–payload delimiter from the buffer,
 * get the payload's size and allocate the payload
 * 
 * @param   this  The message
 * @return        The return value follows the rules of `mds_message_read`
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull))
static int initialise_payload(libmds_message_t* restrict this)
{
  int shift = 0;
  
  /* Skip over the \n (end of empty line) we found from the buffer. */
  this->buffer_off++;
  
  /* Get the length of the payload. */
  if (get_payload_length(this) < 0)
    return -2; /* Malformated value, enters unrecoverable state. */
  
  /* Reallocate the buffer if it is too small. */
  while (this->buffer_off + this->payload_size > this->buffer_size << shift)
    shift++;
  if (shift ? (extend_buffer(this, shift) < 0) : 0)
    return -1;
  
  /* Set pointer to payload. */
  this->payload = this->buffer + this->buffer_off;
  
  return 0;
}


/**
 * Store a header
 * 
 * @param   this    The message
 * @param   length  The length of the header, including LF-termination
 * @return          The return value follows the rules of `mds_message_read`
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 */
__attribute__((nonnull, warn_unused_result))
static int store_header(libmds_message_t* restrict this, size_t length)
{
  char* header;
  
  /* Get pointer to header in buffer. */
  header = this->buffer + this->buffer_off;
  /* NUL-terminate the header. */
  header[length - 1] = '\0';
  
  /* Update read offset. */
  this->buffer += length;
  
  /* Make sure the the header syntax is correct so that
     the program does not need to care about it. */
  if (validate_header(header, length))
    return -2;
  
  /* Store the header in the header list. */
  this->headers[this->header_count++] = header;
  
  return 0;
}


/**
 * Continue reading from the socket into the buffer
 * 
 * @param   this  The message
 * @param   fd    The file descriptor of the socket
 * @return        The return value follows the rules of `mds_message_read`
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 * @throws          Any error specified for recv(3)
 */
__attribute__((nonnull))
static int continue_read(libmds_message_t* restrict this, int fd)
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
      try (extend_buffer(this, 1));
      
      /* and recalculate how much space we have left. */
      n = this->buffer_size - this->buffer_ptr;
    }
  
  /* Then read from the socket. */
  errno = 0;
  got = recv(fd, this->buffer + this->buffer_ptr, n, 0);
  this->buffer_ptr += (size_t)(got < 0 ? 0 : got);
  if (errno)
    return -1;
  if (got == 0)
    return errno = ECONNRESET, -1;
  
  return 0;
}


/**
 * Read the next message from a file descriptor
 * 
 * @param   this  Memory slot in which to store the new message
 * @param   fd    The file descriptor
 * @return        Non-zero on error or interruption, `errno` will be
 *                set accordingly. Destroy the message on error,
 *                be aware that the reading could have been
 *                interrupted by a signal rather than canonical error.
 *                If -2 is returned `errno` will not have been set,
 *                -2 indicates that the message is malformated,
 *                which is a state that cannot be recovered from.
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the process hit the RLIMIT_AS or
 *                  RLIMIT_DATA limit described in getrlimit(2).
 * @throws          Any error specified for recv(3)
 */
int libmds_message_read(libmds_message_t* restrict this, int fd)
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
	     ((p = memchr(this->buffer + this->buffer_off, '\n',
			  (this->buffer_ptr - this->buffer_off) * sizeof(char))) != NULL))
	if ((length = (size_t)(p - (this->buffer + this->buffer_off))))
	  {
	    /* We have found a header. */
	    
	    /* On every eighth header found with this function call,
	       we prepare the header list for eight more headers so
	       that it does not need to be reallocated again and again. */
	    if (header_commit_buffer == 0)
	      try (extend_headers(this, header_commit_buffer = 8));
	    
	    /* Store header. */
	    try (store_header(this, length + 1));
	    header_commit_buffer -= 1;
	  }
	else
	  {
	    /* We have found an empty line, i.e. the end of the headers. */
	    
	    /* Make sure the full payload fits the buffer, and set
	     * the payload buffer pointer. */
	    try (initialise_payload(this));
	    
	    /* Mark end of stage, next stage is getting the payload. */
	    this->stage = 1;
	  }
      
      
      /* Stage 1: payload. */
      if ((this->stage == 1) && (this->buffer_ptr - this->buffer_off >= this->payload_size))
	{
	  /* If we have filled the payload (or there was no payload),
	     mark the end of this stage, i.e. that the message is
	     complete, and return with success. */
	  this->stage = 2;
	  
	  /* Mark the end of the message. */
	  this->buffer_off += this->payload_size;
	  
	  return 0;
	}
      
      
      /* If stage 1 was not completed. */
      
      /* Continue reading from the socket into the buffer. */
      try (continue_read(this, fd));
    }
}

