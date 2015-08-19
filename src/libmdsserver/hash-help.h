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
#ifndef MDS_LIBMDSSERVER_HASH_HELP_H
#define MDS_LIBMDSSERVER_HASH_HELP_H


#include <stdlib.h>
#include <string.h>


/**
 * Calculate the hash of a string
 * 
 * @param   str  The string
 * @return       The hash of the string
 */
static inline size_t __attribute__((pure)) string_hash(const char* str)
{
  size_t hash = 0;
  
  if (str != NULL)
    while (*str != '\0')
      hash = hash * 31 + (size_t)(unsigned char)*str++;
  
  return hash;
}


/**
 * Check whether two `char*`:s are of equal value
 * 
 * @param   str_a  The first string
 * @param   str_b  The second string
 * @return         Whether the strings are equals
 */
static inline int __attribute__((pure)) string_comparator(char* str_a, char* str_b)
{
  if ((str_a != NULL) && (str_b != NULL) && (str_a != str_b))
    return !strcmp((char*)str_a, (char*)str_b);
  else
    return str_a == str_b;
}


#endif

