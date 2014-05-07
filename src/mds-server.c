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

#include <libmdsserver/config.h>
#include <libmdsserver/linked-list.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/fd-table.h>
#include <libmdsserver/mds-message.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/hash-help.h>

#include <alloca.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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



#define MDS_SERVER_VARS_VERSION  0


/**
 * Number of elements in `argv`
 */
static int argc;

/**
 * Command line arguments
 */
static char** argv;


/**
 * The program run state, 1 when running, 0 when shutting down
 */
static volatile sig_atomic_t running = 1;

/**
 * Non-zero when the program is about to re-exec
 */
static volatile sig_atomic_t reexecing = 0;

/**
 * The number of running slaves
 */
static int running_slaves = 0;

/**
 * Mutex for slave data
 */
static pthread_mutex_t slave_mutex;

/**
 * Condition for slave data
 */
static pthread_cond_t slave_cond;

/**
 * The thread that runs the master loop
 */
static pthread_t master_thread;

/**
 * Map from client socket file descriptor to all information (client_t)
 */
static fd_table_t client_map;

/**
 * List of client information (client_t)
 */
static linked_list_t client_list;

/**
 * The next free ID for a client
 */
static uint64_t next_id = 1;



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
  pthread_t _slave_thread;
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Drop privileges like it's hot. */
  if (drop_privileges())
    {
      perror(*argv);
      return 1;
    }
  
  
  /* Sanity check the number of command line arguments. */
  if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT)
    {
      eprint("that number of arguments is ridiculous, I will not allow it.");
      return 1;
    }
  
  
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (strequals(arg, "--initial-spawn")) /* Initial spawn? */
	if (is_respawn == 1)
	  {
	    eprintf("conflicting arguments %s and %s cannot be combined.",
		    "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 0;
      else if (strequals(arg, "--respawn")) /* Respawning after crash? */
	if (is_respawn == 0)
	  {
	    eprintf("conflicting arguments %s and %s cannot be combined.",
		    "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 1;
      else if (startswith(arg, "--socket-fd=")) /* Socket file descriptor. */
	{
	  long int r;
	  char* endptr;
	  if (socket_fd != -1)
	    {
	      eprintf("duplicate declaration of %s.", "--socket-fd");
	      return -1;
	    }
	  arg += strlen("--socket-fd=");
	  r = strtol(arg, &endptr, 10);
	  if ((*argv == '\0') || isspace(*argv) ||
	      (endptr - arg != (ssize_t)strlen(arg)) ||
	      (r < 0) || (r > INT_MAX))
	    {
	      eprintf("invalid value for %s: %s.", "--socket-fd", arg);
	      return 1;
	    }
	  socket_fd = (int)r;
	}
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	reexec = 1;
      else
	/* Not recognised, it is probably for another server. */
	unparsed_args[unparsed_args_ptr++] = arg;
    }
  unparsed_args[unparsed_args_ptr] = NULL;
  if (reexec)
    is_respawn = 1;
  
  
  /* Check that manditory arguments have been specified. */
  if (is_respawn < 0)
    {
      eprintf("missing state argument, require either %s or %s.",
	      "--initial-spawn", "--respawn");
      return 1;
    }
  if (socket_fd < 0)
    {
      eprint("missing socket file descriptor argument.");
      return 1;
    }
  
  
  /* Run mdsinitrc. */
  if (is_respawn == 0)
    {
      pid_t pid;
      
      pid = fork();
      if (pid == (pid_t)-1)
	{
	  perror(*argv);
	  return 1;
	}
      if (pid == 0) /* Child process exec:s, the parent continues without waiting for it. */
	{
	  /* Close all files except stdin, stdout and stderr. */
	  DIR* dir = opendir(SELF_FD);
	  struct dirent* file;
	  
	  if (dir == NULL)
	    perror(*argv); /* Well, that is just unfortunate, but we cannot really do anything. */
	  else
	    while ((file = readdir(dir)) != NULL)
	      if (strcmp(file->d_name, ".") && strcmp(file->d_name, ".."))
		{
		  int fd = atoi(file->d_name);
		  if (fd > 2)
		    close(fd);
		}
	  
	  closedir(dir);
	  close(socket_fd); /* Perhaps it is stdin, stdout or stderr. */
	  
	  /* Run initrc */
	  run_initrc(unparsed_args);
	  return 1;
	}
    }
  
  
  /* Create list and table of clients. */
  if (reexec == 0)
    {
      if (fd_table_create(&client_map))
	{
	  perror(*argv);
	  fd_table_destroy(&client_map, NULL, NULL);
	  return 1;
	}
      if (linked_list_create(&client_list, 32))
	{
	  perror(*argv);
	  fd_table_destroy(&client_map, NULL, NULL);
	  linked_list_destroy(&client_list);
	  return 1;
	}
    }
  
  
  /* Store the current thread so it can be killed from elsewhere. */
  master_thread = pthread_self();
  
  
  /* Make the server update without all slaves dying on SIGUSR1. */
  if (xsigaction(SIGUSR1, sigusr1_trap) < 0)
    {
      perror(*argv);
      fd_table_destroy(&client_map, NULL, NULL);
      linked_list_destroy(&client_list);
      return 1;
    }
  
  
  /* Create mutex and condition for slave counter. */
  pthread_mutex_init(&slave_mutex, NULL);
  pthread_cond_init(&slave_cond, NULL);
  
  
  /* Unmarshal the state of the server. */
  if (reexec)
    {
      pid_t pid = getpid();
      int reexec_fd, r;
      char shm_path[NAME_MAX + 1];
      xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
      reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
      if (reexec_fd < 0)
	{
	  perror(*argv);
	  r = -1;
	}
      else
	{
	  r = unmarshal_server(reexec_fd);
	  close(reexec_fd);
	  shm_unlink(shm_path);
	}
      if (r < 0)
	{
	  /* Close all files (hopefully sockets) we do not know what they are. */
	  DIR* dir = opendir(SELF_FD);
	  struct dirent* file;
	  
	  if (dir == NULL)
	    perror(*argv); /* Well, that is just unfortunate, but we cannot really do anything. */
	  else
	    while ((file = readdir(dir)) != NULL)
	      if (strcmp(file->d_name, ".") && strcmp(file->d_name, ".."))
		{
		  int fd = atoi(file->d_name);
		  if ((fd > 2) && (fd != socket_fd) &&
		      (fd_table_contains_key(&client_map, fd) == 0))
		    close(fd);
		}
	  
	  closedir(dir);
	}
    }
  
  /* Accepting incoming connections. */
  while (running && (reexecing == 0))
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
	      if (reexecing)
		goto reexec;
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
      with_mutex(slave_mutex, running_slaves++;);
      
      /* Start slave thread. */
      errno = pthread_create(&_slave_thread, NULL, slave_loop, (void*)(intptr_t)client_fd);
      if (errno)
	{
	  perror(*argv);
	  with_mutex(slave_mutex, running_slaves--;);
	}
    }
  if (reexecing)
    goto reexec;
  
  
  /* Wait for all slaves to close. */
  with_mutex(slave_mutex,
	     while (running_slaves > 0)
	       pthread_cond_wait(&slave_cond, &slave_mutex););
  
  
  /* Release resources. */
  fd_table_destroy(&client_map, NULL, NULL);
  linked_list_destroy(&client_list);
  pthread_mutex_destroy(&slave_mutex);
  pthread_cond_destroy(&slave_cond);
  
  return 0;
  
  
 reexec:
  {
    pid_t pid = getpid();
    int reexec_fd;
    char shm_path[NAME_MAX + 1];
    
    /* Join with all slaves threads. */
    with_mutex(slave_mutex,
	       while (running_slaves > 0)
		 pthread_cond_wait(&slave_cond, &slave_mutex););
    
    /* Release resources. */
    pthread_mutex_destroy(&slave_mutex);
    pthread_cond_destroy(&slave_cond);
    
    /* Marshal the state of the server. */
    xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
    reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
    if (reexec_fd < 0)
      {
	perror(*argv);
	return 1;
      }
    if (marshal_server(reexec_fd) < 0)
      goto reexec_fail;
    close(reexec_fd);
    reexec_fd = -1;
    
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
  int mutex_created = 0;
  char* msgbuf = NULL;
  size_t n;
  size_t tmp;
  int r;
  
  
  if (information == NULL)
    {
      /* Create information table. */
      information = malloc(sizeof(client_t));
      if (information == NULL)
	{
	  perror(*argv);
	  goto fail;
	}
      
      /* NULL-out pointers. */
      information->interception_conditions = NULL;
      
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
      pthread_mutex_unlock(&slave_mutex);
      if ((tmp == 0) && errno)
	{
	  perror(*argv);
	  goto fail;
	}
      
      /* Fill information table. */
      information->list_entry = entry;
      information->socket_fd = socket_fd;
      information->open = 1;
      information->id = 0;
      information->interception_conditions_count = 0;
      if (mds_message_initialise(&(information->message)))
	{
	  perror(*argv);
	  goto fail;
	}
    }
  
  
  /* Store the thread so that other threads can kill it. */
  information->thread = pthread_self();
  /* Create mutex to make sure two thread to not try to send
     messages concurrently, and other slave local actions. */
  pthread_mutex_init(&(information->mutex), NULL);
  mutex_created = 1;
  
  
  /* Make the server update without all slaves dying on SIGUSR1. */
  if (xsigaction(SIGUSR1, sigusr1_trap) < 0)
    {
      perror(*argv);
      goto fail;
    }
  
  
  /* Fetch messages from the slave. */
  if (information->open)
    while (reexecing == 0)
      {
	r = mds_message_read(&(information->message), socket_fd);
	if (r == 0)
	  message_received(information);
	else
	  if (r == -2)
	    {
	      eprint("corrupt message received.");
	      goto fail;
	    }
	  else if (errno == ECONNRESET)
	    {
	      r = mds_message_read(&(information->message), socket_fd);
	      information->open = 0;
	      if (r == 0)
		message_received(information);
	      /* Connection closed. */
	      break;
	    }
	  else if (errno == EINTR)
	    {
	      /* Stop the thread if we are re-exec:ing the server. */
	      if (reexecing)
		goto reexec;
	    }
	  else
	    {
	      perror(*argv);
	      goto fail;
	    }
      }
  /* Stop the thread if we are re-exec:ing the server. */
  if (reexecing)
    goto reexec;
  
  
  /* Multicast information about the client closing. */
  n = 2 * 10 + 1 + strlen("Client closed: :\n\n");
  snprintf(msgbuf, n,
	   "Client closed: %" PRIu32 ":%" PRIu32 "\n"
	   "\n",
	   (uint32_t)(information->id >> 32),
	   (uint32_t)(information->id >>  0));
  n = strlen(msgbuf) + 1;
  multicast_message(msgbuf, n);
  
  
 fail: /* The loop does break, this done on success as well. */
  /* Close socket and free resources. */
  close(socket_fd);
  if (msgbuf != NULL)
    free(msgbuf);
  if (information != NULL)
    {
      if (information->interception_conditions != NULL)
	{
	  size_t i;
	  for (i = 0; i < information->interception_conditions_count; i++)
	    if (information->interception_conditions[i].condition != NULL)
	      free(information->interception_conditions[i].condition);
	  free(information->interception_conditions);
	}
      if (mutex_created)
	pthread_mutex_destroy(&(information->mutex));
      mds_message_destroy(&(information->message));
      free(information);
    }
  
  /* Unlist client and decrease the slave count. */
  with_mutex(slave_mutex,
	     fd_table_remove(&client_map, socket_fd);
	     if (entry != LINKED_LIST_UNUSED)
	       linked_list_remove(&client_list, entry);
	     running_slaves--;
	     pthread_cond_signal(&slave_cond););
  
  return NULL;
  
  
 reexec:
  /* Tell the master thread that the slave has closed,
     this is done because re-exec causes a race-condition
     between the acception of a slave and the execution
     of the the slave thread. */
  with_mutex(slave_mutex,
	     running_slaves--;
	     pthread_cond_signal(&slave_cond););
  
  return NULL;
}


/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param  client  The client has sent a message
 */
void message_received(client_t* client)
{
  mds_message_t message = client->message;
  int assign_id = 0;
  int modifying = 0;
  int intercept = 0;
  int64_t priority = 0;
  int stop = 0;
  const char* message_id = NULL;
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
    }
  
  /* Ignore message if not labelled with a message ID. */
  if (message_id == NULL)
    {
      eprint("received message with a message ID, ignoring.");
      return;
    }
  
  /* Assign ID if not already assigned. */
  if (assign_id && (client->id == 0))
    {
      intercept |= 2;
      with_mutex(slave_mutex,
		 client->id = next_id++;
		 if (next_id == 0)
		   {
		     eprint("this is impossible, ID counter has overflowed.");
		     /* If the program ran for a millennium it would
			take c:a 585 assignments per nanosecond. This
			cannot possibly happen. (It would require serious
			dedication by generations of ponies (or just an alicorn)
			to maintain the process and transfer it new hardware.) */
		     abort();
		   }
		 );
    }
  
  /* Make the client listen for messages addressed to it. */
  if (intercept)
    {
      size_t size = 64; /* Atleast 25, otherwise we get into problems at ((intercept & 2)) */
      char* buf = malloc((size + 1) * sizeof(char));
      if (buf == NULL)
	{
	  perror(*argv);
	  return;
	}
      
      pthread_mutex_lock(&(client->mutex));
      if ((intercept & 1)) /* from payload */
	{
	  char* payload = client->message.payload;
	  if (client->message.payload_size == 0) /* All messages */
	    {
	      *buf = '\0';
	      add_intercept_condition(client, buf, priority, modifying, stop);
	    }
	  else /* Filtered messages */
	    for (;;)
	      {
		char* end = strchrnul(payload, '\n');
		size_t len = (size_t)(end - payload);
		if (len == 0)
		  {
		    payload++;
		    break;
		  }
		if (len > size)
		  {
		    char* old_buf = buf;
		    buf = realloc(buf, ((size <<= 1) + 1) * sizeof(char));
		    if (buf == NULL)
		      {
			perror(*argv);
			free(old_buf);
			return;
		      }
		  }
		memcpy(buf, payload, len);
		buf[len] = '\0';
		add_intercept_condition(client, buf, priority, modifying, stop);
		if (*end == '\0')
		  break;
		payload = end + 1;
	      }
	}
      if ((intercept & 2)) /* "To: $(client->id)" */
	{
	  snprintf(buf, size, "To: %" PRIu32 ":%" PRIu32,
		   (uint32_t)(client->id >> 32),
		   (uint32_t)(client->id >>  0));
	  add_intercept_condition(client, buf, priority, modifying, stop);
	}
      pthread_mutex_unlock(&(client->mutex));
      
      free(buf);
    }
  
  
  /* Multicast the message. */
  n = mds_message_marshal_size(&message, 0);
  if ((msgbuf = malloc(n)) == NULL)
    {
      perror(*argv);
      return;
    }
  mds_message_marshal(&message, msgbuf, 0);
  multicast_message(msgbuf, n / sizeof(char));
  free(msgbuf);
  
  
  /* Send asigned ID. */
  if (assign_id)
    {
      size_t sent;
      
      /* Construct response. */
      n = 2 * 10 + strlen(message_id) + 1;
      n += strlen("ID assignment: :\nIn response to: \n\n");
      msgbuf = malloc(n * sizeof(char));
      if (msgbuf == NULL)
	{
	  perror(*argv);
	  return;
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
      multicast_message(msgbuf, n);
      
      /* Send message. */
      with_mutex(client->mutex,
		 while (n > 0)
		   {
		     sent = send_message(client->socket_fd, msgbuf, n);
		     if ((sent < n) && (errno != EINTR)) /* Ignore EINTR */
		       {
			 perror(*argv);
			 break;
		       }
		     n -= sent;
		     msgbuf += sent;
		   }
		 );
      free(msgbuf);
    }
}


/**
 * Add an interception condition for a client
 * 
 * @param  client     The client
 * @param  condition  The header, optionally with value, to look for, or empty (not `NULL`) for all messages
 * @param  priority   Interception priority
 * @param  modifying  Whether the client may modify the messages
 * @param  stop       Whether the condition should be removed rather than added
 */
void add_intercept_condition(client_t* client, char* condition, int64_t priority, int modifying, int stop)
{
  size_t n = client->interception_conditions_count;
  interception_condition_t* conds = client->interception_conditions;
  ssize_t nonmodifying = -1;
  char* header = condition;
  char* value;
  size_t hash;
  size_t i;
  
  if ((condition = strdup(condition)) == NULL)
    {
      perror(*argv);
      return;
    }
  
  if ((value = strchr(header, ':')) != NULL)
    {
      *value = '\0'; /* NUL-terminate header. */
      value += 2;    /* Skip over delimiter.  */
    }
  
  hash = string_hash(header);
  
  for (i = 0; i < n; i++)
    {
      if (conds[i].header_hash == hash)
	if (strequals(conds[i].condition, condition))
	  {
	    if (stop)
	      {
		/* Remove the condition from the list. */
		memmove(conds + i, conds + i + 1, --n - i);
		client->interception_conditions_count--;
		/* Diminish the list. */
		if (n == 0)
		  {
		    free(conds);
		    client->interception_conditions = NULL;
		  }
		else
		  {
		    conds = realloc(conds, n * sizeof(interception_condition_t));
		    if (conds == NULL)
		      perror(*argv);
		    else
		      client->interception_conditions = conds;
		  }
	      }
	    else
	      {
		/* Update parameters. */
		conds[i].priority = priority;
		conds[i].modifying = modifying;
		if (modifying && (nonmodifying >= 0))
		  {
		    /* Optimisation: put conditions that are modifying
		       at the beginning. When a client is intercepting
		       we most know if any satisfying condition is
		       modifying. With this optimisation the first
		       satisfying condition will tell us if there is
		       any satisfying condition that is modifying. */
		    interception_condition_t temp = conds[nonmodifying];
		    conds[nonmodifying] = conds[i];
		    conds[i] = temp;
		  }
	      }
	    return;
	  }
      /* Look for the first non-modifying, this is a part of the
         optimisation where we put all modifying conditions at the
	 beginning. */
      if ((nonmodifying < 0) && conds[i].modifying)
	nonmodifying = (ssize_t)i;
    }
  
  if (stop)
    eprint("client tried to stop intercepting messages that it does not intercept.");
  else
    {
      /* Grow the interception condition list. */
      if (conds == NULL)
	conds = malloc(1 * sizeof(interception_condition_t));
      else
	conds = realloc(conds, (n + 1) * sizeof(interception_condition_t));
      if (conds == NULL)
	{
	  perror(*argv);
	  return;
	}
      client->interception_conditions = conds; 
      /* Store condition. */
      client->interception_conditions_count++;
      conds[n].condition = condition;
      conds[n].header_hash = hash;
      conds[n].priority = priority;
      conds[n].modifying = modifying;
      if (modifying && (nonmodifying >= 0))
	{
	  /* Optimisation: put conditions that are modifying
	     at the beginning. When a client is intercepting
	     we most know if any satisfying condition is
	     modifying. With this optimisation the first
	     satisfying condition will tell us if there is
	     any satisfying condition that is modifying. */
	  interception_condition_t temp = conds[nonmodifying];
	  conds[nonmodifying] = conds[n];
	  conds[n] = temp;
	}
    }
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
  int64_t diff = p->priority - q->priority;
  return diff < 0 ? -1 : diff > 0 ? 1 : 0;
}


/**
 * Multicast a message
 * 
 * @param  message  The message
 * @param  length   The length of the message
 */
void multicast_message(char* message, size_t length)
{
  size_t header_count = 0;
  size_t n = length - 1;
  size_t* hashes = NULL;
  char** headers = NULL;
  char** header_values = NULL;
  queued_interception_t* interceptions = NULL;
  size_t interceptions_count = 0;
  size_t i;
  ssize_t node;
  
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
  
  /* Allocate header lists. */
  if ((hashes        = malloc(header_count * sizeof(size_t))) == NULL)  goto fail;
  if ((headers       = malloc(header_count * sizeof(char*)))  == NULL)  goto fail;
  if ((header_values = malloc(header_count * sizeof(char*)))  == NULL)  goto fail;
  
  /* Populate header lists. */
  for (i = 0; i < header_count; i++)
    {
      char* end = strchr(message, '\n');
      char* colon = strchr(message, ':');
      
      *end = '\0';
      if ((header_values[i] = strdup(message)) == NULL)
	{
	  header_count = i;
	  goto fail;
	}
      *colon = '\0';
      if ((headers[i] = strdup(message)) == NULL)
	{
	  free(headers[i]);
	  header_count = i;
	  goto fail;
	}
      *colon = ':';
      *end = '\n';
      hashes[i] = string_hash(headers[i]);
      
      message = end + 1;
    }
  
  /* Get intercepting clients. */
  with_mutex(slave_mutex,
	     /* Count clients. */
	     n = 0;
	     foreach_linked_list_node (client_list, node)
	       n++;
	     
	     /* Allocate interceptor list. */
	     interceptions = malloc(n * sizeof(queued_interception_t*));
	     
	     /* Search clients. */
	     if (interceptions != NULL)
	       foreach_linked_list_node (client_list, node)
		 {
		   client_t* client = (client_t*)(void*)(client_list.values[node]);
		   pthread_mutex_t mutex = client->mutex;
		   interception_condition_t* conds = client->interception_conditions;
		   int64_t priority = 0; /* Initialise to stop incorrect warning. */
		   int modifying = 0; /* Initialise to stop incorrect warning. */
		   size_t j;
		   
		   /* Look for a matching condition. */
		   n = client->interception_conditions_count;
		   with_mutex(mutex,
			      for (i = 0; i < n; i++)
				{
				  interception_condition_t* cond = conds + i;
				  for (j = 0; j < header_count; j++)
				    {
				      if (cond->header_hash == hashes[j])
					if ((*(cond->condition) == '\0') ||
					    strequals(cond->condition, headers[j]) ||
					    strequals(cond->condition, header_values[j]))
					  break;
				    }
				  if (j < header_count)
				    {
				      priority = cond->priority;
				      modifying = cond->modifying;
				      break;
				    }
				}
			      );
		   
		   /* List client of there was a matching condition. */
		   if (i < n)
		     {
		       interceptions[interceptions_count].client    = client;
		       interceptions[interceptions_count].priority  = priority;
		       interceptions[interceptions_count].modifying = modifying;
		       interceptions_count++;
		     }
		 }
	     );
  if (interceptions == NULL)
    goto fail;
  
  /* Sort interceptors. */
  qsort(interceptions, interceptions_count, sizeof(queued_interception_t), cmp_queued_interception);
  
  /* Send message to interceptors. */
  for (i = 0; i < interceptions_count; i++)
    {
      queued_interception_t client_ = interceptions[i];
      client_t* client = client_.client;
      char* msg = message;
      size_t sent;
      n = length;
      
      with_mutex(client->mutex,
		 while (n > 0)
		   {
		     sent = send_message(client->socket_fd, msg, n);
		     if ((sent < n) && (errno != EINTR)) /* Ignore EINTR */
		       {
			 perror(*argv);
			 break;
		       }
		     n -= sent;
		     msg += sent;
		   }
		 );
      
      if ((n > 0) && client_.modifying)
	{
	  /* TODO */
	}
    }
  
  
  errno = 0;
 fail: /* This is done before this function returns even if there was no error. */
  if (errno != 0)
    perror(*argv);
  /* Release resources. */
  for (i = 0; i < header_count; i++)
    {
      if (headers[i]       != NULL)  free(headers[i]);
      if (header_values[i] != NULL)  free(header_values[i]);
    }
  if (interceptions != NULL)  free(interceptions);
  if (headers       != NULL)  free(headers);
  if (header_values != NULL)  free(header_values);
  if (hashes        != NULL)  free(hashes);
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
 * Called with the signal SIGUSR1 is caught.
 * This function should cue a re-exec of the program.
 * 
 * @param  signo  The caught signal
 */
void sigusr1_trap(int signo)
{
  if (reexecing == 0)
    {
      pthread_t current_thread;
      ssize_t node;
      
      reexecing = 1;
      current_thread = pthread_self();
      
      if (pthread_equal(current_thread, master_thread) == 0)
	pthread_kill(master_thread, signo);
      
      with_mutex(slave_mutex,
		 foreach_linked_list_node (client_list, node)
		   {
		     client_t* value = (client_t*)(void*)(client_list.values[node]);
		     if (pthread_equal(current_thread, value->thread) == 0)
		       pthread_kill(value->thread, signo);
		   });
    }
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
  size_t msg_size = 0;
  char* state_buf = NULL;
  char* state_buf_;
  size_t state_n;
  ssize_t wrote;
  ssize_t node;
  size_t j, n;
  
  
  /* Calculate the grand size of all messages and their buffers. */
  for (node = client_list.edge;; list_elements++)
    {
      mds_message_t message;
      client_t* value;
      if ((node = client_list.next[node]) == client_list.edge)
	break;
      
      value = (client_t*)(void*)(client_list.values[node]);
      n = value->interception_conditions_count;
      message = value->message;
      msg_size += mds_message_marshal_size(&message, 1);
      msg_size += n * (sizeof(size_t) + sizeof(int64_t) + sizeof(int));
      
      for (j = 0; j < n; j++)
	msg_size += (strlen(value->interception_conditions[j].condition) + 1) * sizeof(char);
    }
  
  /* Calculate the grand size of all client information. */
  state_n = 5 + sizeof(size_t) + 2 * sizeof(int) + 1 * sizeof(uint64_t);
  state_n *= list_elements;
  state_n += msg_size;
  
  /* Add the size of the rest of the program's state. */
  state_n += 2 * sizeof(int) + 1 * sizeof(sig_atomic_t) + 2 * sizeof(size_t);
  
  /* Allocate a buffer for all data except the client list and the client map. */
  state_buf = state_buf_ = malloc(state_n);
  if (state_buf == NULL)
    goto fail;
  
  
  /* Tell the new version of the program what version of the program it is marshalling. */
  buf_set_next(state_buf_, int, MDS_SERVER_VARS_VERSION);
  
  /* Marshal the miscellaneous state data. */
  buf_set_next(state_buf_, sig_atomic_t, running);
  buf_set_next(state_buf_, uint64_t, next_id);
  
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
      
      /* Get the marshalled size of the message. */
      msg_size = mds_message_marshal_size(&(value->message), 1);
      
      /* Marshal the address, it is used the the client list and the client map, that will be marshalled. */
      buf_set_next(state_buf_, size_t, value_address);
      /* Tell the program how large the marshalled message is. */
      buf_set_next(state_buf_, size_t, msg_size);
      /* Marshal the client info. */
      buf_set_next(state_buf_, ssize_t, value->list_entry);
      buf_set_next(state_buf_, int, value->socket_fd);
      buf_set_next(state_buf_, int, value->open);
      buf_set_next(state_buf_, uint64_t, value->id);
      /* Marshal interception conditions. */
      buf_set_next(state_buf_, size_t, n = value->interception_conditions_count);
      for (j = 0; j < n; j++)
	{
	  interception_condition_t cond = value->interception_conditions[j];
	  memcpy(state_buf_, cond.condition, strlen(cond.condition) + 1);
	  buf_next(state_buf_, char, strlen(cond.condition) + 1);
	  buf_set_next(state_buf_, size_t, cond.header_hash);
	  buf_set_next(state_buf_, int64_t, cond.priority);
	  buf_set_next(state_buf_, int, cond.modifying);
	}
      /* Marshal the message. */
      mds_message_marshal(&(value->message), state_buf_, 1);
      state_buf_ += msg_size / sizeof(char);
    }
  
  
  /* Send the marshalled data into the file. */
  while (state_n > 0)
    {
      errno = 0;
      wrote = write(fd, state_buf, state_n);
      if (errno && (errno != EINTR))
	goto fail;
      state_n -= (size_t)max(wrote, 0);
      state_buf += (size_t)max(wrote, 0);
    }
  free(state_buf);
  
  /* Marshal, and send into the file, the client list. */
  state_buf = malloc(list_size);
  if (state_buf == NULL)
    goto fail;
  linked_list_marshal(&client_list, state_buf);
  while (list_size > 0)
    {
      errno = 0;
      wrote = write(fd, state_buf, list_size);
      if (errno && (errno != EINTR))
	goto fail;
      list_size -= (size_t)max(wrote, 0);
      state_buf += (size_t)max(wrote, 0);
    }
  free(state_buf);
  
  /* Marshal, and send into the file, the client map. */
  state_buf = malloc(map_size);
  if (state_buf == NULL)
    goto fail;
  fd_table_marshal(&client_map, state_buf);
  while (map_size > 0)
    {
      errno = 0;
      wrote = write(fd, state_buf, map_size);
      if (errno && (errno != EINTR))
	goto fail;
      map_size -= (size_t)max(wrote, 0);
      state_buf += (size_t)max(wrote, 0);
    }
  free(state_buf);
  
  return 0;
  
 fail:
  if (state_buf != NULL)
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
  size_t state_buf_size = 8 << 10;
  size_t state_buf_ptr = 0;
  ssize_t got;
  char* state_buf;
  char* state_buf_;
  size_t list_size;
  size_t list_elements;
  size_t i;
  ssize_t node;
  pthread_t _slave_thread;
  
  
  /* Allocate buffer for data. */
  state_buf = state_buf_ = malloc(state_buf_size * sizeof(char));
  if (state_buf == NULL)
    {
      perror(*argv);
      return -1;
    }
  
  /* Read the file. */
  for (;;)
    {
      /* Grow buffer if it is too small. */
      if (state_buf_size == state_buf_ptr)
	{
	  char* old_buf = state_buf;
	  state_buf = realloc(state_buf, (state_buf_size <<= 1) * sizeof(char));
	  if (state_buf == NULL)
	    {
	      perror(*argv);
	      free(old_buf);
	      return -1;
	    }
	}
      
      /* Read from the file into the buffer. */
      got = read(fd, state_buf + state_buf_ptr, state_buf_size - state_buf_ptr);
      if (got < 0)
	{
	  perror(*argv);
	  free(state_buf);
	  return -1;
	}
      if (got == 0)
	break;
      state_buf_ptr += (size_t)got;
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
  buf_get_next(state_buf_, uint64_t, next_id);
  
  /* Get the marshalled size of the client list and how any clients that are marshalled. */
  buf_get_next(state_buf_, size_t, list_size);
  buf_get_next(state_buf_, size_t, list_elements);
  
  /* Unmarshal the clients. */
  for (i = 0; i < list_elements; i++)
    {
      size_t seek = 0;
      size_t j = 0, n = 0;
      size_t value_address;
      size_t msg_size;
      client_t* value;
      
      /* Allocate the client's information. */
      if ((value = malloc(sizeof(client_t))) == NULL)
	{
	  perror(*argv);
	  goto clients_fail;
	}
      
      /* Unmarshal the address, it is used the the client list and the client map, that are also marshalled. */
      buf_get_next(state_buf_, size_t, value_address);
      /* Get the marshalled size of the message. */
      buf_get_next(state_buf_, size_t, msg_size);
      /* Unmarshal the client info. */
      buf_get_next(state_buf_, ssize_t, value->list_entry);
      buf_get_next(state_buf_, int, value->socket_fd);
      buf_get_next(state_buf_, int, value->open);
      buf_set_next(state_buf_, uint64_t, value->id);
      /* Unmarshal interception conditions. */
      buf_get_next(state_buf_, size_t, value->interception_conditions_count = n);
      seek = 5 * sizeof(size_t) + 2 * sizeof(int) + 1 * sizeof(uint64_t);
      value->interception_conditions = malloc(n * sizeof(interception_condition_t));
      if (value->interception_conditions == NULL)
	{
	  perror(*argv);
	  goto clients_fail;
	}
      for (j = 0; j < n; j++)
	{
	  interception_condition_t* cond = value->interception_conditions + j;
	  size_t m = strlen(state_buf_) + 1;
	  if ((cond->condition = malloc(m * sizeof(char))) == NULL)
	    {
	      perror(*argv);
	      goto clients_fail;
	    }
	  memcpy(cond->condition, state_buf_, m);
	  buf_next(state_buf_, char, m);
	  buf_get_next(state_buf_, size_t, cond->header_hash);
	  buf_get_next(state_buf_, int64_t, cond->priority);
	  buf_get_next(state_buf_, int, cond->modifying);
	  seek += m * sizeof(char) + sizeof(size_t) + sizeof(int64_t) + sizeof(int);
	}
      /* Unmarshal the message. */
      if (mds_message_unmarshal(&(value->message), state_buf_))
	{
	  perror(*argv);
	  mds_message_destroy(&(value->message));
	  goto clients_fail;
	}
      state_buf_ += msg_size / sizeof(char);
      
      /* Populate the remapping table. */
      hash_table_put(&unmarshal_remap_map, value_address, (size_t)(void*)value);
      
      
      /* On error, seek past all clients. */
      continue;
    clients_fail:
      with_error = 1;
      if (value != NULL)
	{
	  if (value->interception_conditions != NULL)
	    {
	      for (j = 0; j < n; j++)
		if (value->interception_conditions[j].condition != NULL)
		  free(value->interception_conditions[j].condition);
	      free(value->interception_conditions);
	    }
	  free(value);
	}
      state_buf_ -= seek / sizeof(char);
      for (; i < list_elements; i++)
	{
	  /* There is not need to close the sockets, it is done by
	     the caller because there are conditions where we cannot
	     get here anyway. */
	  msg_size = ((size_t*)state_buf_)[1];
	  buf_next(state_buf_, size_t, 4);
	  buf_next(state_buf_, int, 2);
	  buf_next(state_buf_, uint64_t, 1);
	  buf_get_next(state_buf_, size_t, n);
	  for (j = 0; j < n; j++)
	    {
	      buf_next(state_buf_, char, strlen(state_buf_) + 1);
	      buf_next(state_buf_, size_t, 1);
	      buf_next(state_buf_, int64_t, 1);
	      buf_next(state_buf_, int, 1);
	    }
	  state_buf_ += msg_size / sizeof(char);
	}
      break;
    }
  
  /* Unmarshal the client list. */
  linked_list_unmarshal(&client_list, state_buf_);
  state_buf_ += list_size / sizeof(char);
  
  /* Unmarshal the client map. */
  fd_table_unmarshal(&client_map, state_buf_, unmarshal_remapper);
  
  /* Release the raw data. */
  free(state_buf);
  
  /* Remove non-found elements from the fd table. */
  if (with_error)
    for (i = 0; i < client_map.capacity; i++)
      if (client_map.used[i / 64] & ((uint64_t)1 << (i % 64)))
	if (client_map.values[i] == 0) /* Lets not presume that fd-table actually initialise its allocations. */
	  client_map.used[i / 64] &= ~((uint64_t)1 << (i % 64));
  
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
	  with_mutex(slave_mutex, running_slaves++;);
	  
	  /* Start slave thread. */
	  errno = pthread_create(&_slave_thread, NULL, slave_loop, (void*)(intptr_t)socket_fd);
	  if (errno)
	    {
	      perror(*argv);
	      with_mutex(slave_mutex, running_slaves--;);
	    }
	}
    }
  
  /* Release the remapping table's resources. */
  hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
  
  return -with_error;
}

