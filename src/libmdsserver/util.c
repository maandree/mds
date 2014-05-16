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
 * Re-exec the server.
 * This function only returns on failure.
 * 
 * @param  argc      The number of elements in `argv`
 * @param  argv      The command line arguments
 * @param  reexeced  Whether the server has previously been re-exec:ed
 */
void reexec_server(int argc, char** argv, int reexeced)
{
  char readlink_buf[PATH_MAX];
  ssize_t readlink_ptr;
  char** reexec_args;
  char** reexec_args_;
  int i;
  
  /* Re-exec the server. */
  readlink_ptr = readlink(SELF_EXE, readlink_buf, (sizeof(readlink_buf) / sizeof(char)) - 1);
  if (readlink_ptr < 0)
    return;
  /* ‘readlink() does not append a null byte to buf.’ */
  readlink_buf[readlink_ptr] = '\0';
  reexec_args = alloca(((size_t)argc + 2) * sizeof(char*));
  reexec_args_ = reexec_args;
  if (reexeced == 0)
    {
      *reexec_args_++ = *argv;
      *reexec_args_ = strdup("--re-exec");
      if (*reexec_args_ == NULL)
	return;
      for (i = 1; i < argc; i++)
	reexec_args_[i] = argv[i];
    }
  else /* Don't let the --re-exec:s accumulate. */
    *reexec_args_ = *argv;
  for (i = 1; i < argc; i++)
    reexec_args_[i] = argv[i];
  reexec_args_[argc] = NULL;
  execv(readlink_buf, reexec_args);
}


/**
 * Set up a signal trap.
 * This function should only be used for common mds
 * signals, and this function may choose to add
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
      if (errno && (errno != EINTR))
	return -1;
      length -= (size_t)max(wrote, 0);
      buffer += (size_t)max(wrote, 0);
    }
  return 0;
}


/**
 * Read a file completly and ignore interruptions
 * 
 * @param   fd  The file descriptor
 * @return      The content of the file, you will need to free it. `NULL` on error.
 */
char* full_read(int fd)
{
  size_t state_buf_size = 8 << 10;
  size_t state_buf_ptr = 0;
  char* state_buf;
  ssize_t got;
  
  /* Allocate buffer for data. */
  if (xmalloc(state_buf, state_buf_size, char))
    return NULL;
  
  /* Read the file. */
  for (;;)
    {
      /* Grow buffer if it is too small. */
      if (state_buf_size == state_buf_ptr)
	{
	  char* old_buf = state_buf;
	  state_buf = realloc(state_buf, (state_buf_size <<= 1) * sizeof(char));
	  if (state_buf == NULL)
	    {
	      free(old_buf);
	      return NULL;
	    }
	}
      
      /* Read from the file into the buffer. */
      got = read(fd, state_buf + state_buf_ptr, state_buf_size - state_buf_ptr);
      if ((got < 0) && (errno != EINTR))
	{
	  free(state_buf);
	  return NULL;
	}
      if (got == 0)
	break;
      state_buf_ptr += (size_t)got;
    }
  
  return state_buf;
}

