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
#include "raw-data.h"

#include "globals.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>



/**
 * Initialise a `source_code_t*`
 * 
 * @param  this  The `source_code_t*`
 */
void source_code_initialise(source_code_t* restrict this)
{
  this->lines        = NULL;
  this->real_lines   = NULL;
  this->content      = NULL;
  this->real_content = NULL;
  this->line_count   = 0;
}


/**
 * Release all data in a `source_code_t*`
 * 
 * @param  this  The `source_code_t*`
 */
void source_code_destroy(source_code_t* restrict this)
{
  free(this->lines),        this->lines        = NULL;
  free(this->real_lines),   this->real_lines   = NULL;
  free(this->content),      this->content      = NULL;
  free(this->real_content), this->real_content = NULL;
}


/**
 * Release all data in a `source_code_t*`, and free it
 * 
 * @param  this  The `source_code_t*`
 */
void source_code_free(source_code_t* restrict this)
{
  free(this->lines);
  free(this->real_lines);
  free(this->content);
  free(this->real_content);
  free(this);
}



/**
 * Read the content of a file, ignoring interruptions
 * 
 * @param   pathname  The file to read
 * @param   size      Output parameter for the size of the read content, in char:s
 * @return            The read content, `NULL` on error
 */
static char* read_file(const char* restrict pathname, size_t* restrict size)
{
  size_t buf_size = 8096;
  size_t buf_ptr = 0;
  char* restrict content = NULL;
  char* restrict old = NULL;
  int fd = -1;
  ssize_t got;
  
  /* Allocate buffer for the file's content. */
  fail_if (xmalloc(content, buf_size, char));
  /* Open the file to compile. */
  fail_if ((fd = open(pathname, O_RDONLY)) < 0);
  
  /* Read the file to compile. */
  for (;;)
    {
      /* Make sure the buffer is not small. */
      if (buf_size - buf_ptr < 2048)
	{
	  fail_if (xxrealloc(old, content, buf_size <<= 1, char));
	}
      /* Read a chunk of the file. */
      got = read(fd, content + buf_ptr, (buf_size - buf_ptr) * sizeof(char));
      if ((got < 0) && (errno == EINTR))  continue;
      else if (got < 0)                   goto pfail;
      else if (got == 0)                  break;
      buf_ptr += (size_t)got;
    }
  
  /* Shrink the buffer so it is not excessively large. */
  fail_if (xxrealloc(old, content, buf_ptr, char));
  
  /* Close file decriptor for the file. */
  close(fd);
  
  *size = buf_ptr;
  return content;
  
 pfail:
  xperror(*argv);
  free(old);
  free(content);
  if (fd >= 0)
    close(fd);
  return NULL;
}


/**
 * Remove comments from the content
 * 
 * @param   content  The code to shrink
 * @param   size     The size of `content`, in char:s
 * @return           The new size of `content`, in char:s; this function cannot fail
 */
static size_t remove_comments(char* restrict content, size_t size)
{
#define t  content[n_ptr++] = c
  
  size_t n_ptr = 0, o_ptr = 0;
  int comment = 0, quote = 0, escape = 0;
  
  while (o_ptr < size) /* TODO nested quotes */
    {
      char c = content[o_ptr++];
      /* Remove comment */
      if (comment)
	{
	  if (c == '\n')      t, comment = 0;
	}
      /* Quotes can contain comment symbols, quotes by also contain escapes. */
      else if (escape)        t, escape = 0;
      else if (quote)
	{
	  t;
	  if      (c == '\\')  escape = 1;
	  else if (c == '"')   quote = 0;
	}
      /* # is the comment symbol. */
      else if (c == '#')      comment = 1;
      /* " is the quote symbol. */
      else if (c == '"')      t, quote = 1;
      /* Code and whitespace.  */
      else                    t;
    }
  
  return n_ptr;
  
#undef t
}


/**
 * Create an array of each line in a text
 * 
 * @param   content  The text to split, it must end with an LF.
 *                   LF:s are treated as line endings rather than
 *                   new lines, this means that the final LF will
 *                   not create a new line in the returned array.
 *                   Each LF will be replaced by a NUL-character.
 * @param   length   The length of `content`.
 * @return           An array of each line in `content`. This
 *                   array will be `NULL`-terminated. It will also
 *                   reuse the allocate of `content`. This means
 *                   that each element must not be free:d, rather
 *                   you should simply free this returned allocation
 *                   and the allocation of `content`. On error
 *                   `NULL` is returned, and `content` will not
 *                   have been modified.
 */
static char** line_split(char* content, size_t length)
{
  char** restrict lines = NULL;
  size_t count = 0;
  size_t i, j;
  int new_line = 1;
  
  for (i = 0; i < length; i++)
    if (content[i] == '\n')
      count++;
  
  fail_if (xmalloc(lines, count + 1, char*));
  lines[count] = NULL;
  
  for (i = j = 0; i < length; i++)
    {
      if (new_line)
	new_line = 0, lines[j++] = content + i;
      if (content[i] == '\n')
	{
	  new_line = 1;
	  content[i] = '\0';
	}
    }
  
  return lines;
  
 pfail:
  xperror(*argv);
  return NULL;
}


/**
 * Read lines of a source file
 * 
 * @param   pathname     The pathname of the source file
 * @parma   source_code  Output parameter for read data
 * @return               Zero on success, -1 on error
 */
int read_source_lines(const char* restrict pathname, source_code_t* restrict source_code)
{
  char* content = NULL;
  char* real_content = NULL;
  char* old = NULL;
  size_t content_size;
  size_t real_content_size;
  char** lines = NULL;
  char** real_lines = NULL;
  size_t line_count = 0;
  
  /* Read the file. */
  content = read_file(pathname, &content_size);
  fail_if (content == NULL);
  
  /* Make sure the content ends with a new line. */
  if (!content_size || (content[content_size - 1] != '\n'))
    {
      fail_if (xxrealloc(old, content, content_size + 1, char));
      content[content_size++] = '\n';
    }
  
  /* Simplify file. */
  fail_if (xmalloc(real_content, content_size, char));
  memcpy(real_content, content, content_size * sizeof(char));
  real_content_size = content_size;
  content_size = remove_comments(content, content_size);
  fail_if (xxrealloc(old, content, content_size, char));
  
  /* Split by line.  */
  fail_if ((lines = line_split(content, content_size)) == NULL);
  fail_if ((real_lines = line_split(real_content, real_content_size)) == NULL);
  
  /* Count the number of lines. */
  while (lines[line_count] != NULL)
    line_count++;
  
  source_code->lines = lines;
  source_code->real_lines = real_lines;
  source_code->content = content;
  source_code->real_content = real_content;
  source_code->line_count = line_count;
  return 0;
  
 pfail:
  xperror(*argv);
  free(old);
  free(content);
  free(real_content);
  free(lines);
  free(real_lines);
  return -1;
}
