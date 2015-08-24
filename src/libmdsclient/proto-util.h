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
#ifndef MDS_LIBMDSCLIENT_PROTO_UTIL_H
#define MDS_LIBMDSCLIENT_PROTO_UTIL_H



#include <stddef.h>
#include <stdarg.h>



/**
 * Optimisations `libmds_headers_cherrypick` MAY use
 */
typedef enum libmds_cherrypick_optimisation
{
  /**
   * No optimisation is allowed, in particular this
   * means that the array of headers may not be reordered.
   * The function may still create a copy of the header
   * array and sort the copy.
   * 
   * Cannot be combined with `SORT` or `SORTED`
   * 
   * This option is guaranteed to always have the value 0
   */
  DO_NOT_SORT = 0,
  
  /**
   * `libmds_headers_cherrypick` is allowed to
   * sort the header array. There is no guarantee
   * that the header array will be sorted.
   * 
   * Cannot be combined with `DO_NOT_SORT` or `SORTED`
   */
  SORT = 1,
  
  /**
   * Informs `libmds_headers_cherrypick` that the header
   * array already is sorted. `libmds_headers_cherrypick`
   * can use this information to optimise its procedure.
   * But this also means that the header array will not
   * be reordered.
   * 
   * Cannot be combined with `DO_NOT_SORT` or `SORT`
   */
  SORTED = 2,
  
  
  /**
   * The list of requested headers is not sorted
   * in ascending order
   * 
   * Cannot be combined with `ARGS_SORTED`
   * 
   * This option is guaranteed to always have the value 0
   */
  ARGS_UNSORTED = 0,
  
  /**
   * The list of requested headers is sorted in
   * ascending order
   * 
   * Cannot be combined with `ARGS_UNSORTED`
   */
  ARGS_SORTED = 4,
  
  
} libmds_cherrypick_optimisation_t;


/**
 * Cherrypick headers from a message
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   found         Output parameter for the number of found headers of those that
 *                        were requested.
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
			      libmds_cherrypick_optimisation_t optimisation, ...);

/**
 * Cherrypick headers from a message,
 * linear search will be used without optimisations
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
size_t libmds_headers_cherrypick_linear_unsorted(char** restrict headers, size_t header_count, ...);

/**
 * Cherrypick headers from a message,
 * linear search will be used with optimisation based
 * on that the array is sorted as well as the list of
 * requested headers (`...`)
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
size_t libmds_headers_cherrypick_linear_sorted(char** restrict headers, size_t header_count, ...);

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
size_t libmds_headers_cherrypick_binary_unsorted(char** restrict headers, size_t header_count, ...);

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
size_t libmds_headers_cherrypick_binary_sorted(char** restrict headers, size_t header_count, ...);

/**
 * Cherrypick headers from a message
 * 
 * @param   headers       The headers in the message
 * @param   header_count  The number of headers
 * @param   found         Output parameter for the number of found headers of those that
 *                        were requested. `NULL` is permitted.
 * @param   optimisation  Optimisations the function may use
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
				libmds_cherrypick_optimisation_t optimisation, va_list args);

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
size_t libmds_headers_cherrypick_linear_unsorted_v(char** restrict headers, size_t header_count, va_list args);

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
size_t libmds_headers_cherrypick_linear_sorted_v(char** restrict headers, size_t header_count, va_list args);

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
size_t libmds_headers_cherrypick_binary_unsorted_v(char** restrict headers, size_t header_count, va_list args);

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
size_t libmds_headers_cherrypick_binary_sorted_v(char** restrict headers, size_t header_count, va_list args);

/**
 * Sort the a header array, this is what `libmds_headers_cherrypick`
 * uses to optimise its procedure.
 * 
 * @param  headers      The array of headers
 * @param  headr_count  The number of elements in `headers`
 */
void libmds_headers_sort(char** restrict headers, size_t header_count);


#endif

