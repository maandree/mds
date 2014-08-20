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
#include "mds-echo.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define reconnect_to_display() -1 /* TODO */



#define MDS_ECHO_VARS_VERSION  0



/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 0,
    .require_display = 1,
    .require_respawn_info = 0,
    .sanity_check_argc = 1,
    .fork_for_safety = 0,
    .danger_is_deadly = 1
  };



/**
 * Value of the ‘Message ID’ header for the next message
 */
static int32_t message_id = 1;

/**
 * Buffer for received messages
 */
static mds_message_t received;

/**
 * Whether the server is connected to the display
 */
static int connected = 1;

/**
 * Buffer for message echoing
 */
static char* echo_buffer = NULL;

/**
 * The size allocated to `echo_buffer` divided by `sizeof(char)`
 */
static size_t echo_buffer_size = 0;



/**
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) preinitialise_server(void)
{
  return 0;
}


/**
 * This function should initialise the server,
 * and it not invoked after a re-exec.
 * 
 * @return  Non-zero on error
 */
int initialise_server(void)
{
  const char* const message =
    "Command: intercept\n"
    "Message ID: 0\n"
    "Length: 14\n"
    "\n"
    "Command: echo\n";
  
  if (full_send(message, strlen(message)))
    return 1;
  fail_if (server_initialised() < 0);
  fail_if (mds_message_initialise(&received));
  
  return 0;
 pfail:
  xperror(*argv);
  mds_message_destroy(&received);
  return 1;
}


/**
 * This function will be invoked after `initialise_server` (if not re-exec:ing)
 * or after `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int postinitialise_server(void)
{
  if (connected)
    return 0;
  
  if (reconnect_to_display())
    {
      mds_message_destroy(&received);
      return 1;
    }
  connected = 1;
  return 0;
}


/**
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
size_t marshal_server_size(void)
{
  return 2 * sizeof(int) + sizeof(int32_t) + mds_message_marshal_size(&received);
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  buf_set_next(state_buf, int, MDS_ECHO_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, int32_t, message_id);
  mds_message_marshal(&received, state_buf);
  
  mds_message_destroy(&received);
  return 0;
}


/**
 * Unmarshal server implementation specific data and update the servers state accordingly
 * 
 * On critical failure the program should `abort()` or exit by other means.
 * That is, do not let `reexec_failure_recover` run successfully, if it unrecoverable
 * error has occurred or one severe enough that it is better to simply respawn.
 * 
 * @param   state_buf  The marshalled data that as not been read already
 * @return             Non-zero on error
 */
int unmarshal_server(char* state_buf)
{
  int r;
  /* buf_get_next(state_buf, int, MDS_ECHO_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, int32_t, message_id);
  r = mds_message_unmarshal(&received, state_buf);
  if (r)
    {
      xperror(*argv);
      mds_message_destroy(&received);
    }
  return r;
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) reexec_failure_recover(void)
{
  return -1;
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  int rc = 1;
  
  while (!reexecing && !terminating)
    {
      int r = mds_message_read(&received, socket_fd);
      if (r == 0)
	{
	  r = echo_message();
	  if (r == 0)
	    continue;
	}
      
      if (r == -2)
	{
	  eprint("corrupt message received, aborting.");
	  goto fail;
	}
      else if (errno == EINTR)
	continue;
      else if (errno != ECONNRESET)
	goto pfail;
      
      eprint("lost connection to server.");
      mds_message_destroy(&received);
      mds_message_initialise(&received);
      connected = 0;
      if (reconnect_to_display())
	goto fail;
      connected = 1;
    }
  
  rc = 0;
  goto fail;
 pfail:
  xperror(*argv);
 fail:
  if (rc || !reexecing)
    mds_message_destroy(&received);
  free(echo_buffer);
  return rc;
}


/**
 * Echo the received message payload
 * 
 * @return  Zero on success -1 on error or interruption,
 *          errno will be set accordingly
 */
int echo_message(void)
{
  const char* recv_client_id = NULL;
  const char* recv_message_id = NULL;
  const char* recv_length = NULL;
  size_t i, n;
  
  /* Fetch headers. */
  
#define __get_header(storage, header, skip)  \
  (startswith(received.headers[i], header))  \
    storage = received.headers[i] + skip * strlen(header)
  
  for (i = 0; i < received.header_count; i++)
    {
      if      __get_header(recv_client_id,  "Client ID: ",  1);
      else if __get_header(recv_message_id, "Message ID: ", 1);
      else if __get_header(recv_length,     "Length: ",     0);
      else
	continue;
      
      /* Stop fetch headers if we have found everything we want. */
      if (recv_client_id && recv_message_id && recv_length)
	break;
    }
  
#undef __get_header
  
  
  /* Validate headers. */
  if ((recv_client_id == NULL) || (strequals(recv_client_id, "0:0")))
    {
      eprint("received message from anonymous sender, ignoring.");
      return 0;
    }
  else if (recv_message_id == NULL)
    {
      eprint("received message with ID, ignoring, master server is misbehaving.");
      return 0;
    }
  
  /* Construct echo message headers. */
  
  n = 1 + strlen("To: \nIn response to: \nMessage ID: \n\n");
  n += strlen(recv_client_id) + strlen(recv_message_id) + 3 * sizeof(int32_t);
  if (recv_length != NULL)
    n += strlen(recv_length) + 1;
  
  if ((echo_buffer_size < n) || (echo_buffer_size * 4 > n))
    {
      char* old_buffer = echo_buffer;
      if (xrealloc(echo_buffer, echo_buffer_size = n, char))
	{
	  free(old_buffer);
	  return -1;
	}
    }
  
  sprintf(echo_buffer, "To: %s\nIn response to: %s\nMessage ID: %" PRIi32 "\n%s%s\n",
	  recv_client_id, recv_message_id, message_id,
	  recv_length == NULL ? "" : recv_length,
	  recv_length == NULL ? "" : "\n");
  
  /* Increase message ID. */
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  
  /* Send echo. */
  if (full_send(echo_buffer, strlen(echo_buffer)))
    return 1;
  return full_send(received.payload, received.payload_size);
}


/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Non-zero on success
 */
int full_send(const char* message, size_t length)
{
  size_t sent;
  
  while (length > 0)
    {
      sent = send_message(socket_fd, message, length);
      if (sent > length)
	{
	  eprint("Sent more of a message than exists in the message, aborting.");
	  return -1;
	}
      else if ((sent < length) && (errno != EINTR))
	{
	  xperror(*argv);
	  return -1;
	}
      message += sent;
      length -= sent;
    }
  return 0;
}

