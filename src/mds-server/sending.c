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
#include "sending.h"

#include "globals.h"
#include "client.h"
#include "queued-interception.h"
#include "multicast.h"

#include <libmdsserver/mds-message.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



/**
 * Get the client by its socket's file descriptor in a synchronised manner
 * 
 * @param   client_fd  The file descriptor of the client's socket
 * @return             The client
 */
static client_t* client_by_socket(int client_fd)
{
  size_t address;
  with_mutex (slave_mutex, address = fd_table_get(&client_map, client_fd););
  return (client_t*)(void*)address;
}


/**
 * Send a multicast message to one recipient
 * 
 * @param   multicast  The message
 * @param   recipient  The recipient
 * @param   modifying  Whether the recipient may modify the message
 * @return             Evaluates to true if and only if the entire message was sent
 */
static int send_multicast_to_recipient(multicast_t* multicast, client_t* recipient, int modifying)
{
  char* msg = multicast->message + multicast->message_ptr;
  size_t n = multicast->message_length - multicast->message_ptr;
  size_t sent;
  
  /* Skip Modify ID header if the interceptors will not perform a modification. */
  if ((modifying == 0) && (multicast->message_ptr == 0))
    {
      n -= multicast->message_prefix;
      multicast->message_ptr += multicast->message_prefix;
    }
  
  /* Send the message. */
  n *= sizeof(char);
  with_mutex (recipient->mutex,
	      if (recipient->open)
		{
		  sent = send_message(recipient->socket_fd, msg + multicast->message_ptr, n);
		  n -= sent;
		  multicast->message_ptr += sent / sizeof(char);
		  if ((n > 0) && (errno != EINTR))
		    perror(*argv);
		}
	      );
  
  return n == 0;
}


/**
 * Wait for the recipient of a multicast to reply
 * 
 * @param  recipient  The recipient
 * @param  modify_id  The modify ID of the multicast
 */
static void wait_for_reply(client_t* recipient, uint64_t modify_id)
{
  /* pthread_cond_timedwait is required to handle re-exec and termination because
     pthread_cond_timedwait and pthread_cond_wait ignore interruptions via signals. */
  struct timespec timeout =
    {
      .tv_sec = 1,
      .tv_nsec = 0
    };
  
  with_mutex_if (modify_mutex, recipient->modify_message == NULL,
		 if (hash_table_contains_key(&modify_map, (size_t)modify_id) == 0)
		   {
		     hash_table_put(&modify_map, (size_t)modify_id, (size_t)(void*)recipient);
		     pthread_cond_signal(&slave_cond);
		   }
		 );
  
  with_mutex_if (recipient->modify_mutex, recipient->modify_message == NULL,
		 while ((recipient->modify_message == NULL) && (terminating == 0))
		   pthread_cond_timedwait(&slave_cond, &slave_mutex, &timeout);
		 if (terminating == 0)
		   hash_table_remove(&modify_map, (size_t)modify_id);
		 );
}


/**
 * Multicast a message
 * 
 * @param  multicast  The multicast message
 */
void multicast_message(multicast_t* multicast)
{
  uint64_t modify_id = 0;
  size_t n = strlen("Modify ID: ");
  if (startswith_n(multicast->message, "Modify ID: ", multicast->message_length, n))
    {
      char* value = multicast->message + n;
      char* lf = strchr(value, '\n');
      *lf = '\0';
      modify_id = (uint64_t)atoll(value);
      *lf = '\n';
    }
  
  for (; multicast->interceptions_ptr < multicast->interceptions_count; multicast->interceptions_ptr++)
    {
      queued_interception_t client_ = multicast->interceptions[multicast->interceptions_ptr];
      client_t* client = client_.client;
      int modifying = 0;
      char* old_buf;
      size_t i;
      mds_message_t* mod;
      
      /* After unmarshalling at re-exec, client will be NULL and must be mapped from its socket. */
      if (client == NULL)
	client_.client = client = client_by_socket(client_.socket_fd);
      
      /* Send the message to the recipient. */
      if (send_multicast_to_recipient(multicast, client, client_.modifying) == 0)
	{
	  /* Stop if we are re-exec:ing or terminating, or continue to next recipient on error. */
	  if (terminating)
	    return;
	  else
	    continue;
	}
      
      /* Do not wait for a reply if it is non-modifying. */
      if (client_.modifying == 0)
	{
	  /* Reset how much of the message has been sent before we continue with next recipient. */
	  multicast->message_ptr = 0;
	  continue;
	}
      
      /* Wait for a reply. */
      wait_for_reply(client, modify_id);
      if (terminating)
	return;
      
      /* Act upon the reply. */
      mod = client->modify_message;
      for (i = 0; i < mod->header_count; i++)
	if (strequals(mod->headers[i], "Modify: yes"))
	  {
	    modifying = 1;
	    break;
	  }
      if (modifying)
	{
	  n = mod->payload_size;
	  old_buf = multicast->message;
	  if (xrealloc(multicast->message, multicast->message_prefix + n, char))
	    {
	      perror(*argv);
	      multicast->message = old_buf;
	    }
	  else
	    memcpy(multicast->message + multicast->message_prefix, mod->payload, n);
	}
      
      /* Free the reply. */
      mds_message_destroy(client->modify_message);
      
      /* Reset how much of the message has been sent before we continue with next recipient. */
      multicast->message_ptr = 0;
    }
}


/**
 * Send the next message in a clients multicast queue
 * 
 * @param  client  The client
 */
void send_multicast_queue(client_t* client)
{
  while (client->multicasts_count > 0)
    {
      multicast_t multicast;
      with_mutex_if (client->mutex, client->multicasts_count > 0,
		     size_t c = (client->multicasts_count -= 1) * sizeof(multicast_t);
		     multicast = client->multicasts[0];
		     memmove(client->multicasts, client->multicasts + 1, c);
		     if (c == 0)
		       {
			 free(client->multicasts);
			 client->multicasts = NULL;
		       }
		     );
      multicast_message(&multicast);
      multicast_destroy(&multicast);
    }
}


/**
 * Send the messages that are in a clients reply queue
 * 
 * @param  client  The client
 */
void send_reply_queue(client_t* client)
{
  char* sendbuf = client->send_pending;
  char* sendbuf_ = sendbuf;
  size_t sent;
  size_t n;
  
  if (client->send_pending_size == 0)
    return;
  
  n = client->send_pending_size;
  client->send_pending_size = 0;
  client->send_pending = NULL;
  with_mutex (client->mutex,
	      while (n > 0)
		{
		  sent = send_message(client->socket_fd, sendbuf_, n);
		  n -= sent;
		  sendbuf_ += sent / sizeof(char);
		  if ((n > 0) && (errno != EINTR)) /* Ignore EINTR */
		    {
		      perror(*argv);
		      break;
		    }
		}
	      free(sendbuf);
	      );
}

