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
#include "string.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>



/**
 * Get the length of a string.
 * 
 * @param   string  The string.
 * @return          The length of the string.
 */
size_t string_length(const char32_t* restrict string)
{
  size_t i = 0;
  while (string[i] >= 0)
    i++;
  return i;
}


/**
 * Convert a NUL-terminated UTF-8 string to a -1-terminated UTF-32 string.
 * 
 * @param   string  The UTF-8 string.
 * @return          The string in UTF-32, `NULL` on error.
 */
char32_t* string_decode(const char* restrict string)
{
  size_t i, j, n, length = 0;
  char32_t* rc;
  
  /* Get the length of the UTF-32 string, excluding termination. */
  for (i = 0; string[i]; i++)
    if ((string[i] & 0xC0) != 0x80)
      length++;
  
  /* Allocated UTF-32 string. */
  if (xmalloc(rc, length + 1, char32_t))
    return NULL;
  
  /* Convert to UTF-32. */
  for (i = j = n = 0; string[i]; i++)
    {
      char c = string[i];
      if (n)
	{
	  rc[j] <<= 6, rc[j] |= c & 0x3F;
	  if (--n == 0)
	    j++;
	}
      else if ((c & 0xC0) == 0xC0)
	{
	  while (c & 0x80)
	    n++, c = (char)(c << 1);
	  rc[j] = c >> n--;
	}
      else
	rc[j++] = c & 255;
    }
  
  /* -1-terminate and return. */
  return rc[length] = -1, rc;
}

