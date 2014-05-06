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
#ifndef MDS_LIBMDSSERVER_MACROS_H
#define MDS_LIBMDSSERVER_MACROS_H


/*
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
*/


/**
 * Wrapper for `snprintf` that allows you to forget about the buffer size
 * 
 * @param  buffer:char[]  The buffer, must be of the type `char[]` and not `char*`
 * @param  format:char*   The format
 * @param  ...            The arguments
 */
#define xsnprintf(buffer, format, ...)  \
  snprintf(buffer, sizeof(buffer) / sizeof(char), format, __VA_ARGS__)


/**
 * Wrapper for `fprintf` that prints to `stderr` with
 * the program name prefixed and new line suffixed
 * 
 * @param  format:char*  The format
 */
#define eprint(format)  \
  fprintf(stderr, "%s: " format "\n", *argv)


/**
 * Wrapper for `fprintf` that prints to `stderr` with
 * the program name prefixed and new line suffixed
 * 
 * @param  format:char*   The format
 * @param  ...            The arguments
 */
#define eprintf(format, ...)  \
  fprintf(stderr, "%s: " format "\n", *argv, __VA_ARGS__)


/**
 * Wrapper for `pthread_mutex_lock` and `pthread_mutex_unlock`
 * 
 * @param  mutex:pthread_mutex_t  The mutex
 * @param  instructions           The instructions to run while the mutex is locked
 */
#define with_mutex(mutex, instructions)  \
  pthread_mutex_lock(&(mutex));          \
  instructions                           \
  pthread_mutex_unlock(&(mutex))


/**
 * Return the maximum value of two values
 * 
 * @param   a  One of the values
 * @param   b  The other one of the values
 * @return     The maximum value
 */
#define max(a, b)  \
  (a < b ? b : a)


/**
 * Return the minimum value of two values
 * 
 * @param   a  One of the values
 * @param   b  The other one of the values
 * @return     The minimum value
 */
#define min(a, b)  \
  (a < b ? a : b)


/**
 * Cast a buffer to another type and get the slot for an element
 * 
 * @param   buffer:char*  The buffer
 * @param   type          The data type of the elements for the data type to cast the buffer to
 * @param   index:size_t  The index of the element to address
 * @return  [type]        A slot that can be set or get
 */
#define buf_cast(buffer, type, index)  \
  ((type*)(buffer))[index]


/**
 * Set the value of an element a buffer that is being cast
 * 
 * @param   buffer:char*   The buffer
 * @param   type           The data type of the elements for the data type to cast the buffer to
 * @param   index:size_t   The index of the element to address
 * @param   variable:type  The new value of the element
 * @return  :variable      The new value of the element
 */
#define buf_set(buffer, type, index, variable)	\
  ((type*)(buffer))[index] = (variable)


/**
 * Get the value of an element a buffer that is being cast
 * 
 * @param   buffer:char*   The buffer
 * @param   type           The data type of the elements for the data type to cast the buffer to
 * @param   index:size_t   The index of the element to address
 * @param   variable:type  Slot to set with the value of the element
 * @return  :variable      The value of the element
 */
#define buf_get(buffer, type, index, variable)	\
  variable = ((type*)(buffer))[index]


/**
 * Increase the pointer of a buffer
 * 
 * @param  buffer:char*  The buffer
 * @param  type          A data type
 * @param  count         The number elements of the data type `type` to increase the pointer with
 */
#define buf_next(buffer, type, count)  \
  buffer += (count) * sizeof(type) / sizeof(char)


/**
 * Decrease the pointer of a buffer
 * 
 * @param  buffer:char*  The buffer
 * @param  type          A data type
 * @param  count         The number elements of the data type `type` to decrease the pointer with
 */
#define buf_prev(buffer, type, count)  \
  buffer -= (count) * sizeof(type) / sizeof(char)


/**
 * This macro combines `buf_set` with `buf_next`, it sets
 * element zero and increase the pointer by one element
 * 
 * @param   buffer:char*   The buffer
 * @param   type           The data type of the elements for the data type to cast the buffer to
 * @param   variable:type  The new value of the element
 * @return  :variable      The new value of the element
 */
#define buf_set_next(buffer, type, variable)  \
  buf_set(buffer, type, 0, variable);         \
  buf_next(buffer, type, 1)


/**
 * This macro combines `buf_set` with `buf_next`, it sets
 * element zero and increase the pointer by one element
 * 
 * @param   buffer:char*   The buffer
 * @param   type           The data type of the elements for the data type to cast the buffer to
 * @param   variable:type  Slot to set with the value of the element
 * @return  :variable      The value of the element
 */
#define buf_get_next(buffer, type, variable)  \
  buf_get(buffer, type, 0, variable);         \
  buf_next(buffer, type, 1)


/**
 * Check whether two strings are equal
 * 
 * @param   a:char*  One of the strings
 * @param   b:char*  The other of the strings
 * @return  :int     Whether the strings are equal
 */
#define strequals(a, b)  \
  (strcmp(a, b) == 0)


/**
 * Check whether a string starts with another string
 * 
 * @param   haystack:char*  The string to inspect
 * @param   needle:char*    The string `haystack` should start with
 * @return  :int            Whether `haystack` starts with `needle`
 */
#define startswith(haystack, needle)  \
  (strstr(haystack, needle) == haystack)


/**
 * Set effective user and the effective group to the
 * real user and the real group, respectively. If the
 * group cannot be set, the user till not be set either.
 * 
 * @return  :int  Non-zero on error
 */
#define drop_privileges()                              \
  ((getegid() == getgid() ? 0 : setegid(getgid())) ||  \
   (geteuid() == getuid() ? 0 : seteuid(getuid())))


#endif

