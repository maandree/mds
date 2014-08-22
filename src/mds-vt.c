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
#include "mds-vt.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#define reconnect_to_display() -1 /* TODO */



#define MDS_VT_VARS_VERSION  0



/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 1, /* we want to open the new tty (the rest is fine without root for some reason) */
    .require_display = 1,
    .require_respawn_info = 0,
    .sanity_check_argc = 1,
    .fork_for_safety = 0,
    .danger_is_deadly = 0
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
  buf_set_next(state_buf, int, MDS_VT_VARS_VERSION);
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
  /* buf_get_next(state_buf, int, MDS_VT_VARS_VERSION); */
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
	  r = 0; /* FIXME */
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
  return rc;
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


/**
 * Get the index of the next available virtual terminal
 * 
 * @return  -1 on error, 0 if the terminals are exhausted, otherwise the next terminal
 */
int vt_get_next_available(void)
{
  int next_vt = -1;
  int r = ioctl(STDIN_FILENO, VT_OPENQRY, &next_vt);
  if (r < 0)
    return r;
  return ((next_vt < 0) || (MAX_NR_CONSOLES < next_vt)) ? 0 : next_vt;
}


/**
 * Get the currently active virtual terminal
 * 
 * @return  -1 on error, otherwise the current terminal
 */
int vt_get_active(void)
{
  struct vt_stat state;
  if (ioctl(STDIN_FILENO, VT_GETSTATE, &state) < 0)
    return -1;
  return state.v_active;
}


/**
 * Change currently active virtual terminal and wait for it to complete the switch
 * 
 * @param   vt  The index of the terminal
 * @return      Zero on success, -1 on error
 */
int vt_set_active(int vt)
{
  if (ioctl(STDIN_FILENO, VT_ACTIVATE, vt) < 0)
    return -1;
  
  if (ioctl(STDIN_FILENO, VT_WAITACTIVE, vt) < 0)
    xperror(*argv);
  
  return 0;
}


/**
 * Open a virtual terminal
 * 
 * @param   vt  The index of the terminal
 * @return      The file descriptor for the terminal, -1 on error
 */
int vt_open(int vt)
{
  char vtpath[64]; /* Should be small enought and large enought for any
		      lunatic alternative to /dev/ttyNNN, if not you
		      will need to apply a patch (or fix your system.) */
  sprintf(vtpath, VT_PATH_PATTERN, vt);
  return open(vtpath, O_RDWR);
}


/**
 * Construct a virtual terminal mode that can be used in `vt_get_set_mode`
 * 
 * @param  vt_switch_control  Whether we want to be able to block and delay VT switches
 * @param  vt_leave_signal    The signal that should be send to us we a process is trying
 *                            to switch terminal to another terminal
 * @param  vt_enter_signal    The signal that should be send to us we a process is trying
 *                            to switch terminal to our terminal
 * @param  mode               Output parameter
 */
void vt_construct_mode(int vt_switch_control, int vt_leave_signal,
		       int vt_enter_signal, struct vt_mode* restrict mode)
{
  mode->mode = vt_switch_control ? VT_PROCESS : VT_AUTO;
  mode->waitv = 0;
  mode->relsig = (short int)vt_leave_signal;
  mode->acqsig = (short int)vt_enter_signal;
}

