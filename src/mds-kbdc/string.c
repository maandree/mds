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
#include "string.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>



/**
 * Get the length of a string
 * 
 * @param   string  The string
 * @return          The length of the string
 */
size_t string_length(const char32_t* restrict string)
{
  size_t i = 0;
  while (string[i] != -1)
    i++;
  return i;
}


/**
 * Convert a NUL-terminated UTF-8 string to a -1-terminated UTF-32 string
 * 
 * @param   string  The UTF-8 string
 * @return          The string in UTF-32, `NULL` on error
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
  fail_if (xmalloc(rc, length + 1, char32_t));
  
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
 fail:
  return NULL;
}


/**
 * Convert a -1-terminated UTF-32 string to a NUL-terminated Modified UTF-8 string
 * 
 * Negative values apart from -1 are converted to 0x00
 * 
 * @param   string  The UTF-32 string
 * @return          The string in UTF-8, `NULL` on error
 */
char* string_encode(const char32_t* restrict string)
{
  size_t i, j, n = string_length(string);
  char* restrict rc;
  
  /* Allocated Modified UTF-8 string. */
  fail_if (xmalloc(rc, 7 * n + 1, char));
  
  /* Convert to Modified UTF-8. */
  for (i = j = 0; i < n; i++)
    {
#define _c(s)  rc[j++] = (char)(((c >> (s)) & 0x3F) | 0x80)
#define _t(s)  (0 < c) && ((uint32_t)c < (uint32_t)(1ULL << s))
      char32_t c = string[i];
      if      (c == 0)  rc[j++] = (char)0xC0, rc[j++] = (char)0x80;
      else if (_t( 7))  rc[j++] = (char)c;
      else if (_t(11))  rc[j++] = (char)((c >>  6) | 0xC0), _c( 0);
      else if (_t(16))  rc[j++] = (char)((c >> 12) | 0xE0), _c( 6), _c( 0);
      else if (_t(21))  rc[j++] = (char)((c >> 18) | 0xF0), _c(12), _c( 6), _c( 0);
      /* UTF-8 actually ends here, fits 32 planes. */
      else if (_t(26))  rc[j++] = (char)((c >> 24) | 0xF8), _c(18), _c(12), _c( 6), _c(0);
      else if (_t(31))  rc[j++] = (char)((c >> 30) | 0xFC), _c(24), _c(18), _c(12), _c(6), _c(0);
      /* The orginal UTF-8 specification ended here, fits 31 bits.
       * However, we added another byte so we can fit 32 bits
       * (actually we ca now fit 36 bits.)
       * However, we only needed this in `string_decode` which would
       * not require any changed, but we added it here for symmetry. */
      else              rc[j++] = (char)0xFE, _c(30), _c(24), _c(18), _c(12), _c(6), _c(0);
#undef _t
#undef _c
    }
  
  /* NUL-terminate and return. */
  return rc[j] = '\0', rc;
 fail:
  return NULL;
}


/**
 * Create duplicate of a string
 * 
 * @param   string  The string
 * @return          A duplicate of the string, `NULL` on error or if `string` is `NULL`
 */
char32_t* string_dup(const char32_t* restrict string)
{
  size_t n;
  char32_t* rc;
  if (string == NULL)
    return NULL;
  n = string_length(string) + 1;
  fail_if (xmemdup(rc, string, n, char32_t));
  return rc;
 fail:
  return NULL;
}

