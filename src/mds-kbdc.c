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
#include "mds-kbdc.h"

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
 * The number of elements in `argv`
 */
static int argc;

/**
 * The command line arguments
 */
static char** argv;



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
	  old = content;
	  fail_if (xrealloc(content, buf_size <<= 1, char));
	  old = NULL;
	}
      /* Read a chunk of the file. */
      got = read(fd, content + buf_ptr, (buf_size - buf_ptr) * sizeof(char));
      if ((got < 0) && (errno == EINTR))  continue;
      else if (got < 0)                   goto pfail;
      else if (got == 0)                  break;
      buf_ptr += (size_t)got;
    }
  
  /* Shrink the buffer so it is not excessively large. */
  fail_if (xrealloc(content, buf_ptr, char));
  
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
 * Remove non-functional code (comments and empty lines) from the content
 * 
 * @param   content  The code to shrink
 * @param   size     The size of `content`, in char:s
 * @return           The new size of `content`, in char:s; this function cannot fail
 */
static size_t shrink_file(char* restrict content, size_t size)
{
#define t     content[n_ptr++] = c
#define last  content[n_ptr - 1]
  
  size_t n_ptr = 0, o_ptr = 0;
  int comment = 0, quote = 0, escape = 0;
  
  while (o_ptr < size)
    {
      char c = content[o_ptr++];
      if (comment)
	{
	  if (c == '\n')      comment = 0;
	}
      else if (escape)        t, escape = 0;
      else if (quote)
	{
	  t;
	  if     (c == '\\')  escape = 1;
	  else if (c == '"')  quote = 0;
	}
      else if (c == '#')      comment = 1;
      else if (c == '"')      t, quote = 1;
      else if (!strchr("\n ", c) || !n_ptr || (last != c))
	{
	  /* Store data, but remove unnecessary new lines and unnecessary spaces. */
	  if ((c == '\n') && n_ptr && (last == ' '))  n_ptr--;
	  if ((c == ' ') && n_ptr && (last == '\n'))  continue;
	  t;
	}
    }
  
  return n_ptr;
  
#undef last
#undef t
}


/**
 * Compile a keyboard layout file
 * 
 * @param   argc_  The number of elements in `argv_`
 * @param   argv_  The command line arguments
 * @return         Zero on and only one success
 */
int main(int argc_, char** argv_)
{
  const char* pathname = argv_[1];
  char* restrict content = NULL;
  size_t content_size;
  
  argc = argc_;
  argv = argv_;
  
  /* Read the file. */
  content = read_file(pathname, &content_size);
  fail_if (content == NULL);
  /* Remove comments and empty lines. */
  content_size = shrink_file(content, content_size);
  fail_if (xrealloc(content, content_size, char));
  
  return 0;

 pfail:
  xperror(*argv);
  return 1;
}

