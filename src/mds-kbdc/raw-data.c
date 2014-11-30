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
#include "string.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>



/**
 * Initialise a `mds_kbdc_source_code_t*`
 * 
 * @param  this  The `mds_kbdc_source_code_t*`
 */
void mds_kbdc_source_code_initialise(mds_kbdc_source_code_t* restrict this)
{
  this->lines        = NULL;
  this->real_lines   = NULL;
  this->content      = NULL;
  this->real_content = NULL;
  this->line_count   = 0;
}


/**
 * Release all data in a `mds_kbdc_source_code_t*`
 * 
 * @param  this  The `mds_kbdc_source_code_t*`
 */
void mds_kbdc_source_code_destroy(mds_kbdc_source_code_t* restrict this)
{
  if (this == NULL)
    return;
  free(this->lines),        this->lines        = NULL;
  free(this->real_lines),   this->real_lines   = NULL;
  free(this->content),      this->content      = NULL;
  free(this->real_content), this->real_content = NULL;
}


/**
 * Release all data in a `mds_kbdc_source_code_t*`, and free it
 * 
 * @param  this  The `mds_kbdc_source_code_t*`
 */
void mds_kbdc_source_code_free(mds_kbdc_source_code_t* restrict this)
{
  if (this == NULL)
    return;
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
  if (buf_ptr) /* Simplest way to handle empty files: let the have the initial allocation size.  */
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
 * Find the end of a function call
 * 
 * @param   content  The code
 * @param   offset   The index after the first character after the backslash
 *                   that triggered this call
 * @param   size     The length of `code`
 * @return           The index of the character after the bracket that closes
 *                   the function call (may be outside the code by one character),
 *                   or `size` if the call do not end (that is, the code ends
 *                   prematurely), or zero if there is no function call at `offset`
 */
size_t get_end_of_call(char* restrict content, size_t offset, size_t size)
{
#define C                content[ptr]
#define r(lower, upper)  (((lower) <= C) && (C <= (upper)))
  
  size_t ptr = offset, call_end = 0;
  int escape = 0, quote = 0;
  
  /* Skip to end of function name. */
  while ((ptr < size) && (r('a', 'z') || r('A', 'Z') || (C == '_')))
    ptr++;
  
  /* Check that it is a function call. */
  if ((ptr == size) || (ptr == offset) || (C != '('))
    return 0;
  
  /* Find the end of the function call. */
  while (ptr < size)
    {
      char c = content[ptr++];
      
      /* Escapes may be longer than one character,
         but only the first can affect the parsing. */
      if (escape)                escape = 0;
      /* Nested function and nested quotes can appear. */
      else if (ptr <= call_end)  ;
      /* Quotes end with the same symbols as they start with,
         and quotes automatically escape brackets. */
      /* \ can either start a functon call or an escape. */
      else if (c == '\\')
	{
	  /* It may not be an escape, but registering it
	     as an escape cannot harm us since we only
	     skip the first character, and a function call
	     cannot be that short. */
	  escape = 1;
	  /* Nested quotes can appear at function calls. */
	  call_end = get_end_of_call(content, ptr, size);
	}
      else if (quote)            quote = (c != '"');
      /* End of function call, end of fun. */
      else if (c == ')')         break;
      /* " is the quote symbol. */
      else if (c == '"')         quote = 1;
    }
  
  return ptr;
  
#undef r
#undef C
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
  
  size_t n_ptr = 0, o_ptr = 0, call_end = 0;
  int comment = 0, quote = 0, escape = 0;
  
  while (o_ptr < size)
    {
      char c = content[o_ptr++];
      /* Remove comment. */
      if (comment)
	{
	  if (c == '\n')           t, comment = 0;
	}
      /* Escapes may be longer than one character,
         but only the first can affect the parsing. */
      else if (escape)             t, escape = 0;
      /* Nested quotes can appear at function calls. */
      else if (o_ptr <= call_end)  t;
      /* \ can either start a functon call or an escape. */
      else if (c == '\\')
	{
	  t;
	  /* It may not be an escape, but registering it
	     as an escape cannot harm us since we only
	     skip the first character, and a function call
	     cannot be that short. */
	  escape = 1;
	  /* Nested quotes can appear at function calls. */
	  call_end = get_end_of_call(content, o_ptr, size);
	}
      /* Quotes end with the same symbols as they start with,
         and quotes automatically escape comments. */
      else if (quote)
	{
	  t;
	  if (c == '"')            quote = 0;
	}
      /* # is the comment symbol. */
      else if (c == '#')           comment = 1;
      /* " is the quote symbol. */
      else if (c == '"')           t, quote = 1;
      /* Code and whitespace.  */
      else                         t;
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
 * @param   source_code  Output parameter for read data
 * @return               Zero on success, -1 on error
 */
int read_source_lines(const char* restrict pathname, mds_kbdc_source_code_t* restrict source_code)
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


/**
 * Encode a character in UTF-8
 * 
 * @param   buffer     The buffer where the character should be stored
 * @param   character  The character
 * @return             The of the character in `buffer`, `NULL` on error
 */
static char* encode_utf8(char* buffer, char32_t character)
{
  char32_t text[2];
  char* restrict str;
  char* restrict str_;
  
  text[0] = character;
  text[1] = -1;
  
  if (str_ = str = string_encode(text), str == NULL)
    return NULL;
  
  while (*str)
    *buffer++ = *str++;
  
  free(str_);
  return buffer;
}


/**
 * Parse a quoted and escaped string that may not include function calls or variable dereferences
 * 
 * @param   string  The string
 * @return          The string in machine-readable format, `NULL` on error
 */
char* parse_raw_string(const char* restrict string)
{
#define r(cond, lower, upper)  ((cond) && ((lower) <= c) && (c <= (upper)))
  
  char* rc;
  char* p;
  int escape = 0;
  char32_t buf = 0;
  char c;
  
  /* We know that the output string can only be shorter because
   * it is surrounded by 2 quotes and escape can only be longer
   * then what they escape, for example \uA0, is four characters,
   * but when parsed it generateds 2 bytes in UTF-8, and their
   * is not code point whose UTF-8 encoding is longer than its
   * hexadecimal representation. */
  p = rc = malloc(strlen(string) * sizeof(char));
  if (rc == NULL)
    return NULL;
  
  while ((c = *string++))
    if      (r(escape ==  8, '0', '7'))  buf = (buf << 3) | (c & 15);
    else if (r(escape == 16, '0', '9'))  buf = (buf << 4) | (c & 15);
    else if (r(escape == 16, 'a', 'f'))  buf = (buf << 4) | ((c & 15) + 9);
    else if (r(escape == 16, 'A', 'F'))  buf = (buf << 4) | ((c & 15) + 9);
    else if (escape > 1)
      {
	escape = 0;
	fail_if ((p = encode_utf8(p, buf), p == NULL));
	if (c != '.')
	  *p++ = c;
      }
    else if (escape == 1)
      {
	escape = 0, buf = 0;
	switch (c)
	  {
	  case '0':  escape = 8;   break;
	  case 'u':  escape = 16;  break;
	  default:   *p++ = c;     break;
	  }
      }
    else if (c == '\\')  escape = 1;
    else if (c != '\"')  *p++ = c;
  
  *p = '\0';
  return rc;
 pfail:
  free(rc);
  return NULL;
#undef r
}

