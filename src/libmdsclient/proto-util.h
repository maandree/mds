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
#include <stdint.h>



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
#define libmds_headers_cherrypick_linear_unsorted  libmds_headers_cherrypick_linear_unsorted

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
#define libmds_headers_cherrypick_linear_sorted  libmds_headers_cherrypick_linear_sorted

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
#define libmds_headers_cherrypick_binary_unsorted  libmds_headers_cherrypick_binary_unsorted

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
#define libmds_headers_cherrypick_binary_unsorted  libmds_headers_cherrypick_binary_unsorted

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
#define libmds_headers_cherrypick_linear_unsorted_v  libmds_headers_cherrypick_linear_unsorted_v

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
#define libmds_headers_cherrypick_linear_sorted_v  libmds_headers_cherrypick_linear_sorted_v

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
#define libmds_headers_cherrypick_binary_unsorted_v  libmds_headers_cherrypick_binary_unsorted_v

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
#define libmds_headers_cherrypick_binary_sorted_v  libmds_headers_cherrypick_binary_sorted_v

/**
 * Sort the a header array, this is what `libmds_headers_cherrypick`
 * uses to optimise its procedure.
 * 
 * @param  headers      The array of headers
 * @param  headr_count  The number of elements in `headers`
 */
void libmds_headers_sort(char** restrict headers, size_t header_count);

/**
 * Compose a message
 * 
 * @param   buffer          Pointer to the buffer where the message should be written,
 *                          may point to `NULL` if `buffer_size` points to zero, but the
 *                          pointer itself must not be `NULL`. The buffer may be reallocated.
 *                          Will be updated with the new buffer it is allocated.
 *                          The buffer the pointer points to will be filled with the
 *                          message, however there is no guarantee it will be NUL-terminated.
 * @param   buffer_size     The allocation size of, in `char` `*buffer`, if and only if `buffer`
 *                          points to `NULL`, this point should point to zero, and vice versa.
 *                          Must not be `NULL`. Will be update with the new allocation size.
 *                          It is guaranteed that there will be remove to NUL-terminate
 *                          the message even if it was already NUL-terminate (terminate it
 *                          doubly in that case.)
 * @param   length          Output parameter for the length of the constructed message.
 *                          Must not be `NULL`.
 * @param   payload         The payload that should be added to the message, it should end
 *                          with a LF. `NULL` if the message should not have a payload.
 * @param   payload_length  Pointer to the length of the payload. If `NULL`, `payload`
 *                          must be NUL-terminated, and if it is `NULL` this NUL-termination
 *                          will be used to find the length of the payload. This variable is
 *                          not used if `payload` is `NULL`.
 * @param   ...             The first argument should be a line describing the first header,
 *                          it should be a printf(3)-formatted line that fully describes the
 *                          header line, that is, the header name, colon, blank space and then
 *                          the header's value. No LF should be included. The following
 *                          arguments should be the argument to format the header line.
 *                          If a format for a line begins with a question mark, the remainder
 *                          of the line used, but only if the next argument is non-zero
 *                          (it should be of type `int`,) that argument will not be used
 *                          for the formatting. This may be be iterated any number of this.
 *                          The last argument should be `NULL` to specify that there are no
 *                          more headers. The `Length`-header should not be included, it is
 *                          added automatically. A header may not have a length larger than
 *                          2¹⁵, otherwise the behaviour of this function is undefined.
 * @return                  Zero on success, -1 on error, `errno` will have been set
 *                          accordingly on error.
 * 
 * @throws  ENOMEM          Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                          RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_compose(char** restrict buffer, size_t* restrict buffer_size, size_t* restrict length,
		   const char* restrict payload, const size_t* restrict payload_length,
		   ...) __attribute__((nonnull(1, 2, 3)));

/**
 * Compose a message
 * 
 * @param   buffer          Pointer to the buffer where the message should be written,
 *                          may point to `NULL` if `buffer_size` points to zero, but the
 *                          pointer itself must not be `NULL`. The buffer may be reallocated.
 *                          Will be updated with the new buffer it is allocated.
 *                          The buffer the pointer points to will be filled with the
 *                          message, however there is no guarantee it will be NUL-terminated.
 * @param   buffer_size     The allocation size, in `char` of `*buffer`, if and only if `buffer`
 *                          points to `NULL`, this point should point to zero, and vice versa.
 *                          Must not be `NULL`. Will be update with the new allocation size.
 *                          It is guaranteed that there will be remove to NUL-terminate
 *                          the message even if it was already NUL-terminate (terminate it
 *                          doubly in that case.)
 * @param   length          Output parameter for the length of the constructed message.
 *                          Must not be `NULL`.
 * @param   payload         The payload that should be added to the message, it should end
 *                          with a LF. `NULL` if the message should not have a payload.
 * @param   payload_length  Pointer to the length of the payload. If `NULL`, `payload`
 *                          must be NUL-terminated, and if it is `NULL` this NUL-termination
 *                          will be used to find the length of the payload. This variable is
 *                          not used if `payload` is `NULL`.
 * @param   args            The first argument should be a line describing the first header,
 *                          it should be a printf(3)-formatted line that fully describes the
 *                          header line, that is, the header name, colon, blank space and then
 *                          the header's value. No LF should be included. The following
 *                          arguments should be the argument to format the header line.
 *                          If a format for a line begins with a question mark, the remainder
 *                          of the line used, but only if the next argument is non-zero
 *                          (it should be of type `int`,) that argument will not be used
 *                          for the formatting. This may be be iterated any number of this.
 *                          The last argument should be `NULL` to specify that there are no
 *                          more headers. The `Length`-header should not be included, it is
 *                          added automatically. A header may not have a length larger than
 *                          2¹⁵, otherwise the behaviour of this function is undefined.
 * @return                  Zero on success, -1 on error, `errno` will have been set
 *                          accordingly on error.
 * 
 * @throws  ENOMEM          Out of memory, Possibly, the process hit the RLIMIT_AS or
 *                          RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_compose_v(char** restrict buffer, size_t* restrict buffer_size, size_t* restrict length,
		     const char* restrict payload, const size_t* restrict payload_length,
		     va_list args) __attribute__((nonnull(1, 2, 3)));


/**
 * Increase the message ID counter
 * 
 * @param   message_id  Pointer to the current message ID, will be update with
 *                      the next free message ID. Must not be `NULL`.
 * @param   test        Function that tests whether an message ID is free,
 *                      it takes the message ID to test as its first argument,
 *                      and `data` as its second argument, and returns zero if
 *                      it is in used, a positive integer if it is free, and a
 *                      negative integer if an error occurred. `NULL` if there
 *                      should be no testing.
 * @param   data        Argument to pass to `test` so that it can deal with
 *                      threading or any other issues. Unused if `test` is `NULL`.
 * @return              Zero on success, -1 on error, `errno` will have been set
 *                      accordingly on error.
 * 
 * @throws  EAGAIN      If there are no free message ID to be used.
 *                      It is advisable to make `test` throw this immediately if
 *                      there are no free message ID:s.
 * @throws              Any error that `test` may throw.
 */
int libmds_next_message_id(uint32_t* restrict message_id, int (*test)(uint32_t message_id, void* data),
			   void* data) __attribute__((nonnull(1)));


#endif

