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


#include "config.h"


/*
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
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


/**
 * Wrapper for `clock_gettime` that gets some kind of
 * monotonic time, the exact clock ID is not specified
 * 
 * @param   time_slot:struct timespec*  Pointer to the variable in which to store the time
 * @return  :int                        Zero on sucess, -1 on error
 */
#define monotone(time_slot)  \
  clock_gettime(CLOCK_MONOTONIC, time_slot)


/**
 * Close all file descriptors that satisfies a condition
 * 
 * @param  condition  The condition, it should evaluate the variable `fd`
 */
#define close_files(condition)                                                              \
{                                                                                           \
  DIR* dir = opendir(SELF_FD);                                                              \
  struct dirent* file;                                                                      \
                                                                                            \
  if (dir == NULL)                                                                          \
    perror(*argv); /* Well, that is just unfortunate, but we cannot really do anything. */  \
  else                                                                                      \
    while ((file = readdir(dir)) != NULL)                                                   \
      if (strcmp(file->d_name, ".") && strcmp(file->d_name, ".."))                          \
	{                                                                                   \
	  int fd = atoi(file->d_name);                                                      \
	  if (condition)                                                                    \
	    close(fd);                                                                      \
	}                                                                                   \
                                                                                            \
  closedir(dir);                                                                            \
}


/**
 * Free an array and all elements in an array
 * 
 * @param  array:¿V?*       The array to free
 * @param  elements:size_t  The number of elements, in the array, to free
 * @scope  i:size_t         The variable `i` must be declared as `size_t` and avaiable for use
 */
#define xfree(array, elements)    \
  for (i = 0; i < elements; i++)  \
    free((array)[i]);		  \
  free(array)


/**
 * `malloc` wrapper that returns whether the allocation was not successful
 *  
 * @param   var       The variable to which to assign the allocation
 * @param   elements  The number of elements to allocate
 * @parma   type      The data type of the elements for which to create an allocation
 * @return  :int      Evaluates to true if an only if the allocation failed
 */
#define xmalloc(var, elements, type)  \
  ((var = malloc((elements) * sizeof(type))) == NULL)


/**
 * `calloc` wrapper that returns whether the allocation was not successful
 *  
 * @param   var       The variable to which to assign the allocation
 * @param   elements  The number of elements to allocate
 * @parma   type      The data type of the elements for which to create an allocation
 * @return  :int      Evaluates to true if an only if the allocation failed
 */
#define xcalloc(var, elements, type)  \
  ((var = calloc(elements, sizeof(type))) == NULL)


#endif
