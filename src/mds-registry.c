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
#include "mds-registry.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/hash-help.h>
#include <libmdsserver/client-list.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define reconnect_to_display() -1 /* TODO */



#define MDS_REGISTRY_VARS_VERSION  0



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
    .sanity_check_argc = 1
  };



/**
 * Value of the ‘Message ID’ header for the next message
 */
static int32_t message_id = 2;

/**
 * Buffer for received messages
 */
static mds_message_t received;

/**
 * Whether the server is connected to the display
 */
static int connected = 1;

/**
 * Protocol registry table
 */
static hash_table_t reg_table;

/**
 * Reusable buffer for data to send
 */
static char* send_buffer = NULL;

/**
 * The size of `send_buffer`
 */
static size_t send_buffer_size = 0;

/**
 * General mutex
 */
static pthread_mutex_t reg_mutex;

/**
 * General condition
 */
static pthread_cond_t reg_cond;



/**
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) preinitialise_server(void)
{
  if ((errno = pthread_mutex_init(&reg_mutex, NULL)))
    {
      perror(*argv);
      return 1;
    }
  
  if ((errno = pthread_cond_init(&reg_cond, NULL)))
    {
      perror(*argv);
      pthread_mutex_destroy(&reg_mutex);
      return 1;
    }
  
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
    "Length: 32\n"
    "\n"
    "Command: register\n"
    "Client closed\n" /* TODO support not implemented yet */
    /* -- NEXT MESSAGE -- */
    "Command: reregister\n"
    "Message ID: 1\n"
    "\n";
  
  if (full_send(message, strlen(message)))
    return 1;
  if (hash_table_create_tuned(&reg_table, 32))
    {
      perror(*argv);
      hash_table_destroy(&reg_table, NULL, NULL);
      return 1;
    }
  reg_table.key_comparator = (compare_func*)string_comparator;
  reg_table.hasher = (hash_func*)string_hash;
  server_initialised();
  mds_message_initialise(&received);
  return 0;
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
  size_t rc = 2 * sizeof(int) + sizeof(int32_t) + sizeof(size_t);
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
  size_t n = mds_message_marshal_size(&received);
  buf_set_next(state_buf, int, MDS_REGISTRY_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, int32_t, message_id);
  buf_set_next(state_buf, size_t, n);
  mds_message_marshal(&received, state_buf);
  state_buf += n / sizeof(char);
  /* TODO marshal &reg_table */
  
  hash_table_destroy(&reg_table, (free_func*)reg_table_free_key, (free_func*)reg_table_free_value);
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
  int r, rc = 0;
  size_t n;
  /* buf_get_next(state_buf, int, MDS_REGISTRY_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, int32_t, message_id);
  buf_get_next(state_buf, size_t, n);
  rc |= r = mds_message_unmarshal(&received, state_buf);
  if (r)
    mds_message_destroy(&received);
  state_buf += n / sizeof(char);
  /* TODO unmarshal &reg_table */
  
  reg_table.key_comparator = (compare_func*)string_comparator;
  reg_table.hasher = (hash_func*)string_hash;
  return rc;
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
	  r = handle_message();
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
  perror(*argv);
 fail:
  if (rc || !reexecing)
    {
      hash_table_destroy(&reg_table, (free_func*)reg_table_free_key, (free_func*)reg_table_free_value);
      mds_message_destroy(&received);
    }
  pthread_mutex_destroy(&reg_mutex);
  pthread_cond_destroy(&reg_cond);
  free(send_buffer);
  return rc;
}


/**
 * Handle the received message
 * 
 * @return  Zero on success -1 on error or interruption,
 *          errno will be set accordingly
 */
int handle_message(void)
{
  const char* recv_client_id = NULL;
  const char* recv_message_id = NULL;
  const char* recv_length = NULL;
  const char* recv_action = NULL;
  size_t i, length = 0;
  
#define __get_header(storage, header)  \
  (startswith(received.headers[i], header))  \
    storage = received.headers[i] + strlen(header)
  
  for (i = 0; i < received.header_count; i++)
    {
      if      __get_header(recv_client_id,  "Client ID: ");
      else if __get_header(recv_message_id, "Message ID: ");
      else if __get_header(recv_length,     "Length: ");
      else if __get_header(recv_action,     "Action: ");
      else
	continue;
      if (recv_client_id && recv_message_id && recv_length && recv_action)
	break;
    }
  
#undef __get_header
  
  
  if ((recv_client_id == NULL) || (strequals(recv_client_id, "0:0")))
    {
      eprint("received message from anonymous sender, ignoring.");
      return 0;
    }
  else if (strchr(recv_client_id, ':') == NULL)
    {
      eprint("received message from sender without a colon it its ID, ignoring, invalid ID.");
      return 0;
    }
  else if ((recv_length == NULL) && ((recv_action == NULL) || !strequals(recv_action, "list")))
    {
      eprint("received empty message without `Action: list`, ignoring, has no effect.");
      return 0;
    }
  else if (recv_message_id == NULL)
    {
      eprint("received message with ID, ignoring, master server is misbehaving.");
      return 0;
    }
  
  
  if (recv_length != NULL)
    length = (size_t)atoll(recv_length);
  if (recv_action != NULL)
    recv_action = "add";
  
#define __registry_action(action)  registry_action(length, action, recv_client_id, recv_message_id)
  
  if      (strequals(recv_action, "add"))     return __registry_action(1);
  else if (strequals(recv_action, "remove"))  return __registry_action(-1);
  else if (strequals(recv_action, "wait"))    return __registry_action(0);
  else if (strequals(recv_action, "list"))    return list_registry(recv_client_id, recv_message_id);
  else
    {
      eprint("received invalid action, ignoring.");
      return 0;
    }
  
#undef __registry_action
}


/**
 * Perform an action over the registry
 * 
 * @param   length           The length of the received message
 * @param   action           -1 to remove command, +1 to add commands, 0 to
 *                           wait until the message commnds are registered
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The ID of the received message
 * @return                   Zero on success -1 on error or interruption,
 *                           errno will be set accordingly
 */
int registry_action(size_t length, int action, const char* recv_client_id, const char* recv_message_id)
{
  char* payload = received.payload;
  uint64_t client = 0;
  hash_table_t* wait_set = NULL;
  size_t begin;
  char client_words[22];
  char* client_high;
  char* client_low;
  
  if (action)
    {
      strcpy(client_high = client_words, recv_client_id);
      client_low = rawmemchr(client_words, ':');
      *client_low++ = '\0';
      client = (uint64_t)atoll(client_high);
      client <<= 32;
      client |= (uint64_t)atoll(client_low);
    }
  else
    {
      wait_set = malloc(sizeof(hash_table_t));
      if (hash_table_create(wait_set))
	{
	  perror(*argv);
	  hash_table_destroy(wait_set, NULL, NULL);
	  free(wait_set);
	  return -1;
	}
      wait_set->key_comparator = (compare_func*)string_comparator;
      wait_set->hasher = (hash_func*)string_hash;
    }
  
  if (received.payload_size == length)
    {
      if (xrealloc(received.payload, received.payload_size <<= 1, char))
	{
	  received.payload = payload;
	  received.payload_size >>= 1;
	  perror(*argv);
	  if (wait_set != NULL)
	    {
	      hash_table_destroy(wait_set, NULL, NULL);
	      free(wait_set);
	    }
	  return -1;
	}
      else
	payload = received.payload;
    }
  
  payload[length] = '\n';
  
  errno = pthread_mutex_lock(&reg_mutex);
  if (errno)
    {
      perror(*argv);
      return -1;
    }
  
  for (begin = 0; begin < length;)
    {
      char* end = rawmemchr(payload + begin, '\n');
      size_t len = (size_t)(end - payload) - begin - 1;
      char* command = payload + begin;
      size_t command_key = (size_t)(void*)command;
      
      command[len] = '\0';
      begin += len + 1;
      
      if (action == 1)
	{
	  if (hash_table_contains_key(&reg_table, command_key))
	    {
	      size_t address = hash_table_get(&reg_table, command_key);
	      client_list_t* list = (client_list_t*)(void*)address;
	      if (client_list_add(list, client) < 0)
		{
		  perror(*argv);
		  pthread_mutex_unlock(&reg_mutex);
		  return -1;
		}
	    }
	  else
	    {
	      client_list_t* list = malloc(sizeof(client_list_t));
	      void* address = list;
	      if (list == NULL)
		{
		  perror(*argv);
		  pthread_mutex_unlock(&reg_mutex);
		  return -1;
		}
	      if ((command = strdup(command)) == NULL)
		{
		  perror(*argv);
		  free(list);
		  pthread_mutex_unlock(&reg_mutex);
		  return -1;
		}
	      command_key = (size_t)(void*)command;
	      if (client_list_create(list, 1) ||
		  client_list_add(list, client) ||
		  (hash_table_put(&reg_table, command_key, (size_t)address) == 0))
		{
		  perror(*argv);
		  client_list_destroy(list);
		  free(list);
		  free(command);
		  pthread_mutex_unlock(&reg_mutex);
		  return -1;
		}
	    }
	}
      else if ((action == -1) && hash_table_contains_key(&reg_table, command_key))
	{
	  hash_entry_t* entry = hash_table_get_entry(&reg_table, command_key);
	  size_t address = entry->value;
	  client_list_t* list = (client_list_t*)(void*)address;
	  client_list_remove(list, client);
	  if (list->size == 0)
	    {
	      client_list_destroy(list);
	      free(list);
	      hash_table_remove(&reg_table, command_key);
	      reg_table_free_key(entry->key);
	    }
	}
      else if ((action == 0) && !hash_table_contains_key(&reg_table, command_key))
	{
	  if ((command = strdup(command)) == NULL)
	    {
	      perror(*argv);
	      hash_table_destroy(wait_set, (free_func*)reg_table_free_key, NULL);
	      free(wait_set);
	      pthread_mutex_unlock(&reg_mutex);
	      return -1;
	    }
	  command_key = (size_t)(void*)command;
	  if (hash_table_put(wait_set, command_key, 1) == 0)
	    if (errno)
	      {
		perror(*argv);
		free(command);
		hash_table_destroy(wait_set, (free_func*)reg_table_free_key, NULL);
		free(wait_set);
		pthread_mutex_unlock(&reg_mutex);
		return -1;
	      }
	}
    }
  
  pthread_mutex_unlock(&reg_mutex);
  
  if (action == 0)
    {
      /* TODO */
    }
  
  return 0;
}


/**
 * Send a list of all registered commands to a client
 * 
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The ID of the received message
 * @return                   Zero on success -1 on error or interruption,
 *                           errno will be set accordingly
 */
int list_registry(const char* recv_client_id, const char* recv_message_id)
{
  size_t ptr = 0, i;
  
  if (send_buffer_size == 0)
    {
      if (xmalloc(send_buffer, 256, char))
	{
	  perror(*argv);
	  return -1;
	}
      send_buffer_size = 256;
    }
  
  errno = pthread_mutex_lock(&reg_mutex);
  if (errno)
    {
      perror(*argv);
      return -1;
    }
  
  for (i = 0; i < reg_table.capacity; i++)
    {
      hash_entry_t* bucket = reg_table.buckets[i];
      for (; bucket != NULL; bucket = bucket->next)
	{
	  size_t key = bucket->key;
	  char* command = (char*)(void*)key;
	  size_t len = strlen(command);
	  
	  while (ptr + len + 1 >= send_buffer_size)
	    {
	      char* old = send_buffer;
	      if (xrealloc(send_buffer, send_buffer_size <<= 1, char))
		{
		  send_buffer = old;
		  send_buffer_size >>= 1;
		  perror(*argv);
		  pthread_mutex_unlock(&reg_mutex);
		  return -1;
		}
	    }
	  
	  memcpy(send_buffer + ptr, command, len * sizeof(char));
	  ptr += len;
	  send_buffer[ptr++] = '\n';
	}
    }
  
  i = strlen(recv_message_id) + strlen(recv_client_id) + 10 + 19;
  i += strlen("To: %s\nIn response to: %s\nMessage ID: %" PRIi32 "\nLength: %" PRIu64 "\n\n");
  
  while (ptr + i >= send_buffer_size)
    {
      char* old = send_buffer;
      if (xrealloc(send_buffer, send_buffer_size <<= 1, char))
	{
	  send_buffer = old;
	  send_buffer_size >>= 1;
	  perror(*argv);
	  pthread_mutex_unlock(&reg_mutex);
	  return -1;
	}
    }
  
  sprintf(send_buffer + ptr, "To: %s\nIn response to: %s\nMessage ID: %" PRIi32 "\nLength: %" PRIu64 "\n\n",
	  recv_message_id, recv_client_id, message_id, ptr);
  
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  
  pthread_mutex_unlock(&reg_mutex);
  
  if (full_send(send_buffer + ptr, strlen(send_buffer + ptr)))
    return 1;
  return full_send(send_buffer, ptr);
}


/**
 * Free a key from a table
 * 
 * @param  obj  The key
 */
void reg_table_free_key(size_t obj)
{
  char* command = (char*)(void*)obj;
  free(command);
}


/**
 * Free a value from a table
 * 
 * @param  obj  The value
 */
void reg_table_free_value(size_t obj)
{
  client_list_t* list = (client_list_t*)(void*)obj;
  client_list_destroy(list);
  free(list);
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
	  perror(*argv);
	  return -1;
	}
      message += sent;
      length -= sent;
    }
  return 0;
}

