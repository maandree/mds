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
#include "mds-server.h"

#include "globals.h"
#include "interception_condition.h"
#include "client.h"
#include "queued_interception.h"
#include "multicast.h"
#include "signals.h"
#include "interceptors.h"
#include "sending.h"

#include <libmdsserver/config.h>
#include <libmdsserver/linked-list.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/fd-table.h>
#include <libmdsserver/mds-message.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/hash-help.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <inttypes.h>
#include <time.h>



/**
 * Entry point of the server
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  int is_respawn = -1;
  int socket_fd = -1;
  int reexec = 0;
  int unparsed_args_ptr = 1;
  char* unparsed_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  int i;
  pthread_t slave_thread;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 3)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 3.
#endif
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Drop privileges like it's hot. */
  fail_if (drop_privileges());
  
  /* Sanity check the number of command line arguments. */
  exit_if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT,
	   eprint("that number of arguments is ridiculous, I will not allow it."););
  
  
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      int v;
      if ((v = strequals(arg, "--initial-spawn")) || /* Initial spawn? */
	  strequals(arg, "--respawn"))               /* Respawning after crash? */
	{
	  exit_if (is_respawn == v,
		   eprintf("conflicting arguments %s and %s cannot be combined.",
			   "--initial-spawn", "--respawn"););
	  is_respawn = !v;
	}
      else if (startswith(arg, "--socket-fd=")) /* Socket file descriptor. */
	{
	  exit_if (socket_fd != -1,
		   eprintf("duplicate declaration of %s.", "--socket-fd"););
	  exit_if (strict_atoi(arg += strlen("--socket-fd="), &socket_fd, 0, INT_MAX) < 0,
		   eprintf("invalid value for %s: %s.", "--socket-fd", arg););
	}
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	reexec = 1;
      else
	/* Not recognised, it is probably for another server. */
	unparsed_args[unparsed_args_ptr++] = arg;
    }
  unparsed_args[unparsed_args_ptr] = NULL;
  if (reexec)
    {
      is_respawn = 1;
      eprint("re-exec performed.");
    }
  
  
  /* Check that manditory arguments have been specified. */
  exit_if (is_respawn < 0,
	   eprintf("missing state argument, require either %s or %s.",
		   "--initial-spawn", "--respawn"););
  exit_if (socket_fd < 0,
	   eprint("missing socket file descriptor argument."););
  
  
  /* Run mdsinitrc. */
  if (is_respawn == 0)
    {
      pid_t pid = fork();
      fail_if (pid == (pid_t)-1);
      
      if (pid == 0) /* Child process exec:s, the parent continues without waiting for it. */
	{
	  /* Close all files except stdin, stdout and stderr. */
	  close_files((fd > 2) || (fd == socket_fd));
	  
	  /* Run mdsinitrc. */
	  run_initrc(unparsed_args);
	  return 1;
	}
    }
  
#define __free(I) \
  if (I >= 0)  fd_table_destroy(&client_map, NULL, NULL);  \
  if (I >= 1)  linked_list_destroy(&client_list);          \
  if (I >= 2)  pthread_mutex_destroy(&slave_mutex);        \
  if (I >= 3)  pthread_cond_destroy(&slave_cond);          \
  if (I >= 4)  pthread_mutex_destroy(&modify_mutex);       \
  if (I >= 5)  pthread_cond_destroy(&modify_cond);         \
  if (I >= 6)  hash_table_destroy(&modify_map, NULL, NULL)
  
#define error_if(I, CONDITION)  if (CONDITION)  { perror(*argv); __free(I); return 1; }
  
  /* Create list and table of clients. */
  if (reexec == 0)
    {
      error_if (0, fd_table_create(&client_map));
      error_if (1, linked_list_create(&client_list, 32));
    }
  
  /* Store the current thread so it can be killed from elsewhere. */
  master_thread = pthread_self();
  
  /* Set up traps for especially handled signals. */
  error_if (1, trap_signals() < 0);
  
  /* Create mutex and condition for slave counter. */
  error_if (1, (errno = pthread_mutex_init(&slave_mutex, NULL)));
  error_if (2, (errno = pthread_cond_init(&slave_cond, NULL)));
  
  /* Create mutex, condition and map for message modification. */
  error_if (3, (errno = pthread_mutex_init(&modify_mutex, NULL)));
  error_if (4, (errno = pthread_cond_init(&modify_cond, NULL)));
  error_if (5, hash_table_create(&modify_map));
  
#undef error_if
  
  
  /* Unmarshal the state of the server. */
  if (reexec)
    {
      pid_t pid = getpid();
      int reexec_fd, r;
      char shm_path[NAME_MAX + 1];
      
      /* Acquire access to marshalled data. */
      xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
      reexec_fd = shm_open(shm_path, O_RDONLY, S_IRWXU);
      fail_if (reexec_fd < 0); /* Critical. */
      /* Unmarshal state. */
      r = unmarshal_server(reexec_fd);
      close(reexec_fd);
      shm_unlink(shm_path);
      if (r < 0)
	{
	  /* Close all files (hopefully sockets) we do not know what they are. */
	  close_files((fd > 2) && (fd != socket_fd) && (fd_table_contains_key(&client_map, fd) == 0));
	}
    }
  
  /* Accepting incoming connections. */
  while (running && (terminating == 0))
    {
      /* Accept connection. */
      int client_fd = accept(socket_fd, NULL, NULL);
      
      /* Handle errors and shutdown. */
      if (client_fd == -1)
	{
	  switch (errno)
	    {
	    case EINTR:
	      /* Interrupted. */
	      if (terminating)
		goto terminate;
	      break;
	      
	    case ECONNABORTED:
	    case EINVAL:
	      /* Closing. */
	      running = 0;
	      break;
	      
	    default:
	      /* Error. */
	      perror(*argv);
	      break;
	    }
	  continue;
	}
      
      /* Increase number of running slaves. */
      with_mutex (slave_mutex, running_slaves++;);
      
      /* Start slave thread. */
      create_slave(&slave_thread, client_fd);
    }
  
 terminate:
  
  if (reexecing)
    goto reexec;
  
  /* Join with all slaves threads. */
  with_mutex (slave_mutex,
	      while (running_slaves > 0)
		pthread_cond_wait(&slave_cond, &slave_mutex););
  
  /* Release resources. */
  __free(9999);
  
  return 0;
  
#undef __free
  
  
 reexec:
  {
    pid_t pid = getpid();
    int reexec_fd;
    char shm_path[NAME_MAX + 1];
    ssize_t node;
    
    /* Join with all slaves threads. */
    with_mutex (slave_mutex,
		while (running_slaves > 0)
		  pthread_cond_wait(&slave_cond, &slave_mutex););
    
    /* Release resources. */
    pthread_mutex_destroy(&slave_mutex);
    pthread_cond_destroy(&slave_cond);
    pthread_mutex_destroy(&modify_mutex);
    pthread_cond_destroy(&modify_cond);
    hash_table_destroy(&modify_map, NULL, NULL);
    
    /* Marshal the state of the server. */
    xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
    reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
    fail_if (reexec_fd < 0);
    if (marshal_server(reexec_fd) < 0)
      goto reexec_fail;
    close(reexec_fd);
    reexec_fd = -1;
    
    /* Release resources. */
    foreach_linked_list_node (client_list, node)
      {
	client_t* client = (client_t*)(void*)(client_list.values[node]);
	client_destroy(client);
      }
    fd_table_destroy(&client_map, NULL, NULL);
    linked_list_destroy(&client_list);
    
    /* Re-exec the server. */
    reexec_server(argc, argv, reexec);
    
  reexec_fail:
    perror(*argv);
    if (reexec_fd >= 0)
      close(reexec_fd);
    shm_unlink(shm_path);
    /* Returning non-zero is important, otherwise the server cannot
       be respawn if the re-exec fails. */
    return 1;
  }
  
 pfail:
  perror(*argv);
  return 1;
}


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Outout data
 */
void* slave_loop(void* data)
{
  int socket_fd = (int)(intptr_t)data;
  ssize_t entry = LINKED_LIST_UNUSED;
  size_t information_address = fd_table_get(&client_map, (size_t)socket_fd);
  client_t* information = (client_t*)(void*)information_address;
  char* msgbuf = NULL;
  size_t n;
  size_t tmp;
  int r;
  
  
  if (information == NULL)
    {
      /* Create information table. */
      fail_if (xmalloc(information, 1, client_t));
      client_initialise(information);
      
      /* Add to list of clients. */
      pthread_mutex_lock(&slave_mutex);
      entry = linked_list_insert_end(&client_list, (size_t)(void*)information);
      if (entry == LINKED_LIST_UNUSED)
	{
	  perror(*argv);
	  pthread_mutex_unlock(&slave_mutex);
	  goto fail;
	}
      
      /* Add client to table. */
      tmp = fd_table_put(&client_map, socket_fd, (size_t)(void*)information);
      if ((tmp == 0) && errno)
	{
	  perror(*argv);
	  pthread_mutex_unlock(&slave_mutex);
	  goto fail;
	}
      pthread_mutex_unlock(&slave_mutex);
      
      /* Fill information table. */
      information->list_entry = entry;
      information->socket_fd = socket_fd;
      information->open = 1;
      fail_if (mds_message_initialise(&(information->message)));
    }
  
  /* Store slave thread and create mutexes and conditions. */
  fail_if (client_initialise_threading(information));
  
  /* Set up traps for especially handled signals. */
  fail_if (trap_signals() < 0);
  
  
  /* Fetch messages from the slave. */
  while ((terminating == 0) && information->open)
    {
      /* Send queued multicast messages. */
      send_multicast_queue(information);
      
      /* Send queued messages. */
      send_reply_queue(information);
      
      
      /* Fetch message. */
      r = fetch_message(information);
      if ((r == 0) && message_received(information))
	goto terminate;
      else if (r == -2)
	goto fail;
      else if (r && (errno == EINTR) && terminating)
      	goto terminate; /* Stop the thread if we are re-exec:ing or terminating the server. */
    }
  /* Stop the thread if we are re-exec:ing or terminating the server. */
  if (terminating)
    goto terminate;
  
  
  /* Multicast information about the client closing. */
  n = 2 * 10 + 1 + strlen("Client closed: :\n\n");
  fail_if (xmalloc(msgbuf, n, char));
  snprintf(msgbuf, n,
	   "Client closed: %" PRIu32 ":%" PRIu32 "\n"
	   "\n",
	   (uint32_t)(information->id >> 32),
	   (uint32_t)(information->id >>  0));
  n = strlen(msgbuf);
  queue_message_multicast(msgbuf, n, information);
  msgbuf = NULL;
  
  
 terminate: /* This done on success as well. */
  if (reexecing)
    goto reexec;
  
 fail: /* This done on success as well. */
  /* Close socket and free resources. */
  close(socket_fd);
  free(msgbuf);
  if (information != NULL)
    client_destroy(information);
  
  /* Unlist client and decrease the slave count. */
  with_mutex (slave_mutex,
	      fd_table_remove(&client_map, socket_fd);
	      if (entry != LINKED_LIST_UNUSED)
		linked_list_remove(&client_list, entry);
	      running_slaves--;
	      pthread_cond_signal(&slave_cond););
  return NULL;
  
  
 pfail:
  perror(*argv);
  goto fail;
  
  
 reexec:
  /* Tell the master thread that the slave has closed,
     this is done because re-exec causes a race-condition
     between the acception of a slave and the execution
     of the the slave thread. */
  with_mutex (slave_mutex,
	      running_slaves--;
	      pthread_cond_signal(&slave_cond););
  return NULL;
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


/**
 * Receive a full message and update open status if the client closes
 * 
 * @param   client  The client
 * @return          Zero on success, -2 on failure, otherwise -1
 */
int fetch_message(client_t* client)
{
  int r = mds_message_read(&(client->message), client->socket_fd);
  if (r == 0)
    return 0;
  
  if (r == -2)
    eprint("corrupt message received.");
  else if (errno == ECONNRESET)
    {
      r = mds_message_read(&(client->message), client->socket_fd);
      client->open = 0;
      /* Connection closed. */
    }
  else if (errno != EINTR)
    {
      r = -2;
      perror(*argv);
    }
  
  return r;
}


/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param   client  The client has sent a message
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
  size_t i, n;
  char* msgbuf;
  
  
  /* Parser headers. */
  for (i = 0; i < message.header_count; i++)
    {
      const char* h = message.headers[i];
      if      (strequals(h,  "Command: assign-id"))  assign_id  = 1;
      else if (strequals(h,  "Command: intercept"))  intercept  = 1;
      else if (strequals(h,  "Modifying: yes"))      modifying  = 1;
      else if (strequals(h,  "Stop: yes"))           stop       = 1;
      else if (startswith(h, "Message ID: "))        message_id = strstr(h, ": ") + 2;
      else if (startswith(h, "Priority: "))          priority   = atoll(strstr(h, ": ") + 2);
      else if (startswith(h, "Modify ID: "))         modify_id  = (uint64_t)atoll(strstr(h, ": ") + 2);
    }
  
  
  /* Notify waiting client about a received message modification. */
  if (modifying != 0)
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
      if (xmalloc(multicast = recipient->modify_message, 1, mds_message_t))
	goto fail;
      multicast->headers = NULL;
      multicast->header_count = 0;
      multicast->payload = NULL;
      multicast->payload_size = 0;
      multicast->payload_ptr = 0;
      multicast->buffer = NULL;
      multicast->buffer_size = 0;
      multicast->buffer_ptr = 0;
      multicast->stage = 0;
      if (xmalloc(multicast->payload, message.payload_size, char))
	goto fail;
      memcpy(multicast->payload, message.payload, message.payload_size * sizeof(char));
      if (xmalloc(multicast->headers, message.header_count, char*))
	goto fail;
      for (i = 0; i < message.header_count; i++, multicast->header_count++)
	{
	  multicast->headers[i] = strdup(message.headers[i]);
	  if (multicast->headers[i] == NULL)
	    goto fail;
	}
      goto done;
    fail:
      if (multicast != NULL)
	{
	  mds_message_destroy(multicast);
	  free(multicast);
	  recipient->modify_message = NULL;
	}
    done:
      pthread_mutex_unlock(&(modify_mutex));
      with_mutex (client->modify_mutex, pthread_cond_signal(&(client->modify_cond)););
      
      /* Do nothing more, not not even multicast this message. */
      return 0;
    }
  
  
  if (message_id == NULL)
    {
      eprint("received message with a message ID, ignoring.");
      return 0;
    }
  
  /* Assign ID if not already assigned. */
  if (assign_id && (client->id == 0))
    {
      intercept |= 2;
      with_mutex_if (slave_mutex, (client->id = ++next_client_id) == 0,
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
      size_t size = 64; /* Atleast 25, otherwise we get into problems at ((intercept & 2)) */
      char* buf;
      if (xmalloc(buf, size + 1, char))
	{
	  perror(*argv);
	  return 0;
	}
      
      pthread_mutex_lock(&(client->mutex));
      if ((intercept & 1)) /* from payload */
	{
	  char* payload = client->message.payload;
	  size_t payload_size = client->message.payload_size;
	  if (client->message.payload_size == 0) /* All messages */
	    {
	      *buf = '\0';
	      add_intercept_condition(client, buf, priority, modifying, stop);
	    }
	  else /* Filtered messages */
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
			perror(*argv);
			free(old_buf);
			pthread_mutex_unlock(&(client->mutex));
			return 0;
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
	}
      if ((intercept & 2)) /* "To: $(client->id)" */
	{
	  snprintf(buf, size, "To: %" PRIu32 ":%" PRIu32,
		   (uint32_t)(client->id >> 32),
		   (uint32_t)(client->id >>  0));
	  add_intercept_condition(client, buf, priority, modifying, 0);
	}
      pthread_mutex_unlock(&(client->mutex));
      
      free(buf);
    }
  
  
  /* Multicast the message. */
  n = mds_message_compose_size(&message);
  if ((msgbuf = malloc(n)) == NULL)
    {
      perror(*argv);
      return 0;
    }
  mds_message_compose(&message, msgbuf);
  queue_message_multicast(msgbuf, n / sizeof(char), client);
  
  
  /* Send asigned ID. */
  if (assign_id)
    {
      char* msgbuf_;
      
      /* Construct response. */
      n = 2 * 10 + strlen(message_id) + 1;
      n += strlen("ID assignment: :\nIn response to: \n\n");
      if (xmalloc(msgbuf, n, char))
	{
	  perror(*argv);
	  return 0;
	}
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
      if (msgbuf_ == NULL)
	{
	  perror(*argv);
	  free(msgbuf);
	  return 0;
	}
      queue_message_multicast(msgbuf_, n, client);
      
      /* Queue message to be sent when this function returns.
         This done to simplify `multicast_message` for re-exec and termination. */
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
		      char* msg_new = realloc(client->send_pending, new_len * sizeof(char));
		      if (msg_new != NULL)
			{
			  memcpy(msg_new + client->send_pending_size, msgbuf, n * sizeof(char));
			  client->send_pending = msg_new;
			  client->send_pending_size = new_len;
			}
		      else
			perror(*argv);
		      free(msgbuf);
		    }
		  );
    }
  
  return 0;
}


/**
 * Compare two queued interceptors by priority
 * 
 * @param   a:const queued_interception_t*  One of the interceptors
 * @param   b:const queued_interception_t*  The other of the two interceptors
 * @return                                  Negative if a before b, positive if a after b, otherwise zero
 */
static int cmp_queued_interception(const void* a, const void* b)
{
  const queued_interception_t* p = b; /* Highest first, so swap them. */
  const queued_interception_t* q = a;
  return p->priority < q->priority ? -1 :
         p->priority > q->priority ? 1 : 0;
}


/**
 * Queue a message for multicasting
 * 
 * @param  message  The message
 * @param  length   The length of the message
 * @param  sender   The original sender of the message
 */
void queue_message_multicast(char* message, size_t length, client_t* sender)
{
  char* msg = message;
  size_t header_count = 0;
  size_t n = length - 1;
  size_t* hashes = NULL;
  char** headers = NULL;
  char** header_values = NULL;
  queued_interception_t* interceptions = NULL;
  size_t interceptions_count = 0;
  multicast_t* multicast = NULL;
  size_t i;
  uint64_t modify_id;
  char modify_id_header[13 + 3 * sizeof(uint64_t)];
  void* new_buf;
  
  /* Count the number of headers. */
  for (i = 0; i < n; i++)
    if (message[i] == '\n')
      {
	header_count++;
	if (message[i + 1] == '\n')
	  break;
      }
  
  if (header_count == 0)
    return; /* Invalid message. */
  
  /* Allocate multicast message. */
  fail_if (xmalloc(multicast, 1, multicast_t));
  multicast_initialise(multicast);
  
  /* Allocate header lists. */
  fail_if (xmalloc(hashes,        header_count, size_t));
  fail_if (xmalloc(headers,       header_count, char*));
  fail_if (xmalloc(header_values, header_count, char*));
  
  /* Populate header lists. */
  for (i = 0; i < header_count; i++)
    {
      char* end = strchr(msg, '\n');
      char* colon = strchr(msg, ':');
      
      *end = '\0';
      if ((header_values[i] = strdup(msg)) == NULL)
	{
	  header_count = i;
	  goto pfail;
	}
      *colon = '\0';
      if ((headers[i] = strdup(msg)) == NULL)
	{
	  free(headers[i]);
	  header_count = i;
	  goto pfail;
	}
      *colon = ':';
      *end = '\n';
      hashes[i] = string_hash(headers[i]);
      
      msg = end + 1;
    }
  
  /* Get intercepting clients. */
  pthread_mutex_lock(&(slave_mutex));
  interceptions = get_interceptors(sender, hashes, headers, header_values, header_count, &interceptions_count);
  pthread_mutex_unlock(&(slave_mutex));
  fail_if (interceptions == NULL);
  
  /* Sort interceptors. */
  qsort(interceptions, interceptions_count, sizeof(queued_interception_t), cmp_queued_interception);
  
  /* Add prefix to message with ‘Modify ID’ header. */
  with_mutex (slave_mutex,
	      modify_id = next_modify_id++;
	      if (next_modify_id == 0)
		next_modify_id = 1;
	      );
  xsnprintf(modify_id_header, "Modify ID: %" PRIu64 "\n", modify_id);
  n = strlen(modify_id_header);
  new_buf = message;
  fail_if (xrealloc(new_buf, n + length, char));
  message = new_buf;
  memmove(message + n, message, length * sizeof(char));
  memcpy(message, modify_id_header, n * sizeof(char));
  
  /* Store information. */
  multicast->interceptions = interceptions;
  multicast->interceptions_count = interceptions_count;
  multicast->message = message;
  multicast->message_length = length + n;
  multicast->message_prefix = n;
  message = NULL;
  
  /* Queue message multicasting. */
  with_mutex (sender->mutex,
	      new_buf = sender->multicasts;
	      if (xrealloc(new_buf, sender->multicasts_count + 1, multicast_t))
		goto fail_queue;
	      sender->multicasts = new_buf;
	      sender->multicasts[sender->multicasts_count++] = *multicast;
	      multicast = NULL;
	     fail_queue:
	      );
  
 fail: /* This is done before this function returns even if there was no error. */
  /* Release resources. */
  xfree(headers, header_count);
  xfree(header_values, header_count);
  free(hashes);
  free(message);
  if (multicast != NULL)
    multicast_destroy(multicast);
  free(multicast);
  return;
  
 pfail:
  perror(*argv);
  goto fail;
}


/**
 * Exec into the mdsinitrc script
 * 
 * @param  args  The arguments to the child process
 */
void run_initrc(char** args)
{
  char pathname[PATH_MAX];
  struct passwd* pwd;
  char* env;
  char* home;
  args[0] = pathname;
  
#define __exec(FORMAT, ...)  \
  xsnprintf(pathname, FORMAT, __VA_ARGS__);  execv(args[0], args)
  
  /* Test $XDG_CONFIG_HOME. */
  if ((env = getenv_nonempty("XDG_CONFIG_HOME")) != NULL)
    {
      __exec("%s/%s", env, INITRC_FILE);
    }
  
  /* Test $HOME. */
  if ((env = getenv_nonempty("HOME")) != NULL)
    {
      __exec("%s/.config/%s", env, INITRC_FILE);
      __exec("%s/.%s", env, INITRC_FILE);
    }
  
  /* Test ~. */
  pwd = getpwuid(getuid()); /* Ignore error. */
  if (pwd != NULL)
    {
      home = pwd->pw_dir;
      if ((home != NULL) && (*home != '\0'))
	{
	  __exec("%s/.config/%s", home, INITRC_FILE);
	  __exec("%s/.%s", home, INITRC_FILE);
	}
    }
  
  /* Test $XDG_CONFIG_DIRS. */
  if ((env = getenv_nonempty("XDG_CONFIG_DIRS")) != NULL)
    {
      char* begin = env;
      char* end;
      int len;
      for (;;)
	{
	  end = strchrnul(begin, ':');
	  len = (int)(end - begin);
	  if (len > 0)
	    {
	      __exec("%.*s/%s", len, begin, INITRC_FILE);
	    }
	  if (*end == '\0')
	    break;
	  begin = end + 1;
	}
    }
  
  /* Test /etc. */
  __exec("%s/%s", SYSCONFDIR, INITRC_FILE);

#undef __exec
  
  /* Everything failed. */
  eprintf("unable to run %s file, you might as well kill me.", INITRC_FILE);
}


/**
 * Marshal the server's state into a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int marshal_server(int fd)
{
  size_t list_size = linked_list_marshal_size(&client_list);
  size_t map_size = fd_table_marshal_size(&client_map);
  size_t list_elements = 0;
  size_t state_n = 0;
  char* state_buf = NULL;
  char* state_buf_;
  ssize_t node;
  
  
  /* Calculate the grand size of all client information. */
  for (node = client_list.edge;; list_elements++)
    {
      if ((node = client_list.next[node]) == client_list.edge)
	break;
      state_n += client_marshal_size((client_t*)(void*)(client_list.values[node]));
    }
  
  /* Add the size of the rest of the program's state. */
  state_n += sizeof(int) + sizeof(sig_atomic_t) + 2 * sizeof(uint64_t) + 2 * sizeof(size_t);
  state_n += list_elements * sizeof(size_t) + list_size + map_size;
  
  /* Allocate a buffer for all data except the client list and the client map. */
  state_buf = state_buf_ = malloc(state_n);
  if (state_buf == NULL)
    goto fail;
  
  
  /* Tell the new version of the program what version of the program it is marshalling. */
  buf_set_next(state_buf_, int, MDS_SERVER_VARS_VERSION);
  
  /* Marshal the miscellaneous state data. */
  buf_set_next(state_buf_, sig_atomic_t, running);
  buf_set_next(state_buf_, uint64_t, next_client_id);
  buf_set_next(state_buf_, uint64_t, next_modify_id);
  
  /* Tell the program how large the marshalled client list is and how any clients are marshalled. */
  buf_set_next(state_buf_, size_t, list_size);
  buf_set_next(state_buf_, size_t, list_elements);
  
  /* Marshal the clients. */
  foreach_linked_list_node (client_list, node)
    {
      /* Get the memory address of the client. */
      size_t value_address = client_list.values[node];
      /* Get the client's information. */
      client_t* value = (client_t*)(void*)value_address;
      
      /* Marshal the address, it is used the the client list and the client map, that will be marshalled. */
      buf_set_next(state_buf_, size_t, value_address);
      /* Marshal the client informationation. */
      state_buf_ += client_marshal(value, state_buf_) / sizeof(char);
    }
  
  /* Marshal the client list. */
  linked_list_marshal(&client_list, state_buf_);
  state_buf_ += list_size / sizeof(char);
  /* Marshal the client map. */
  fd_table_marshal(&client_map, state_buf_);
  
  /* Send the marshalled data into the file. */
  if (full_write(fd, state_buf, state_n) < 0)
    goto fail;
  free(state_buf);
  
  return 0;
  
 fail:
  perror(*argv);
  free(state_buf);
  return -1;
}


/**
 * Address translation table used by `unmarshal_server` and `remapper`
 */
static hash_table_t unmarshal_remap_map;

/**
 * Address translator for `unmarshal_server`
 * 
 * @param   old  The old address
 * @return       The new address
 */
static size_t unmarshal_remapper(size_t old)
{
  return hash_table_get(&unmarshal_remap_map, old);
}

/**
 * Unmarshal the server's state from a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int unmarshal_server(int fd)
{
  int with_error = 0;
  char* state_buf;
  char* state_buf_;
  size_t list_size;
  size_t list_elements;
  size_t i;
  ssize_t node;
  pthread_t slave_thread;
  
  
  /* Read the file. */
  if ((state_buf = state_buf_ = full_read(fd)) == NULL)
    {
      perror(*argv);
      return -1;
    }
  
  /* Create memory address remapping table. */
  if (hash_table_create(&unmarshal_remap_map))
    {
      perror(*argv);
      free(state_buf);
      hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
      return -1;
    }
  
  
  /* Get the marshal protocal version. Not needed, there is only the one version right now. */
  /* buf_get(state_buf_, int, 0, MDS_SERVER_VARS_VERSION); */
  buf_next(state_buf_, int, 1);
  
  /* Unmarshal the miscellaneous state data. */
  buf_get_next(state_buf_, sig_atomic_t, running);
  buf_get_next(state_buf_, uint64_t, next_client_id);
  buf_get_next(state_buf_, uint64_t, next_modify_id);
  
  /* Get the marshalled size of the client list and how any clients that are marshalled. */
  buf_get_next(state_buf_, size_t, list_size);
  buf_get_next(state_buf_, size_t, list_elements);
  
  /* Unmarshal the clients. */
  for (i = 0; i < list_elements; i++)
    {
      size_t n;
      size_t value_address;
      client_t* value;
      
      /* Allocate the client's information. */
      if (xmalloc(value, 1, client_t))
	goto clients_fail;
      
      /* Unmarshal the address, it is used the the client list and the client map, that are also marshalled. */
      buf_get_next(state_buf_, size_t, value_address);
      /* Unmarshal the client information. */
      n = client_unmarshal(value, state_buf_);
      if (n == 0)
	goto clients_fail;
      
      /* Populate the remapping table. */
      if (hash_table_put(&unmarshal_remap_map, value_address, (size_t)(void*)value) == 0)
	if (errno)
	  goto clients_fail;
      
      /* Delayed seeking. */
      state_buf_ += n / sizeof(char);
      
      
      /* On error, seek past all clients. */
      continue;
    clients_fail:
      perror(*argv);
      with_error = 1;
      if (value != NULL)
	{
	  buf_prev(state_buf_, size_t, 1);
	  free(value);
	}
      for (; i < list_elements; i++)
	/* There is not need to close the sockets, it is done by
	   the caller because there are conditions where we cannot
	   get here anyway. */
	state_buf_ += client_unmarshal_skip(state_buf_) / sizeof(char);
      break;
    }
  
  /* Unmarshal the client list. */
  if (linked_list_unmarshal(&client_list, state_buf_))
    goto critical_fail;
  state_buf_ += list_size / sizeof(char);
  
  /* Unmarshal the client map. */
  if (fd_table_unmarshal(&client_map, state_buf_, unmarshal_remapper))
    goto critical_fail;
  
  /* Release the raw data. */
  free(state_buf);
  
  /* Remove non-found elements from the fd table. */
#define __bit(I, _OP_)  client_map.used[I / 64] _OP_ ((uint64_t)1 << (I % 64))
  if (with_error)
    for (i = 0; i < client_map.capacity; i++)
      if ((__bit(i, &)) && (client_map.values[i] == 0))
	/* Lets not presume that fd-table actually initialise its allocations. */
	__bit(i, &= ~);
#undef __bit
  
  /* Remap the linked list and remove non-found elements, and start the clients. */
  foreach_linked_list_node (client_list, node)
    {
      /* Remap the linked list and remove non-found elements. */
      size_t new_address = unmarshal_remapper(client_list.values[node]);
      client_list.values[node] = new_address;
      if (new_address == 0) /* Returned if missing (or if the address is the invalid NULL.) */
	linked_list_remove(&client_list, node);
      else
	{
	  /* Start the clients. (Errors do not need to be reported.) */
	  client_t* client = (client_t*)(void*)new_address;
	  int socket_fd = client->socket_fd;
	  
	  /* Increase number of running slaves. */
	  with_mutex (slave_mutex, running_slaves++;);
	  
	  /* Start slave thread. */
	  create_slave(&slave_thread, socket_fd);
	}
    }
  
  /* Release the remapping table's resources. */
  hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
  
  return -with_error;

 critical_fail:
  perror(*argv);
  abort();
}


/**
 * Create, start and detache a slave thread
 * 
 * @param   thread     The address at where to store the thread
 * @param   socket_fd  The file descriptor of the slave's socket
 * @return             Zero on success, -1 on error, error message will have been printed
 */
int create_slave(pthread_t* thread_slot, int socket_fd)
{
  if ((errno = pthread_create(thread_slot, NULL, slave_loop, (void*)(intptr_t)socket_fd)))
    {
      perror(*argv);
      with_mutex (slave_mutex, running_slaves--;);
      return -1;
    }
  if ((errno = pthread_detach(*thread_slot)))
    {
      perror(*argv);
      return -1;
    }
  return 0;
}

