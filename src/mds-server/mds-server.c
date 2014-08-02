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
#include "interception-condition.h"
#include "client.h"
#include "queued-interception.h"
#include "multicast.h"
#include "interceptors.h"
#include "sending.h"
#include "slavery.h"
#include "receiving.h"

#include <libmdsserver/config.h>
#include <libmdsserver/linked-list.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/fd-table.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/hash-help.h>

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/socket.h>
#include <dirent.h>
#include <inttypes.h>


/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 0,
    .require_display = 0, /* We will service one ourself. */
    .require_respawn_info = 1,
    .sanity_check_argc = 1
  };



#define __free(I)                                           \
  if (I >  0)  pthread_mutex_destroy(&slave_mutex);         \
  if (I >  1)  pthread_cond_destroy(&slave_cond);           \
  if (I >  2)  pthread_mutex_destroy(&modify_mutex);        \
  if (I >  3)  pthread_cond_destroy(&modify_cond);          \
  if (I >= 4)  hash_table_destroy(&modify_map, NULL, NULL); \
  if (I >= 5)  fd_table_destroy(&client_map, NULL, NULL);   \
  if (I >= 6)  linked_list_destroy(&client_list)
  
#define error_if(I, CONDITION)  \
  if (CONDITION)  { xperror(*argv); __free(I); return 1; }


/**
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int preinitialise_server(void)
{
  int unparsed_args_ptr = 1;
  char* unparsed_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  int i;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 3)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 3.
#endif
  
  
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (startswith(arg, "--socket-fd=")) /* Socket file descriptor. */
	{
	  exit_if (socket_fd != -1,
		   eprintf("duplicate declaration of %s.", "--socket-fd"););
	  exit_if (strict_atoi(arg += strlen("--socket-fd="), &socket_fd, 0, INT_MAX) < 0,
		   eprintf("invalid value for %s: %s.", "--socket-fd", arg););
	}
      else if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm((unsigned)min(atoi(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
      else
	if (!strequals(arg, "--initial-spawn") && !strequals(arg, "--respawn"))
	  /* Not recognised, it is probably for another server. */
	  unparsed_args[unparsed_args_ptr++] = arg;
    }
  unparsed_args[unparsed_args_ptr] = NULL;
  
  /* Check that manditory arguments have been specified. */
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
  
  
  /* Create mutex and condition for slave counter. */
  error_if (0, (errno = pthread_mutex_init(&slave_mutex, NULL)));
  error_if (1, (errno = pthread_cond_init(&slave_cond, NULL)));
  
  /* Create mutex, condition and map for message modification. */
  error_if (2, (errno = pthread_mutex_init(&modify_mutex, NULL)));
  error_if (3, (errno = pthread_cond_init(&modify_cond, NULL)));
  error_if (4, hash_table_create(&modify_map));
  
  
  return 0;
  
 pfail:
  xperror(*argv);
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
  /* Create list and table of clients. */
  error_if (5, fd_table_create(&client_map));
  error_if (6, linked_list_create(&client_list, 32));
  
  return 0;
}


/**
 * This function will be invoked after `initialise_server` (if not re-exec:ing)
 * or after `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) postinitialise_server(void)
{
  /* We do not need to initialise anything else. */
  return 0;
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  /* Accepting incoming connections. */
  while (running && (terminating == 0))
    if (accept_connection() == 1)
      break;
  
  /* Join with all slaves threads. */
  with_mutex (slave_mutex,
	      while (running_slaves > 0)
		pthread_cond_wait(&slave_cond, &slave_mutex););
  
  if (reexecing == 0)
    {
      /* Release resources. */
      __free(9999);
    }
  
  return 0;
}


#undef error_if
#undef __free


/**
 * Accept an incoming and start a slave thread for it
 * 
 * @return  Zero normally, 1 if terminating
 */
int accept_connection(void)
{
  pthread_t slave_thread;
  int client_fd;
  
  /* Accept connection. */
  client_fd = accept(socket_fd, NULL, NULL);
  if (client_fd >= 0)
    {
      /* Increase number of running slaves. */
      with_mutex (slave_mutex, running_slaves++;);
      
      /* Start slave thread. */
      create_slave(&slave_thread, client_fd);
    }
  else
    /* Handle errors and shutdown. */
    if ((errno == EINTR) && terminating) /* Interrupted for termination. */
      return 1;
    else if ((errno == ECONNABORTED) || (errno == EINVAL)) /* Closing. */
      running = 0;
    else if (errno != EINTR) /* Error. */
      xperror(*argv);
  return 0;
}


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Output data
 */
void* slave_loop(void* data)
{
  int slave_fd = (int)(intptr_t)data;
  size_t information_address = fd_table_get(&client_map, (size_t)slave_fd);
  client_t* information = (client_t*)(void*)information_address;
  char* msgbuf = NULL;
  char buf[] = "To: all";
  size_t n;
  int r;
  
  
  if (information == NULL) /* Did not re-exec. */
    {
      /* Initialise the client. */
      fail_if ((information = initialise_client(slave_fd)) == NULL);
      
      /* Register client to receive broadcasts. */
      add_intercept_condition(information, buf, 0, 0, 0);
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
  send_multicast_queue(information);
  
  
 terminate: /* This done on success as well. */
  if (reexecing)
    goto reexec;
  
 fail: /* This done on success as well. */
  /* Close socket and free resources. */
  close(slave_fd);
  free(msgbuf);
  if (information != NULL)
    {
      /* Unlist and free client. */
      with_mutex (slave_mutex, linked_list_remove(&client_list, information->list_entry););
      client_destroy(information);
    }
  
  /* Unmap client and decrease the slave count. */
  with_mutex (slave_mutex,
	      fd_table_remove(&client_map, slave_fd);
	      running_slaves--;
	      pthread_cond_signal(&slave_cond););
  return NULL;
  
  
 pfail:
  xperror(*argv);
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
		{
		  xperror(*argv);
		  goto fail_queue;
		}
	      sender->multicasts = new_buf;
	      sender->multicasts[sender->multicasts_count++] = *multicast;
	      free(multicast);
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
  xperror(*argv);
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

