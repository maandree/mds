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
#include "proto-util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>



/**
 * The number of headers there must be before
 * it is beneficial to sort them.
 */
#ifndef LIBMDS_HEADERS_SORT_THRESHOLD
# define LIBMDS_HEADERS_SORT_THRESHOLD  100  /* XXX: Value is chosen at random */
#endif

/**
 * The number of headers there must be before
 * it is beneficial to copy them, if that
 * it required to sort them. Note that this
 * value plus that of `LIBMDS_HEADERS_SORT_THRESHOLD`
 * is the threshold for copying and sorting
 * a header array.
 */
#ifndef LIBMDS_HEADERS_COPY_THRESHOLD
# define LIBMDS_HEADERS_COPY_THRESHOLD  10  /* XXX: Value is chosen at random */
#endif

/**
 * The number of headers there must be before
 * it is beneficial to search them using binary
 * search rather than linear search.
 * 
 * Note that hybrid search is not implemented,
 * is is either full binary or full linear.
 */
#ifndef LIBMDS_HEADERS_BINSEARCH_THRESHOLD
# define LIBMDS_HEADERS_BINSEARCH_THRESHOLD  1000  /* XXX: Value is chosen at semirandom */
#endif



/**
 * Variant of `strcmp` that regards the first string as
 * ending at the first occurrence of the substring ": "
 * 
 * @param   header_a  The header that may include a value
 * @param   header_b  The header that may not include a value
 * @return            Same syntax as strcmp(3)
 */
static int headercmp(const char* header_a, const char* header_b)
{
  int a, b;
  for (;;)
    {
      a = (int)*header_a++;
      b = (int)*header_b++;
      
      if ((a == ':') && (*header_a == ' '))
	return 0 - b;
      if ((a == 0) || (b == 0))
	return a - b;
      if (a - b)
	return a - b;
    }
}


/**
 * Variant for `headercmp` where the arguments are pointers to the
 * values that should be compared, and the second parameter is
 * treated the same way as the first parameter
 * 
 * @param   strp_a  Pointer to the first string
 * @param   strp_b  Pointer to the second string
 * @return          Will return `strcmp(*strp_a, *strp_b)`, see strcmp(3)
 */
static int headerpcmp(const void* headerp_a, const void* headerp_b)
{
  const char* header_a = *(char* const*)headerp_a;
  const char* header_b = *(char* const*)headerp_b;
  int a, b, az, bz;
  
  for (;;)
    {
      a = (int)*header_a++;
      b = (int)*header_b++;
      az = (a == ':') && (*header_a == ' ');
      bz = (b == ':') && (*header_b == ' ');
      
      if (az)
	return 0 - !(bz || (b == 0));
      if (bz)
	return a - 0;
      if ((a == 0) || (b == 0))
	return a - b;
      if (a - b)
	return a - b;
    }
}


/**
 * Cherrypick headers from a message
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   found         Output parameter for the number of found headers of those that
 *                        where requested. `NULL` is permitted.
 * @param   optimisation  Optimisations the function may use, only one value please
 * @param   ...           The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                Zero on success, -1 on error, `errno` will have been set
 *                        accordingly on error.
 * 
 * @throws  ENOMEM        Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                        RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_headers_cherrypick(char** restrict headers, size_t header_count, size_t* restrict found,
			      libmds_cherrypick_optimisation_t optimisation, ...)
{
  va_list args;
  int r, saved_errno;
  va_start(args, optimisation);
  r = libmds_headers_cherrypick_v(headers, header_count, found, optimisation, args);
  saved_errno = errno;
  va_end(args);
  errno = saved_errno;
  return r;
}


/**
 * Cherrypick headers from a message,
 * linear search will be used without optimisation
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   ...           The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_linear_unsorted(char** restrict headers, size_t header_count, ...)
{
  va_list args;
  size_t r;
  int saved_errno;
  va_start(args, header_count);
  r = libmds_headers_cherrypick_linear_unsorted_v(headers, header_count, args);
  saved_errno = errno;
  va_end(args);
  errno = saved_errno;
  return r;
}


/**
 * Cherrypick headers from a message,
 * linear search will be used with optimisation based on that the array is sorted
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   ...           The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_linear_sorted(char** restrict headers, size_t header_count, ...)
{
  va_list args;
  size_t r;
  int saved_errno;
  va_start(args, header_count);
  r = libmds_headers_cherrypick_linear_sorted_v(headers, header_count, args);
  saved_errno = errno;
  va_end(args);
  errno = saved_errno;
  return r;
}


/**
 * Cherrypick headers from a message,
 * binary search will be used
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   ...           The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_binary_unsorted(char** restrict headers, size_t header_count, ...)
{
  va_list args;
  size_t r;
  int saved_errno;
  va_start(args, header_count);
  r = libmds_headers_cherrypick_binary_unsorted_v(headers, header_count, args);
  saved_errno = errno;
  va_end(args);
  errno = saved_errno;
  return r;
}


/**
 * Cherrypick headers from a message,
 * binary search will be used with optimisation based on
 * that list of requested headers (`...`)  is sorted
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   ...           The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_binary_sorted(char** restrict headers, size_t header_count, ...)
{
  va_list args;
  size_t r;
  int saved_errno;
  va_start(args, header_count);
  r = libmds_headers_cherrypick_binary_sorted_v(headers, header_count, args);
  saved_errno = errno;
  va_end(args);
  errno = saved_errno;
  return r;
}


/**
 * Cherrypick headers from a message
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   found         Output parameter for the number of found headers of those that
 *                        were requested. `NULL` is permitted.
 * @param   optimisation  Optimisations the function may use, only one value please
 * @param   args          The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                Zero on success, -1 on error, `errno` will have been set
 *                        accordingly on error.
 * 
 * @throws  ENOMEM        Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                        RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_headers_cherrypick_v(char** restrict headers, size_t header_count, size_t* restrict found,
				libmds_cherrypick_optimisation_t optimisation, va_list args)
{
  char** headers_ = headers;
  size_t found_;
  libmds_cherrypick_optimisation_t sorted;
  
  if (found != NULL)
    *found = 0;
  
  optimisation ^= sorted = optimisation & ARGS_SORTED;
  
  if (optimisation == DO_NOT_SORT)
    {
      if (header_count >= LIBMDS_HEADERS_SORT_THRESHOLD + LIBMDS_HEADERS_COPY_THRESHOLD)
	{
	  headers_ = malloc(header_count * sizeof(char*));
	  if (headers_ == NULL)
	    return -1;
	  memcpy(headers_, headers, header_count * sizeof(char*));
	  libmds_headers_sort(headers_, header_count);
	  optimisation = SORTED;
	}
    }
  else if (optimisation == SORT)
    if (header_count >= LIBMDS_HEADERS_SORT_THRESHOLD)
      {
	libmds_headers_sort(headers_, header_count);
	optimisation = SORTED;
      }
  
  if (optimisation != SORTED)
    found_ = libmds_headers_cherrypick_linear_unsorted_v(headers_, header_count, args);
  else if (header_count < LIBMDS_HEADERS_BINSEARCH_THRESHOLD)
    {
      if (sorted)
	found_ = libmds_headers_cherrypick_linear_sorted_v(headers_, header_count, args);
      else
	found_ = libmds_headers_cherrypick_linear_unsorted_v(headers_, header_count, args);
    }
  else if (sorted)
    found_ = libmds_headers_cherrypick_binary_sorted_v(headers_, header_count, args);
  else
    found_ = libmds_headers_cherrypick_binary_unsorted_v(headers_, header_count, args);
  
  if (found != NULL)
    *found = found_;
  
  if (headers_ != headers)
    free(headers_);
  
  return 0;
}


/**
 * Cherrypick headers from a message,
 * linear search will be used without optimisation
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   args          The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_linear_unsorted_v(char** restrict headers, size_t header_count, va_list args)
{
  const char* header;
  char** value_out;
  size_t found = 0, i;
  
  for (;;)
    {
      header = va_arg(args, const char*);
      if (header == NULL)
	return found;
      
      value_out = va_arg(args, char**);
      *value_out = NULL;
      
      for (i = 0; i < header_count; i++)
	if (!headercmp(headers[i], header))
	  {
	    *value_out = strstr(headers[i], ": ") + 2;
	    found++;
	    break;
	  }
    }
}


/**
 * Cherrypick headers from a message,
 * linear search will be used with optimisation based
 * on that the array is sorted as well as the list of
 * requested headers (`args`)
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   args          The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_linear_sorted_v(char** restrict headers, size_t header_count, va_list args)
{
  const char* header;
  char** value_out;
  size_t found = 0, i = 0;
  int r;
  
  for (;;)
    {
      header = va_arg(args, const char*);
      if (header == NULL)
	return found;
      
      value_out = va_arg(args, char**);
      *value_out = NULL;
      
      for (; i < header_count; i++)
	{
	  r = headercmp(headers[i], header);
	  if (r == 0)
	    {
	      *value_out = strstr(headers[i], ": ") + 2;
	      found++, i++;
	      break;
	    }
	  if (r > 0)
	    break;
	}
    }
}


/**
 * Cherrypick headers from a message,
 * binary search will be used
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   args          The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_binary_unsorted_v(char** restrict headers, size_t header_count, va_list args)
{
  const char* header;
  char** value_out;
  void* found_element;
  size_t found = 0;
  
  for (;;)
    {
      header = va_arg(args, const char*);
      if (header == NULL)
	return found;
      
      value_out = va_arg(args, char**);
      *value_out = NULL;
      
      found_element = bsearch(&header, headers, header_count, sizeof(char*), headerpcmp);
      if (found_element != NULL)
	*value_out = strstr(*(char**)found_element, ": ") + 2, found++;
    }
}


/**
 * Cherrypick headers from a message,
 * binary search will be used with optimisation based on
 * that list of requested headers (`args`)  is sorted
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   args          The first argument should be the name of a header, it should
 *                        have the type `const char*`. The second argument should be
 *                        a pointer to the location where the header named by the first
 *                        argument in the argument list names. It should have the type
 *                        `char**`. If the header is found, its value will be stored,
 *                        and it will be a NUL-terminated string. If the header is not
 *                        found, `NULL` will be stored.  The next two arguments is
 *                        interpreted analogously to the first two arguments, and the
 *                        following two in the same way, and so on. When there are no
 *                        more headers in the list, it should be terminated with a `NULL`.
 * @return                The number of found headers of those that were requested
 */
size_t libmds_headers_cherrypick_binary_sorted_v(char** restrict headers, size_t header_count, va_list args)
{
  const char* header;
  char** value_out;
  void* found_element;
  size_t found = 0;
  size_t offset = 0;
  
  /* I sincerely doubt even this amount of optimisation is needed,
   * so there is no need for even faster algorithms, keep in mind
   * that the overhead increases with faster algorithms. */
  
  for (;;)
    {
      header = va_arg(args, const char*);
      if (header == NULL)
	return found;
      
      value_out = va_arg(args, char**);
      *value_out = NULL;
      
      found_element = bsearch(&header, headers + offset, header_count - offset, sizeof(char*), headerpcmp);
      if (found_element != NULL)
	{
	  offset = (size_t)((char**)found_element - headers);
	  *value_out = strstr(*(char**)found_element, ": ") + 2;
	  found++;
	}
    }
}


/**
 * Sort the a header array, this is what `libmds_headers_cherrypick`
 * uses to optimise its procedure.
 * 
 * @param  headers      The array of headers
 * @param  headr_count  The number of elements in `headers`
 */
void libmds_headers_sort(char** restrict headers, size_t header_count)
{
  qsort(headers, header_count, sizeof(char*), headerpcmp);
}

