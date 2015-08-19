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
#include "mds-colour.h"
/* TODO reload settings on SIGUSR2 */
/* TODO --save-changes */

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/hash-help.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define reconnect_to_display() -1



#define MDS_COLOUR_VARS_VERSION  0



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
    .danger_is_deadly = 0
  };



/**
 * Value of the ‘Message ID’ header for the next message
 */
static uint32_t message_id = 1;

/**
 * Buffer for received messages
 */
static mds_message_t received;

/**
 * Whether the server is connected to the display
 */
static int connected = 1;

/**
 * Buffer for sending messages
 */
static char* send_buffer = NULL;

/**
 * The size allocated to `send_buffer` divided by `sizeof(char)`
 */
static size_t send_buffer_size = 0;

/**
 * List of all colours
 */
static colour_slot_t* colour_list = NULL;



/**
 * Send a full message even if interrupted
 * 
 * @param   message:const char*  The message to send
 * @param   length:size_t        The length of the message
 * @return  :int                 Zero on success, -1 on error
 */
#define full_send(message, length)  \
  ((full_send)(socket_fd, message, length))


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
  int stage = 0;
  const char* const message =
    "Command: intercept\n"
    "Message ID: 0\n"
    "Length: 62\n"
    "\n"
    "Command: list-colours\n"
    "Command: get-colour\n"
    "Command: set-colour\n";
  
  fail_if (full_send(message, strlen(message)));
  fail_if (server_initialised() < 0);  stage++;
  fail_if (mds_message_initialise(&received));
  
  return 0;
 fail:
  xperror(*argv);
  if (stage >= 1)
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
  
  fail_if (reconnect_to_display());
  connected = 1;
  return 0;
 fail:
  mds_message_destroy(&received);
  return 1;
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
  size_t rc = 2 * sizeof(int) + sizeof(uint32_t);
  rc += mds_message_marshal_size(&received);
  return rc;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  buf_set_next(state_buf, int, MDS_COLOUR_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, uint32_t, message_id);
  
  mds_message_marshal(&received, state_buf);
  state_buf += mds_message_marshal_size(&received) / sizeof(char*);
  
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
  /* buf_get_next(state_buf, int, MDS_COLOUR_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, uint32_t, message_id);
  fail_if (mds_message_unmarshal(&received, state_buf));
  return 0;
 fail:
  xperror(*argv);
  mds_message_destroy(&received);
  return -1;
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
  int rc = 1, r;
  
  while (!reexecing && !terminating)
    {
      if (danger)
	{
	  danger = 0;
	  free(send_buffer), send_buffer = NULL;
	  send_buffer_size = 0;
	  colour_list_pack(&colour_list);
	}
      
      if (r = mds_message_read(&received, socket_fd), r == 0)
	if (r = handle_message(), r == 0)
	  continue;
      
      if (r == -2)
	{
	  eprint("corrupt message received, aborting.");
	  goto done;
	}
      else if (errno == EINTR)
	continue;
      else
	fail_if (errno != ECONNRESET);
      
      eprint("lost connection to server.");
      mds_message_destroy(&received);
      mds_message_initialise(&received);
      connected = 0;
      fail_if (reconnect_to_display());
      connected = 1;
    }
  
  rc = 0;
  goto done;
 fail:
  xperror(*argv);
 done:
  if (rc || !reexecing)
    mds_message_destroy(&received);
  free(send_buffer);
  return rc;
}


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void)
{
  const char* recv_command = NULL;
  const char* recv_client_id = "0:0";
  const char* recv_message_id = NULL;
  const char* recv_include_values = NULL;
  const char* recv_name = NULL;
  const char* recv_remove = NULL;
  const char* recv_bytes = NULL;
  const char* recv_red = NULL;
  const char* recv_green = NULL;
  const char* recv_blue = NULL;
  size_t i;
  
#define __get_header(storage, header)			\
  (startswith(received.headers[i], header))		\
    storage = received.headers[i] + strlen(header)
  
  for (i = 0; i < received.header_count; i++)
    {
      if      __get_header(recv_command,        "Command: ");
      else if __get_header(recv_client_id,      "Client ID: ");
      else if __get_header(recv_message_id,     "Message ID: ");
      else if __get_header(recv_include_values, "Include values: ");
      else if __get_header(recv_name,           "Name: ");
      else if __get_header(recv_remove,         "Remove: ");
      else if __get_header(recv_bytes,          "Bytes: ");
      else if __get_header(recv_red,            "Red: ");
      else if __get_header(recv_green,          "Green: ");
      else if __get_header(recv_blue,           "Blue: ");
    }
  
#undef __get_header
  
  if (recv_message_id == NULL)
    {
      eprint("received message without ID, ignoring, master server is misbehaving.");
      return 0;
    }
  
  if (recv_command == NULL)
    return 0; /* How did that get here, not matter, just ignore it? */

#define t(expr)  do { fail_if (expr); return 0; } while (0)
  if (strequals(recv_command, "list-colours"))
    t (handle_list_colours(recv_client_id, recv_message_id, recv_include_values));
  if (strequals(recv_command, "get-colour"))
    t (handle_get_colour(recv_client_id, recv_message_id, recv_name));
  if (strequals(recv_command, "set-colour"))
    t (handle_set_colour(recv_name, recv_remove, recv_bytes, recv_red, recv_green, recv_blue));
#undef t
  
  return 0; /* How did that get here, not matter, just ignore it? */
 fail:
  return -1;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: list-colours`
 * 
 * @param   recv_client_id       The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id      The value of the `Message ID`-header
 * @param   recv_include_values  The value of the `Include values`-header, `NULL` if omitted
 * @return                       Zero on success, -1 on error
 */
int handle_list_colours(const char* recv_client_id, const char* recv_message_id,
			const char* recv_include_values)
{
  int include_values = 0;
  
  if (strequals(recv_client_id, "0:0"))
    return eprint("got a query from an anonymous client, ignoring."), 0;
  
  if      (recv_include_values == NULL)            include_values = 0;
  else if (strequals(recv_include_values, "yes"))  include_values = 1;
  else if (strequals(recv_include_values, "no"))   include_values = 0;
  else
    ; /* TODO send EPROTO*/
  
  /* TODO send list */
  
  return 0;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: get-colour`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_name        The value of the `Name`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_get_colour(const char* recv_client_id, const char* recv_message_id, const char* recv_name)
{
  if (strequals(recv_client_id, "0:0"))
    return eprint("got a query from an anonymous client, ignoring."), 0;
  
  if (recv_name == NULL)
    ; /* TODO send EPROTO */
  
  /* TODO send colour, "not defined" if missing */
  
  return 0;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: set-colour`
 * 
 * @param   recv_name    The value of the `Name`-header, `NULL` if omitted
 * @param   recv_remove  The value of the `Remove`-header, `NULL` if omitted
 * @param   recv_bytes   The value of the `Bytes`-header, `NULL` if omitted
 * @param   recv_red     The value of the `Red`-header, `NULL` if omitted
 * @param   recv_green   The value of the `Green`-header, `NULL` if omitted
 * @param   recv_blue    The value of the `Blue`-header, `NULL` if omitted
 * @return               Zero on success, -1 on error
 */
int handle_set_colour(const char* recv_name, const char* recv_remove, const char* recv_bytes,
		      const char* recv_red, const char* recv_green, const char* recv_blue)
{
  uint64_t limit = UINT64_MAX;
  int remove_colour = 0;
  colour_t colour;
  int bytes;
  
  if      (recv_remove == NULL)            remove_colour = 0;
  else if (strequals(recv_remove, "yes"))  remove_colour = 1;
  else if (strequals(recv_remove, "no"))   remove_colour = 0;
  else
    return eprint("got an invalid value on the Remove-header, ignoring."), 0;
  
  if (recv_name == NULL)
    return eprint("did not get all required headers, ignoring."), 0;
  
  if (remove_colour == 0)
    {
      if ((recv_bytes == NULL) || (recv_red == NULL) || (recv_green == NULL) || (recv_blue == NULL))
	return eprint("did not get all required headers, ignoring."), 0;
      
      if (strict_atoi(recv_bytes, &bytes, 1, 8))
	return eprint("got an invalid value on the Bytes-header, ignoring."), 0;
      if ((bytes != 1) && (bytes != 2) && (bytes != 4) && (bytes != 8))
	return eprint("got an invalid value on the Bytes-header, ignoring."), 0;
      
      if (bytes < 8)
	limit = (((uint64_t)1) << (bytes * 8)) - 1;
      
      colour.bytes = bytes;
      if (strict_atou64(recv_red, &(colour.red), 0, limit))
	return eprint("got an invalid value on the Red-header, ignoring."), 0;
      if (strict_atou64(recv_green, &(colour.green), 0, limit))
	return eprint("got an invalid value on the Green-header, ignoring."), 0;
      if (strict_atou64(recv_blue, &(colour.blue), 0, limit))
	return eprint("got an invalid value on the Blue-header, ignoring."), 0;
      
      /* TODO set colour */
    }
  else
    {
      /* TODO remove colour */
    }
  
  return 0;
}


/**
 * This function is called when a signal that
 * signals that the system to dump state information
 * and statistics has been received
 * 
 * @param  signo  The signal that has been received
 */
void received_info(int signo)
{
  (void) signo;
  iprintf("next message ID: %" PRIu32, message_id);
  iprintf("connected: %s", connected ? "yes" : "no");
  iprintf("send buffer size: %zu bytes", send_buffer_size);
}

