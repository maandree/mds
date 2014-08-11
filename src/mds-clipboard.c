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
    .sanity_check_argc = 1,
    .fork_for_safety = 0
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
    "Length: 33\n"
    "\n"
    "Command: clipboard\n"
    "Client closed\n";
  
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
  
  fail_if (server_initialised() < 0);
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
	  if (r = handle_message(), r == 0)
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


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void)
{
  /* Fetch message headers. */
  
  const char* recv_client_id = "0:0";
  const char* recv_message_id = NULL;
  const char* recv_length = NULL;
  const char* recv_level = NULL;
  const char* recv_action = NULL;
  const char* recv_size = NULL;
  const char* recv_index = "0";
  const char* recv_time_to_live = "forever";
  const char* recv_client_closed = NULL;
  size_t i;
  int level;
  
#define __get_header(storage, header)  \
  (startswith(received.headers[i], header))  \
    storage = received.headers[i] + strlen(header)
  
  for (i = 0; i < received.header_count; i++)
    {
      if      __get_header(recv_client_id,     "Client ID: ");
      else if __get_header(recv_message_id,    "Message ID: ");
      else if __get_header(recv_length,        "Length: ");
      else if __get_header(recv_action,        "Action: ");
      else if __get_header(recv_level,         "Level: ");
      else if __get_header(recv_size,          "Size: ");
      else if __get_header(recv_index,         "Index: ");
      else if __get_header(recv_time_to_live,  "Time to live: ");
      else if __get_header(recv_client_closed, "Client closed: ");
      else
	continue;
    }
  
#undef __get_header
  
  
  /* Validate headers and take appropriate action. */
  
  if (recv_message_id == NULL)
    {
      eprint("received message with ID, ignoring, master server is misbehaving.");
      return 0;
    }
  
  if (recv_client_closed)
    {
      if (strequals(recv_client_closed, "0:0"))
	return 0;
      return clipboard_death(recv_client_closed);
    }
  
  if (recv_action == NULL)
    {
      eprint("received message without any action, ignoring.");
      return 0;
    }
  if (recv_level == NULL)
    {
      eprint("received message without specified clipboard level, ignoring.");
      return 0;
    }
  level = atoi(recv_level);
  if ((level < 0) || (CLIPBOARD_LEVELS <= level))
    {
      eprint("received message without invalid clipboard level, ignoring.");
      return 0;
    }
  if (strequals(recv_client_id, "0:0"))
    if (strequals(recv_message_id, "read") || strequals(recv_message_id, "get-size"))
      {
	eprint("received information request from an anonymous client, ignoring.");
	return 0;
      }
  
  if (strequals(recv_message_id, "add"))
    {
      if (recv_length == NULL)
	{
	  eprint("received request for adding a clipboard entry but did not receive any content, ignoring.");
	  return 0;
	}
      if ((strequals(recv_client_id, "0:0")) && startswith(recv_time_to_live, "until-death"))
	{
	  eprint("received request new clipboard entry with autopurge upon"
		 " client close from an anonymous client, ignoring.");
	  return 0;
	}
      return clipboard_add(level, recv_time_to_live, recv_client_id);
    }
  else if (strequals(recv_message_id, "read"))
    return clipboard_read(level, (size_t)atoll(recv_index), recv_client_id, recv_message_id);
  else if (strequals(recv_message_id, "clear"))
    return clipboard_clear(level);
  else if (strequals(recv_message_id, "set-size"))
    {
      if (recv_size == NULL)
	{
	  eprint("received request for clipboard resizing without a new size, ignoring.");
	  return 0;
	}
      return clipboard_set_size(level, (size_t)atoll(recv_size));
    }
  else if (strequals(recv_message_id, "get-size"))
    return clipboard_get_size(level, recv_client_id, recv_message_id);
  
  eprint("received message with invalid action, ignoring.");
  return 0;
}


/**
 * Free an entry from the clipboard
 * 
 * @param  entry  The clipboard entry to free
 */
static inline void free_clipboard_entry(clipitem_t* entry)
{
  if (entry->autopurge == CLIPITEM_AUTOPURGE_NEVER)
    free(entry->content);
  else
    wipe_and_free(entry->content, entry->length);
  entry->content = NULL;
}


/**
 * Broadcast notification about an automatic removal of an entry
 * 
 * @param   level  The clipboard level
 * @param   index  The index in the clipstack of the removed entry
 * @return         Zero on success, -1 on error, `errno` will be set accordingly
 */
static int clipboard_notify_pop(int level, size_t index)
{
  size_t size = clipboard_size[level];
  size_t used = clipboard_used[level];
  size_t n = 10 + 3 * (3 * sizeof(size_t) + sizeof(int));
  char* message;
  
  n += strlen("Command: clipboard-info\n"
	      "Event: crash\n"
	      "Message ID: %" PRIi32 "\n"
	      "Level: %i\n"
	      "Popped: %lu\n"
	      "Size: %lu\n"
	      "Used: %lu\n"
	      "\n");
  
  if (xmalloc(message, n, char))
    return -1;
  
  sprintf(message,
	  "Command: clipboard-info\n"
	  "Event: crash\n"
	  "Message ID: %" PRIi32 "\n"
	  "Level: %i\n"
	  "Popped: %lu\n"
	  "Size: %lu\n"
	  "Used: %lu\n"
	  "\n",
	  message_id, level, index, size, used);
  
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  return full_send(message, strlen(message)) ? -1 : 0;
}


/**
 * Remove old entries from a clipstack
 * 
 * @param   level      The clipboard level
 * @param   client_id  The ID of the client that has newly closed, `NULL` if none
 * @return             Zero on success, -1 on error
 */
static int clipboard_purge(int level, const char* client_id)
{
  uint64_t client = client_id ? parse_client_id(client_id) : 0;
  struct timespec now;
  size_t i;
  
  fail_if (monotone(&now));
  
  for (i = 0; i < clipboard_used[level]; i++)
    {
      clipitem_t* clip = clipboard[level] + i;
      if ((clip->autopurge & CLIPITEM_AUTOPURGE_UPON_DEATH))
	{
	  if (clip->client == client)
	    goto removed;
	}
      if ((clip->autopurge & CLIPITEM_AUTOPURGE_UPON_CLOCK))
	{
	  if (clip->dethklok.tv_sec > now.tv_sec)
	    goto removed;
	  if (clip->dethklok.tv_sec == now.tv_sec)
	    if (clip->dethklok.tv_nsec >= now.tv_nsec)
	      goto removed;
	}
      
      continue;
    removed:
      free_clipboard_entry(clipboard[level] + i);
      clipboard_used[level]--;
      fail_if (clipboard_notify_pop(level, i));
      memmove(clipboard[level] + i, clipboard[level] + i + 1, (clipboard_used[level] - i) * sizeof(clipitem_t));
      i--;
    }
  
  return 0;
 pfail:
  xperror(*argv);
  return -1;
}


/**
 * Remove entries in the clipboard added by a client
 * 
 * @param   recv_client_id  The ID of the client
 * @return                  Zero on success, -1 on error
 */
int clipboard_death(const char* recv_client_id)
{
  int i;
  for (i = 0; i < CLIPBOARD_LEVELS; i++)
    if (clipboard_purge(i, recv_client_id))
      return -1;
  return 0;
}


/**
 * Add a new entry to the clipboard
 * 
 * @param   level           The clipboard level
 * @param   time_to_live    When the entry should be removed
 * @param   recv_client_id  The ID of the client
 * @return                  Zero on success, -1 on error
 */
int clipboard_add(int level, const char* time_to_live, const char* recv_client_id)
{
  int autopurge = CLIPITEM_AUTOPURGE_UPON_CLOCK;
  uint64_t client = recv_client_id ? parse_client_id(recv_client_id) : 0;
  clipitem_t new_clip;
  
  if (clipboard_purge(level, NULL))
    return -1;
  
  if (strequals(time_to_live, "forever"))
    autopurge = CLIPITEM_AUTOPURGE_NEVER;
  else if (strequals(time_to_live, "until-death"))
    autopurge = CLIPITEM_AUTOPURGE_UPON_DEATH;
  else if (startswith(time_to_live, "until-death "))
    {
      autopurge = CLIPITEM_AUTOPURGE_UPON_DEATH_OR_CLOCK;
      time_to_live += strlen("until-death ");
    }
  
  if ((autopurge & CLIPITEM_AUTOPURGE_UPON_CLOCK))
    {
      struct timespec dethklok;
      fail_if (monotone(&dethklok));
      dethklok.tv_sec += (time_t)atoll(time_to_live);
      new_clip.dethklok = dethklok;
    }
  else
    {
      new_clip.dethklok.tv_sec = 0;
      new_clip.dethklok.tv_nsec = 0;
    }
  
  new_clip.client = client;
  new_clip.autopurge = autopurge;
  new_clip.length = received.payload_size;
  
  fail_if (xmalloc(new_clip.content, new_clip.length, char));
  memcpy(new_clip.content, received.payload, new_clip.length * sizeof(char));
  
  if (clipboard_used[level] == clipboard_size[level])
    free_clipboard_entry(clipboard[level] + clipboard_used[level] - 1);
  memmove(clipboard[level] + 1, clipboard[level], (clipboard_used[level] - 1) * sizeof(clipitem_t));
  clipboard[level][0] = new_clip;
  
  return 0;
 pfail:
  xperror(*argv);
  return -1;
}


/**
 * Read an entry to the clipboard
 * 
 * @param   level            The clipboard level
 * @param   index            The index of the clipstack element
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The message ID of the received message
 * @return                   Zero on success, -1 on error
 */
int clipboard_read(int level, size_t index, const char* recv_client_id, const char* recv_message_id)
{
  char* message = NULL;
  clipitem_t* clip = NULL;
  size_t n;
  
  if (clipboard_purge(level, NULL))
    return -1;
  
  if (clipboard_used[level] == 0)
    {
      n = strlen("To: %s\n"
		 "In response to: %s\n"
		 "Message ID: %" PRIi32 "\n"
		 "\n");
      n += strlen(recv_client_id) + strlen(recv_message_id) + 10;
      
      fail_if (xmalloc(message, n, char));
      
      sprintf(message,
	      "To: %s\n"
	      "In response to: %s\n"
	      "Message ID: %" PRIi32 "\n"
	      "\n",
	      recv_client_id, recv_message_id, message_id);
      
      goto send;
    }
  
  if (index >= clipboard_used[level])
    index = clipboard_used[level] - 1;
  
  clip = clipboard[level] + index;
  
  n = strlen("To: %s\n"
	     "In response to: %s\n"
	     "Message ID: %" PRIi32 "\n"
	     "Length: %lu\n"
	     "\n");
  n += strlen(recv_client_id) + strlen(recv_message_id) + 10 + 3 * sizeof(size_t);
  
  fail_if (xmalloc(message, n, char));
  
  sprintf(message,
	  "To: %s\n"
	  "In response to: %s\n"
	  "Message ID: %" PRIi32 "\n"
	  "Length: %lu\n"
	  "\n",
	  recv_client_id, recv_message_id, message_id, clip->length);
  
 send:
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  fail_if (full_send(message, strlen(message)));
  if (clip != NULL)
    fail_if (full_send(clip->content, clip->length));
  
  free(message);
  return 0;
 pfail:
  xperror(*argv);
  return -1;
}


/**
 * Clear a clipstack
 * 
 * @param   level  The clipboard level
 * @return         Zero on success, -1 on error
 */
int clipboard_clear(int level)
{
  size_t i;
  for (i = 0; i < clipboard_used[level]; i++)
    free_clipboard_entry(clipboard[level] + i);
  clipboard_used[level] = 0;
  return 0;
}


/**
 * Resize a clipstack
 * 
 * @param   level  The clipboard level
 * @param   size   The new clipstack size
 * @return         Zero on success, -1 on error
 */
int clipboard_set_size(int level, size_t size)
{
  size_t i;
  if (clipboard_purge(level, NULL))
    return -1;
  
  if (size < clipboard_size[level])
    {
      size_t old_used = clipboard_used[level];
      if (size < old_used)
	{
	  clipboard_used[level] = size;
	  for (i = size; i < old_used; i++)
	    free_clipboard_entry(clipboard[level] + i);
	}
    }
  
  if (size != clipboard_size[level])
    {
      clipitem_t* old = clipboard[level];
      if (xrealloc(clipboard[level], size, clipitem_t))
	{
	  clipboard[level] = old;
	  goto pfail;
	}
      clipboard_size[level] = size;
    }
  
  return 0;
 pfail:
  xperror(*argv);
  return -1;
}


/**
 * Get the size of a clipstack and how many entries it contains
 * 
 * @param   level            The clipboard level
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The message ID of the received message
 * @return                   Zero on success, -1 on error
 */
int clipboard_get_size(int level, const char* recv_client_id, const char* recv_message_id)
{
  char* message = NULL;
  size_t n;
  if (clipboard_purge(level, NULL))
    return -1;
  
  n = strlen("To: %s\n"
	     "In response to: %s\n"
	     "Message ID: %" PRIi32 "\n"
	     "Size: %lu\n"
	     "Used: %lu\n"
	     "\n");
  n += strlen(recv_client_id) + strlen(recv_message_id) + 10 + 2 * 3 * sizeof(size_t);
  
  fail_if (xmalloc(message, n, char));
  sprintf(message,
	  "To: %s\n"
	  "In response to: %s\n"
	  "Message ID: %" PRIi32 "\n"
	  "Size: %lu\n"
	  "Used: %lu\n"
	  "\n",
	  recv_client_id, recv_message_id, message_id, clipboard_size[level], clipboard_used[level]);
  
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  
  fail_if (full_send(message, strlen(message)));
  
  free(message);
  return 0;
 pfail:
  xperror(*argv);
  free(message);
  return -1;
}

