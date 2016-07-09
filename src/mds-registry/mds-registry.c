/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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

#include "util.h"
#include "globals.h"
#include "registry.h"

#include <libmdsserver/util.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/hash-help.h>
#include <libmdsserver/linked-list.h>

#include <errno.h>
#include <stdio.h>
#define reconnect_to_display() -1



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
int preinitialise_server(void)
{
  int stage = 0;
  
  fail_if ((errno = pthread_mutex_init(&slave_mutex, NULL)));  stage++;
  fail_if ((errno = pthread_cond_init(&slave_cond, NULL)));  stage++;
  
  linked_list_create(&slave_list, 2);
  
  return 0;
  
 fail:
  xperror(*argv);
  if (stage >= 1)  pthread_mutex_destroy(&slave_mutex);
  if (stage >= 2)  pthread_cond_destroy(&slave_cond);
  return 1;
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
    "Length: 32\n"
    "\n"
    "Command: register\n"
    "Client closed\n"
    /* -- NEXT MESSAGE -- */
    "Command: reregister\n"
    "Message ID: 1\n"
    "\n";
  
  /* We are asking all servers to reregister their
     protocols for two reasons:
     
     1) The server would otherwise not get registrations
        from servers started before this server.
     2) If this server crashes we may miss registrations
        that happen between the crash and the recovery.
   */
  
  fail_if (full_send(message, strlen(message)));  stage++;
  fail_if (hash_table_create_tuned(&reg_table, 32));
  reg_table.key_comparator = (compare_func*)string_comparator;
  reg_table.hasher = (hash_func*)string_hash;
  fail_if (server_initialised() < 0);  stage++;
  fail_if (mds_message_initialise(&received));
  
  return 0;  
 fail:
  xperror(*argv);
  if (stage >= 1)  hash_table_destroy(&reg_table, NULL, NULL);
  if (stage >= 2)  mds_message_destroy(&received);
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
	  with_mutex (slave_mutex, linked_list_pack(&slave_list););
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
  /* Join with all slaves threads. */
  with_mutex (slave_mutex,
              while (running_slaves > 0)
                pthread_cond_wait(&slave_cond, &slave_mutex););
  
  if (rc || !reexecing)
    {
      hash_table_destroy(&reg_table, (free_func*)reg_table_free_key, (free_func*)reg_table_free_value);
      mds_message_destroy(&received);
    }
  pthread_mutex_destroy(&slave_mutex);
  pthread_cond_destroy(&slave_cond);
  free(send_buffer);
  return rc;
}

