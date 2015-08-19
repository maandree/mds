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
#include "util.h"
#include "config.h"
#include "macros.h"

#include <alloca.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/wait.h>
#include <stdint.h>



/**
 * The content of `/proc/self/exe`, when
 * `prepare_reexec` was invoked.
 */
static char self_exe[PATH_MAX] = {0};



/**
 * Convert a client ID string into a client ID integer
 * 
 * @param   str  The client ID string
 * @return       The client ID integer
 */
uint64_t parse_client_id(const char* str)
{
  char client_words[22];
  char* client_high;
  char* client_low;
  uint64_t client;
  
  strcpy(client_high = client_words, str);
  client_low = rawmemchr(client_words, ':');
  *client_low++ = '\0';
  client = atou64(client_high) << 32;
  client |= atou64(client_low);
  
  return client;
}


/**
 * Read an environment variable, but handle it as undefined if empty
 * 
 * @param   var  The environment variable's name
 * @return       The environment variable's value, `NULL` if empty or not defined
 */
char* getenv_nonempty(const char* var)
{
  char* rc = getenv(var);
  if ((rc == NULL) || (*rc == '\0'))
    return NULL;
  return rc;
}


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
 * @return  Zero on success, -1 on error
 */
int prepare_reexec(void)
{
  ssize_t len;
  len = readlink(SELF_EXE, self_exe, (sizeof(self_exe) / sizeof(char)) - 1);
  fail_if (len < 0);
  /* ‘readlink() does not append a null byte to buf.’ */
  self_exe[len] = '\0';
  /* Handle possible race condition: file was removed. */
  if (access(self_exe, F_OK) < 0)
    if (!strcmp(self_exe + (len - 10), " (deleted)"))
      self_exe[len - 10] = '\0';
  return 0;
 fail:
  return -1;
}


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
void reexec_server(int argc, char** argv, int reexeced)
{
  char** reexec_args;
  char** reexec_args_;
  int i;
  
  /* Re-exec the server. */
  reexec_args = alloca(((size_t)argc + 2) * sizeof(char*));
  reexec_args_ = reexec_args;
  if (reexeced == 0)
    {
      *reexec_args_++ = *argv;
      fail_if (xstrdup(*reexec_args_, "--re-exec"));
      for (i = 1; i < argc; i++)
	reexec_args_[i] = argv[i];
    }
  else /* Don't let the --re-exec:s accumulate. */
    *reexec_args_ = *argv;
  for (i = 1; i < argc; i++)
    reexec_args_[i] = argv[i];
  reexec_args_[argc] = NULL;
  execv(self_exe[0] ? self_exe : argv[0], reexec_args);
 fail:
  return;
}


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
int xsigaction(int signo, void (*function)(int signo))
{
  struct sigaction action;
  sigset_t sigset;
  
  sigemptyset(&sigset);
  action.sa_handler = function;
  action.sa_mask = sigset;
  action.sa_flags = 0;
  
  return sigaction(signo, &action, NULL);
}


/**
 * Send a message over a socket
 * 
 * @param   socket   The file descriptor of the socket
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           The number of bytes that have been sent (even on error)
 */
size_t send_message(int socket, const char* message, size_t length)
{
  size_t block_size = length;
  size_t sent = 0;
  ssize_t just_sent;
  
  errno = 0;
  while (length > 0)
    if ((just_sent = send(socket, message + sent, min(block_size, length), MSG_NOSIGNAL)) < 0)
      {
	if (errno == EMSGSIZE)
	  {
	    block_size >>= 1;
	    if (block_size == 0)
	      return sent;
	  }
	else
	  return sent;
      }
    else
      {
	sent += (size_t)just_sent;
	length -= (size_t)just_sent;
      }
  
  return sent;
}


/**
 * A version of `atoi` that is strict about the syntax and bounds
 * 
 * @param   str    The text to parse
 * @param   value  Slot in which to store the value
 * @param   min    The minimum accepted value
 * @param   max    The maximum accepted value
 * @return         Zero on success, -1 on syntax error
 */
int strict_atoi(const char* str, int* value, int min, int max)
{
  long long int r;
  char* endptr;
  
  r = strtoll(str, &endptr, 10);
  if ((*str == '\0') || isspace(*str) ||
      (endptr - str != (ssize_t)strlen(str)) ||
      (r < (long long int)min) ||
      (r > (long long int)max))
    return -1;
  
  *value = (int)r;
  return 0;
}


/**
 * Send a buffer into a file and ignore interruptions
 * 
 * @param   fd      The file descriptor
 * @param   buffer  The buffer
 * @param   length  The length of the buffer
 * @return          Zero on success, -1 on error
 */
int full_write(int fd, const char* buffer, size_t length)
{
  ssize_t wrote;
  while (length > 0)
    {
      errno = 0;
      wrote = write(fd, buffer, length);
      fail_if (errno && (errno != EINTR));
      length -= (size_t)max(wrote, 0);
      buffer += (size_t)max(wrote, 0);
    }
  return 0;
 fail:
  return -1;
}


/**
 * Read a file completely and ignore interruptions
 * 
 * @param   fd      The file descriptor
 * @param   length  Output parameter for the length of the file, may be `NULL`
 * @return          The content of the file, you will need to free it. `NULL` on error.
 */
char* full_read(int fd, size_t* length)
{
  char* old_buf = NULL;
  size_t buffer_size = 8 << 10;
  size_t buffer_ptr = 0;
  char* buffer;
  ssize_t got;
  int saved_errno;
  
  if (length != NULL)
    *length = 0;
  
  /* Allocate buffer for data. */
  fail_if (xmalloc(buffer, buffer_size, char));
  
  /* Read the file. */
  for (;;)
    {
      /* Grow buffer if it is too small. */
      if (buffer_size == buffer_ptr)
	fail_if (xxrealloc(old_buf, buffer, buffer_size <<= 1, char));
      
      /* Read from the file into the buffer. */
      got = read(fd, buffer + buffer_ptr, buffer_size - buffer_ptr);
      fail_if ((got < 0) && (errno != EINTR));
      if (got == 0)
	break;
      buffer_ptr += (size_t)got;
    }
  
  if (length != NULL)
    *length = buffer_ptr;
  return buffer;
 fail:
  saved_errno = errno;
  free(old_buf);
  free(buffer);
  return errno = saved_errno, NULL;
}


/**
 * Send a full message even if interrupted
 * 
 * @param   socket   The file descriptor for the socket to use
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Zero on success, -1 on error
 */
int full_send(int socket, const char* message, size_t length)
{
  size_t sent;
  
  while (length > 0)
    {
      sent = send_message(socket, message, length);
      fail_if ((sent < length) && (errno != EINTR));
      message += sent;
      length -= sent;
    }
  return 0;
 fail:
  return -1;
}


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
int startswith_n(const char* haystack, const char* needle, size_t haystack_n, size_t needle_n)
{
  size_t i;
  if (haystack_n < needle_n)
    return 0;
  
  for (i = 0; i < needle_n; i++)
    if (haystack[i] != needle[i])
      return 0;
  
  return 1;
}


/**
 * Wrapper around `waitpid` that never returns on an interruption unless
 * it is interrupted 100 times within the same second
 * 
 * @param   pid      See description of `pid` in the documentation for `waitpid`
 * @param   status   See description of `status` in the documentation for `waitpid`
 * @param   options  See description of `options` in the documentation for `waitpid`
 * @return           See the documentation for `waitpid`
 */
pid_t uninterruptable_waitpid(pid_t pid, int* restrict status, int options)
{
  struct timespec time_start;
  struct timespec time_intr;
  int intr_count = 0, have_time;
  pid_t rc;
  
  have_time = (monotone(&time_start) >= 0);
  
 rewait:
  rc = waitpid(pid, status, options);
  if (rc == (pid_t)-1)
    {
      fail_if (errno != EINTR);
      if (have_time && (monotone(&time_intr) >= 0))
	if (time_start.tv_sec != time_intr.tv_sec)
	  intr_count = 0;
      if (intr_count++ < 100)
	goto rewait;
      /* Don't let the CPU catch fire! */
      errno = EINTR;
    }
 fail:
  return rc;
}


/**
 * Check whether a NUL-terminated string is encoded in UTF-8
 * 
 * @param   string              The string
 * @param   allow_modified_nul  Whether Modified UTF-8 is allowed, which allows a two-byte encoding for NUL
 * @return                      Zero if good, -1 on encoding error
 */
int verify_utf8(const char* string, int allow_modified_nul)
{
  static long BYTES_TO_MIN_BITS[] = {0, 0,  8, 12, 17, 22, 37};
  static long BYTES_TO_MAX_BITS[] = {0, 7, 11, 16, 21, 26, 31};
  long bytes = 0, read_bytes = 0, bits = 0, c, character;
  
  /*                                                      min bits  max bits
    0.......                                                 0         7
    110..... 10......                                        8        11
    1110.... 10...... 10......                              12        16
    11110... 10...... 10...... 10......                     17        21
    111110.. 10...... 10...... 10...... 10......            22        26
    1111110. 10...... 10...... 10...... 10...... 10......   27        31
   */
  
  while ((c = (long)(*string++)))
    if (read_bytes == 0)
      {
	/* First byte of the character. */
	
	if ((c & 0x80) == 0x00)
	  /* Single-byte character. */
	  continue;
	
	if ((c & 0xC0) == 0x80)
	  /* Single-byte character marked as multibyte, or
	     a non-first byte in a multibyte character. */
	  return -1;
	
	/* Multibyte character. */
	while ((c & 0x80))
	  bytes++, c <<= 1;
	read_bytes = 1;
	character = c & 0x7F;
	if (bytes > 6)
	  /* 31-bit characters can be encoded with 6-bytes,
	     and UTF-8 does not cover higher code points. */
	  return -1;
      }
    else
      {
	/* Not first byte of the character. */
	
	if ((c & 0xC0) != 0x80)
	  /* Beginning of new character before a
	     multibyte character has ended. */
	  return -1;
	
	character = (character << 6) | (c & 0x7F);
	
	if (++read_bytes < bytes)
	  /* Not at last byte yet. */
	  continue;
	
	/* Check that the character is not unnecessarily long. */
	while (character)
	  character >>= 1, bits++;
	bits = ((bits == 0) && (bytes == 2) && allow_modified_nul) ? 8 : bits;
	if ((bits < BYTES_TO_MIN_BITS[bytes]) || (BYTES_TO_MAX_BITS[bytes] < bits))
	  return -1;
	
	read_bytes = bytes = bits = 0;
      }
  
  /* Make sure we did not stop at the middle of a multibyte character. */
  return read_bytes == 0 ? 0 : -1;
}

