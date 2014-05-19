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
#include "slavery.h"
#include "reexec.h"
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
      else if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm((unsigned)min(atoi(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
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
    complete_reexec(socket_fd);
  
  /* Accepting incoming connections. */
  while (running && (terminating == 0))
    if (accept_connection(socket_fd) == 1)
      break;
  
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
  perform_reexec(reexec);
  /* Returning non-zero is important, otherwise the server cannot
     be respawn if the re-exec fails. */
  
 pfail:
  perror(*argv);
  return 1;
}


/**
 * Accept an incoming and start a slave thread for it
 * 
 * @param   socket_fd  The file descriptor of the server socket
 * @return             Zero normally, 1 if terminating
 */
int accept_connection(int socket_fd)
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
      perror(*argv);
  return 0;
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
  size_t information_address = fd_table_get(&client_map, (size_t)socket_fd);
  client_t* information = (client_t*)(void*)information_address;
  char* msgbuf = NULL;
  size_t n;
  int r;
  
  
  /* Intiailsie the client. */
  if (information == NULL)
    fail_if ((information = initialise_client(socket_fd)) == NULL);
  
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
  close(socket_fd);
  free(msgbuf);
  if (information != NULL)
    {
      /* Unlist and free client. */
      with_mutex (slave_mutex, linked_list_remove(&client_list, information->list_entry););
      client_destroy(information);
    }
  
  /* Unmap client and decrease the slave count. */
  with_mutex (slave_mutex,
	      fd_table_remove(&client_map, socket_fd);
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
		  perror(*argv);
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

