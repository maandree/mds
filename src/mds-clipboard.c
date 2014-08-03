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
#include "mds-clipboard.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define reconnect_to_display() -1 /* TODO */



#define MDS_CLIPBOARD_VARS_VERSION  0



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
    .require_respawn_info = 1,
    .sanity_check_argc = 1
  };



/**
 * Delete entry only when needed
 */
#define CLIPITEM_AUTOPURGE_NEVER  0

/**
 * Delete entry when the client closes, or needed
 */
#define CLIPITEM_AUTOPURGE_UPON_DEATH  1

/**
 * Delete entry when a point in time has elapsed, or needed
 */
#define CLIPITEM_AUTOPURGE_UPON_CLOCK  2


/**
 * A clipboard entry
 */
typedef struct clipitem
{
  /**
   * The stored content
   */
  char* content;
  
  /**
   * The length of the stored content
   */
  size_t length;
  
  /**
   * Time of planned death if `autopurge` is `CLIPITEM_AUTOPURGE_UPON_CLOCK`
   */
  struct timespec dethklok;
  
  /**
   * The client that issued the inclusion of this entry
   */
  uint64_t client;
  
  /**
   * Rule for automatic deletion
   */
  int autopurge;
  
} clipitem_t;


/**
 * The number of levels in the clipboard
 */
#define CLIPBOARD_LEVELS  3


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
 * The size of each clipstack
 */
static size_t clipboard_size[CLIPBOARD_LEVELS] = { 10, 1, 1 };

/**
 * The number of used elements in each clipstack
 */
static size_t clipboard_used[CLIPBOARD_LEVELS] = { 0, 0, 0 };

/**
 * The entries in each clipstack
 */
static clipitem_t* clipboard[CLIPBOARD_LEVELS];



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
  ssize_t i = 0;
  const char* const message =
    "Command: intercept\n"
    "Message ID: 0\n"
    "Length: 19\n"
    "\n"
    "Command: clipboard\n";
  
  if (full_send(message, strlen(message)))
    return 1;
  
  if (is_respawn)
    {
      const char* const crash_message =
	"Command: clipboard-info\n"
	"Event: crash\n"
	"Message ID: 1\n"
	"\n";
      
      if (full_send(crash_message, strlen(crash_message)))
	return 1;
      
      message_id++;
    }
  
  server_initialised();
  fail_if (mds_message_initialise(&received));
  
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    fail_if (xcalloc(clipboard[i], clipboard_size[i], clipitem_t));
  
  return 0;
  
 pfail:
  xperror(*argv);
  mds_message_destroy(&received);
  while (--i >= 0)
    free(clipboard[i]);
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
  size_t i, j, rc =  2 * sizeof(int) + sizeof(int32_t) + mds_message_marshal_size(&received);
  rc += 2 * CLIPBOARD_LEVELS * sizeof(size_t);
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    for (j = 0; j < clipboard_used[i]; j++)
      {
	clipitem_t clip = clipboard[i][j];
	rc += sizeof(size_t) + sizeof(time_t) + sizeof(long) + sizeof(uint64_t) + sizeof(int);
	rc += clip.length * sizeof(char);
      }
  return rc;
}


/**
 * Wipe a memory area and free it
 * 
 * @param  s  The memory area
 * @param  n  The number of bytes to write
 */
static inline __attribute__((optimize("-O0"))) void wipe_and_free(void* s, size_t n)
{
  if (s != NULL)
    free(memset(s, 0, n));
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  size_t i, j;
  
  buf_set_next(state_buf, int, MDS_CLIPBOARD_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, int32_t, message_id);
  mds_message_marshal(&received, state_buf);
  
  /* Removed entires from the clipboard that may not be marshalled. */
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    for (j = 0; j < clipboard_used[i]; j++)
      {
	clipitem_t clip = clipboard[i][j];
	
	if (clip.autopurge == CLIPITEM_AUTOPURGE_NEVER)
	  continue;
	
	wipe_and_free(clip.content, clip.length * sizeof(char));
	
	memmove(clipboard[i] + j, clipboard[i] + j + 1, (clipboard_used[i] - j - 1) * sizeof(clipitem_t));
	clipboard_used[i]--;
	j--;
      }
  
  /* Marshal clipboard. */
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    {
      buf_set_next(state_buf, size_t, clipboard_size[i]);
      buf_set_next(state_buf, size_t, clipboard_used[i]);
      for (j = 0; j < clipboard_used[i]; j++)
	{
	  clipitem_t clip = clipboard[i][j];
	  buf_set_next(state_buf, size_t, clip.length);
	  buf_set_next(state_buf, time_t, clip.dethklok.tv_sec);
	  buf_set_next(state_buf, long, clip.dethklok.tv_nsec);
	  buf_set_next(state_buf, uint64_t, clip.client);
	  buf_set_next(state_buf, int, clip.autopurge);
	  memcpy(state_buf, clip.content, clip.length * sizeof(char));
	  state_buf += clip.length;
	  
	  free(clip.content);
	}
      free(clipboard[i]);
    }
  
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
  size_t i, j;
  
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    clipboard[i] = NULL;
  
  /* buf_get_next(state_buf, int, MDS_CLIPBOARD_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, int32_t, message_id);
  fail_if (mds_message_unmarshal(&received, state_buf));
  
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    {
      buf_get_next(state_buf, size_t, clipboard_size[i]);
      buf_get_next(state_buf, size_t, clipboard_used[i]);
      fail_if (xcalloc(clipboard[i], clipboard_size[i], clipitem_t));
      
      for (j = 0; j < clipboard_used[i]; j++)
	{
	  clipitem_t* clip = clipboard[i] + j;
	  buf_get_next(state_buf, size_t, clip->length);
	  buf_get_next(state_buf, time_t, clip->dethklok.tv_sec);
	  buf_get_next(state_buf, long, clip->dethklok.tv_nsec);
	  buf_get_next(state_buf, uint64_t, clip->client);
	  buf_get_next(state_buf, int, clip->autopurge);
	  fail_if (xmalloc(clip->content, clip->length, char));
	  memcpy(clip->content, state_buf, clip->length * sizeof(char));
	  state_buf += clip->length;
	}
    }
  
  return 0;
 pfail:
  xperror(*argv);
  mds_message_destroy(&received);
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    if (clipboard[i] != NULL)
      {
	for (j = 0; j < clipboard_used[i]; j++)
	  free(clipboard[i][j].content);
	free(clipboard[i]);
      }
  abort();
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
  int rc = 1;
  size_t i, j;
  
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
  if (!rc && reexecing)
    return 0;
  mds_message_destroy(&received);
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    if (clipboard[i] != NULL)
      {
	for (j = 0; j < clipboard_used[i]; j++)
	  wipe_and_free(clipboard[i][j].content, clipboard[i][j].length);
	free(clipboard[i]);
      }
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

