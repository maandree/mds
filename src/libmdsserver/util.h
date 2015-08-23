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
#ifndef MDS_LIBMDSSERVER_UTIL_H
#define MDS_LIBMDSSERVER_UTIL_H


#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


/**
 * Convert a client ID string into a client ID integer
 * 
 * @param   str  The client ID string
 * @return       The client ID integer
 */
uint64_t parse_client_id(const char* str);

/**
 * Read an environment variable, but handle it as undefined if empty
 * 
 * @param   var  The environment variable's name
 * @return       The environment variable's value, `NULL` if empty or not defined
 */
char* getenv_nonempty(const char* var);

/**
 * Prepare the server so that it can reexec into
 * a newer version of the executed file.
 * 
 * This is required for two reasons:
 * 1:  We cannot use argv[0] as PATH-resolution may
 *     cause it to reexec into another pathname, and
 *     maybe to wrong program. Additionally argv[0]
 *     may not even refer to the program, and chdir
 *     could also hinter its use.
 * 2:  The kernel appends ` (deleted)` to
 *     `/proc/self/exe` once it has been removed,
 *     so it cannot be replaced.
 * 
 * The function will should be called immediately, it
 * will store the content of `/proc/self/exe`.
 * 
 * @return  Zero on success, -1 on error
 */
int prepare_reexec(void);

/**
 * Re-exec the server.
 * This function only returns on failure.
 * 
 * If `prepare_reexec` failed or has not been called,
 * `argv[0]` will be used as a fallback.
 * 
 * @param  argc      The number of elements in `argv`
 * @param  argv      The command line arguments
 * @param  reexeced  Whether the server has previously been re-exec:ed
 */
void reexec_server(int argc, char** argv, int reexeced);

/**
 * Set up a signal trap.
 * This function should only be used for common mds
 * signals and signals that does not require and
 * special settings. This function may choose to add
 * additional behaviour depending on the signal, such
 * as blocking other signals.
 * 
 * @param   signo     The signal to trap
 * @param   function  The function to run when the signal is caught
 * @return            Zero on success, -1 on error
 */
int xsigaction(int signo, void (*function)(int signo));

/**
 * Send a message over a socket
 * 
 * @param   socket   The file descriptor of the socket
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           The number of bytes that have been sent (even on error)
 */
size_t send_message(int socket, const char* message, size_t length);

/**
 * A version of `atoi` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoi(const char* str, int* value, int min, int max);

/**
 * A version of `atoj` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoj(const char* str, intmax_t* value, intmax_t min, intmax_t max);

/**
 * A version of `atouj` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atouj(const char* str, uintmax_t* value, uintmax_t min, uintmax_t max);

/**
 * A version of `atoh` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoh(const char* str, short int* value, int min, int max);

/**
 * A version of `atouh` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atouh(const char* str, unsigned short int* value, unsigned int min, unsigned int max);

/**
 * A version of `atou` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atou(const char* str, unsigned int* value, unsigned int min, unsigned int max);

/**
 * A version of `atol` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atol(const char* str, long int* value, long int min, long int max);

/**
 * A version of `atoul` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoul(const char* str, unsigned long int* value, unsigned long int min, unsigned long int max);

/**
 * A version of `atoll` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoll(const char* str, long long int* value, long long int min, long long int max);

/**
 * A version of `atoull` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoull(const char* str, unsigned long long int* value,
		  unsigned long long int min, unsigned long long int max);

/**
 * A version of `atoz` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoz(const char* str, size_t* value, size_t min, size_t max);

/**
 * A version of `atosz` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atosz(const char* str, ssize_t* value, ssize_t min, ssize_t max);

/**
 * A version of `ato8` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_ato8(const char* str, int8_t* value, int min, int max);

/**
 * A version of `atou8` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atou8(const char* str, uint8_t* value, int min, int max);

/**
 * A version of `ato16` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_ato16(const char* str, int16_t* value, int min, int max);

/**
 * A version of `atou16` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atou16(const char* str, uint16_t* value, unsigned int min, unsigned int max);

/**
 * A version of `ato32` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_ato32(const char* str, int32_t* value, int32_t min, int32_t max);

/**
 * A version of `atou32` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atou32(const char* str, uint32_t* value, uint32_t min, uint32_t max);

/**
 * A version of `ato64` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_ato64(const char* str, int64_t* value, int64_t min, int64_t max);

/**
 * A version of `atou64` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atou64(const char* str, uint64_t* value, uint64_t min, uint64_t max);

/**
 * Send a buffer into a file and ignore interruptions
 * 
 * @param   fd      The file descriptor
 * @param   buffer  The buffer
 * @param   length  The length of the buffer
 * @return          Zero on success, -1 on error
 */
int full_write(int fd, const char* buffer, size_t length);

/**
 * Read a file completely and ignore interruptions
 * 
 * @param   fd      The file descriptor
 * @param   length  Output parameter for the length of the file, may be `NULL`
 * @return          The content of the file, you will need to free it. `NULL` on error
 */
char* full_read(int fd, size_t* length);

/**
 * Send a full message even if interrupted
 * 
 * @param   socket   The file descriptor for the socket to use
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Zero on success, -1 on error
 */
int full_send(int socket, const char* message, size_t length);

/**
 * Check whether a string begins with a specific string,
 * where neither of the strings are necessarily NUL-terminated
 * 
 * @param   haystack    The string that should start with the other string
 * @param   needle      The string the first string should start with
 * @param   haystack_n  The length of `haystack`
 * @param   needle_n    The length of `needle`
 * @return              Whether the `haystack` begins with `needle`
 */
int startswith_n(const char* haystack, const char* needle, size_t haystack_n, size_t needle_n) __attribute__((pure));

/**
 * Wrapper around `waitpid` that never returns on an interruption unless
 * it is interrupted 100 times within the same second
 * 
 * @param   pid      See description of `pid` in the documentation for `waitpid`
 * @param   status   See description of `status` in the documentation for `waitpid`
 * @param   options  See description of `options` in the documentation for `waitpid`
 * @return           See the documentation for `waitpid`
 */
pid_t uninterruptable_waitpid(pid_t pid, int* restrict status, int options);

/**
 * Check whether a NUL-terminated string is encoded in UTF-8
 * 
 * @param   string              The string
 * @param   allow_modified_nul  Whether Modified UTF-8 is allowed, which allows a two-byte encoding for NUL
 * @return                      Zero if good, -1 on encoding error
 */
int verify_utf8(const char* string, int allow_modified_nul) __attribute__((pure));

/**
 * Construct an error message to be sent to a client
 * 
 * @param   recv_client_id    The client ID attached on the message that was received, must not be `NULL`
 * @param   recv_message_id   The message ID attached on the message that was received, must not be `NULL`
 * @param   custom            Non-zero if the error is a custom error
 * @param   errnum            The error number, `errno` should be used if the error
 *                            is not a custom error, zero should be used on success,
 *                            a negative integer, such as -1, indicates that the error
 *                            message should not include an error number,
 *                            it is not allowed to have this value be negative and
 *                            `custom` be zero at the same time
 * @param   message           The description of the error, the line feed at the end
 *                            is added automatically, `NULL` if the description should
 *                            be omitted
 * @param   send_buffer       Pointer to the buffer where the message should be stored,
 *                            it should contain the current send buffer, must not be `NULL`
 * @param   send_buffer_size  Pointer to the allocation size of `*send_buffer`, it should
 *                            contain the current size of `*send_buffer` and will be updated
 *                            with the new size, must not be `NULL`
 * @return                    The length of the message, zero on error
 */
size_t construct_error_message(const char* restrict recv_client_id, const char* restrict recv_message_id,
			       int custom, int errnum, const char* restrict message, char** restrict send_buffer,
			       size_t* restrict send_buffer_size) __attribute__((nonnull(1, 2, 6, 7)));
/* TODO document in info manual */

/**
 * Send an error message
 * 
 * @param   recv_client_id    The client ID attached on the message that was received, must not be `NULL`
 * @param   recv_message_id   The message ID attached on the message that was received, must not be `NULL`
 * @param   custom            Non-zero if the error is a custom error
 * @param   errnum            The error number, `errno` should be used if the error
 *                            is not a custom error, zero should be used on success,
 *                            a negative integer, such as -1, indicates that the error
 *                            message should not include an error number,
 *                            it is not allowed to have this value be negative and
 *                            `custom` be zero at the same time
 * @param   message           The description of the error, the line feed at the end
 *                            is added automatically, `NULL` if the description should
 *                            be omitted
 * @param   send_buffer       Pointer to the buffer where the message should be stored,
 *                            it should contain the current send buffer, must not be `NULL`
 * @param   send_buffer_size  Pointer to the allocation size of `*send_buffer`, it should
 *                            contain the current size of `*send_buffer` and will be updated
 *                            with the new size, must not be `NULL`
 * @param   socket_fd         The file descriptor of the socket
 * @return                    Zero on success, -1 on error
 */
int send_error(const char* restrict recv_client_id, const char* restrict recv_message_id,
	       int custom, int errnum, const char* restrict message, char** restrict send_buffer,
	       size_t* restrict send_buffer_size, int socket_fd) __attribute__((nonnull(1, 2, 6, 7)));
/* TODO document in info manual */


#endif

