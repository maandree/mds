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
#include "receiving.h"

#include "globals.h"
#include "client.h"
#include "interceptors.h"

#include <libmdsserver/hash-table.h>
#include <libmdsserver/mds-message.h>
#include <libmdsserver/macros.h>

#include <stddef.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


/**
 * Queue a message for multicasting
 * 
 * @param  message  The message
 * @param  length   The length of the message
 * @param  sender   The original sender of the message
 */
void queue_message_multicast(char* message, size_t length, client_t* sender);


/**
 * Notify waiting client about a received message modification
 * 
 * @param   client     The client whom sent the message
 * @param   message    The message
 * @param   modify_id  The modify ID of the message
 * @return             Normally zero, but 1 if exited because of re-exec or termination
 */
static int modifying_notify(client_t* client, mds_message_t message, uint64_t modify_id)
{
  /* pthread_cond_timedwait is required to handle re-exec and termination because
     pthread_cond_timedwait and pthread_cond_wait ignore interruptions via signals. */
  struct timespec timeout =
    {
      .tv_sec = 1,
      .tv_nsec = 0
    };
  size_t address;
  client_t* recipient;
  mds_message_t* multicast;
  size_t i;
  
  pthread_mutex_lock(&(modify_mutex));
  while (hash_table_contains_key(&modify_map, (size_t)modify_id) == 0)
    {
      if (terminating)
	{
	  pthread_mutex_unlock(&(modify_mutex));
	  return 1;
	}
      pthread_cond_timedwait(&slave_cond, &slave_mutex, &timeout);
    }
  address = hash_table_get(&modify_map, (size_t)modify_id);
  recipient = (client_t*)(void*)address;
  fail_if (xmalloc(multicast = recipient->modify_message, 1, mds_message_t));
  mds_message_zero_initialise(multicast);
  fail_if (xmalloc(multicast->payload, message.payload_size, char));
  memcpy(multicast->payload, message.payload, message.payload_size * sizeof(char));
  fail_if (xmalloc(multicast->headers, message.header_count, char*));
  for (i = 0; i < message.header_count; i++, multicast->header_count++)
    {
      multicast->headers[i] = strdup(message.headers[i]);
      fail_if (multicast->headers[i] == NULL);
    }
 done:
  pthread_mutex_unlock(&(modify_mutex));
  with_mutex (client->modify_mutex, pthread_cond_signal(&(client->modify_cond)););
  
  return 0;
  
  
 fail:
  xperror(*argv);
  if (multicast != NULL)
    {
      mds_message_destroy(multicast);
      free(multicast);
      recipient->modify_message = NULL;
    }
  goto done;
}


/**
 * Add intercept conditions listed in the payload of a message
 * 
 * @param   client     The intercepting client
 * @param   modifying  Whether then client may modify the messages
 * @param   priority   The client's interception priority
 * @param   stop       Whether to stop listening rather than start or reconfigure
 * @return             Zero on success, -1 on error
 */
static int add_intercept_conditions_from_message(client_t* client, int modifying, int64_t priority, int stop)
{
  int errno_ = 0;
  char* payload = client->message.payload;
  size_t payload_size = client->message.payload_size;
  size_t size = 64;
  char* buf;
  
  if (xmalloc(buf, size + 1, char))
    return -1;
  
  /* All messages */
  if (client->message.payload_size == 0)
    {
      *buf = '\0';
      add_intercept_condition(client, buf, priority, modifying, stop);
      goto done;
    }
  
  /* Filtered messages */
  for (;;)
    {
      char* end = memchr(payload, '\n', payload_size);
      size_t len = end == NULL ? payload_size : (size_t)(end - payload);
      if (len == 0)
	{
	  payload++;
	  payload_size--;
	  break;
	}
      if (len > size)
	{
	  char* old_buf = buf;
	  if (xrealloc(buf, (size <<= 1) + 1, char))
	    {
	      errno_ = errno;
	      free(old_buf);
	      pthread_mutex_unlock(&(client->mutex));
	      break;
	    }
	}
      memcpy(buf, payload, len);
      buf[len] = '\0';
      add_intercept_condition(client, buf, priority, modifying, stop);
      if (end == NULL)
	break;
      payload = end + 1;
      payload_size -= len + 1;
    }
  
 done:
  free(buf);
  errno = errno_;
  return errno_ ? -1 : 0;
}


/**
 * Assign and ID to a client, if not already assigned, and send it to that client
 * 
 * @param   client      The client to who an ID should be assigned
 * @param   message_id  The message ID of the ID request
 * @return              Zero on success, -1 on error
 */
static int assign_and_send_id(client_t* client, const char* message_id)
{
  char* msgbuf = NULL;
  char* msgbuf_;
  size_t n;
  int rc = -1;
  
  /* Construct response. */
  n = 2 * 10 + strlen(message_id) + 1;
  n += strlen("ID assignment: :\nIn response to: \n\n");
  fail_if (xmalloc(msgbuf, n, char));
  snprintf(msgbuf, n,
	   "ID assignment: %" PRIu32 ":%" PRIu32 "\n"
	   "In response to: %s\n"
	   "\n",
	   (uint32_t)(client->id >> 32),
	   (uint32_t)(client->id >>  0),
	   message_id == NULL ? "" : message_id);
  n = strlen(msgbuf);
  
  /* Multicast the reply. */
  msgbuf_ = strdup(msgbuf);
  fail_if (msgbuf_ == NULL);
  queue_message_multicast(msgbuf_, n, client);
  
  /* Queue message to be sent when this function returns.
     This done to simplify `multicast_message` for re-exec and termination. */
#define fail  fail_in_mutex
  with_mutex (client->mutex,
	      if (client->send_pending_size == 0)
		{
		  /* Set the pending message. */
		  client->send_pending = msgbuf;
		  client->send_pending_size = n;
		}
	      else
		{
		  /* Concatenate message to already pending messages. */
		  size_t new_len = client->send_pending_size + n;
		  char* msg_new = client->send_pending;
		  fail_if (xrealloc(msg_new, new_len, char));
		  memcpy(msg_new + client->send_pending_size, msgbuf, n * sizeof(char));
		  client->send_pending = msg_new;
		  client->send_pending_size = new_len;
		}
	      (msgbuf = NULL, rc = 0, errno = 0);
	     fail_in_mutex:
	      );
#undef fail
  
 fail: /* Also success. */
  xperror(*argv);
  free(msgbuf);
  return rc;
}


/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param   client  The client whom sent the message
 * @return          Normally zero, but 1 if exited because of re-exec or termination
 */
int message_received(client_t* client)
{
  mds_message_t message = client->message;
  int assign_id = 0;
  int modifying = 0;
  int intercept = 0;
  int64_t priority = 0;
  int stop = 0;
  const char* message_id = NULL;
  uint64_t modify_id = 0;
  char* msgbuf = NULL;
  size_t i, n;
  
  
  /* Parser headers. */
  for (i = 0; i < message.header_count; i++)
    {
      const char* h = message.headers[i];
      if      (strequals(h,  "Command: assign-id"))  assign_id  = 1;
      else if (strequals(h,  "Command: intercept"))  intercept  = 1;
      else if (strequals(h,  "Modifying: yes"))      modifying  = 1;
      else if (strequals(h,  "Stop: yes"))           stop       = 1;
      else if (startswith(h, "Message ID: "))        message_id = strstr(h, ": ") + 2;
      else if (startswith(h, "Priority: "))          priority   = ato64(strstr(h, ": ") + 2);
      else if (startswith(h, "Modify ID: "))         modify_id  = atou64(strstr(h, ": ") + 2);
    }
  
  
  /* Notify waiting client about a received message modification. */
  if (modifying)
    return modifying_notify(client, message, modify_id);
    /* Do nothing more, not not even multicast this message. */
  
  
  if (message_id == NULL)
    {
      eprint("received message without a message ID, ignoring.");
      return 0;
    }
  
  /* Assign ID if not already assigned. */
  if (assign_id && (client->id == 0))
    {
      intercept |= 2;
      with_mutex_if (slave_mutex, (client->id = next_client_id++) == 0,
		     eprint("this is impossible, ID counter has overflowed.");
		     /* If the program ran for a millennium it would
			take c:a 585 assignments per nanosecond. This
			cannot possibly happen. (It would require serious
			dedication by generations of ponies (or just an alicorn)
			to maintain the process and transfer it new hardware.) */
		     abort();
		     );
    }
  
  /* Make the client listen for messages addressed to it. */
  if (intercept)
    {
      pthread_mutex_lock(&(client->mutex));
      if ((intercept & 1)) /* from payload */
	fail_if (add_intercept_conditions_from_message(client, modifying, priority, stop) < 0);
      if ((intercept & 2)) /* "To: $(client->id)" */
	{
	  char buf[26];
	  xsnprintf(buf, "To: %" PRIu32 ":%" PRIu32,
		    (uint32_t)(client->id >> 32),
		    (uint32_t)(client->id >>  0));
	  add_intercept_condition(client, buf, priority, modifying, 0);
	}
      pthread_mutex_unlock(&(client->mutex));
    }
  
  
  /* Multicast the message. */
  n = mds_message_compose_size(&message);
  fail_if ((msgbuf = malloc(n)) == NULL);
  mds_message_compose(&message, msgbuf);
  queue_message_multicast(msgbuf, n / sizeof(char), client);
  msgbuf = NULL;
  
  
  /* Send asigned ID. */
  if (assign_id)
    fail_if (assign_and_send_id(client, message_id) < 0);
  
  return 0;
  
 fail:
  xperror(*argv);
  free(msgbuf);
  return 0;
}
