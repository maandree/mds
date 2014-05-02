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
#include "config.h"

#include <libmdsserver/linked-list.h>
#include <libmdsserver/fd-table.h>
#include <libmdsserver/mds-message.h>

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
 * The program run state, 1 when running,
 * 0 when shutting down
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
 * Map from client socket file descriptor to all information (client_t)
 */
static fd_table_t client_map;

/**
 * List of client information (client_t)
 */
static linked_list_t client_list;



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
  int unparsed_args_ptr = 1;
  char* unparsed_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  int i;
  pthread_t _slave_thread;
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Drop privileges like it's hot. */
  if ((geteuid() == getuid() ? 0 : seteuid(getuid())) ||
      (getegid() == getgid() ? 0 : setegid(getgid())))
    {
      perror(*argv);
      return 1;
    }
  
  
  /* Sanity check the number of command line arguments. */
  if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT)
    {
      fprintf(stderr,
	      "%s: that number of arguments is ridiculous, I will not allow it.\n",
	      *argv);
      return 1;
    }
  
  
  /* TODO: support re-exec */
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (!strcmp(arg, "--initial-spawn")) /* Initial spawn? */
	if (is_respawn == 1)
	  {
	    fprintf(stderr,
		    "%s: conflicting arguments %s and %s cannot be combined.\n",
		    *argv, "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 0;
      else if (!strcmp(arg, "--respawn")) /* Respawning after crash? */
	if (is_respawn == 0)
	  {
	    fprintf(stderr,
		    "%s: conflicting arguments %s and %s cannot be combined.\n",
		    *argv, "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 1;
      else if (strstr(arg, "--socket-fd=") == arg) /* Socket file descriptor. */
	{
	  long int r;
	  char* endptr;
	  if (socket_fd != -1)
	    {
	      fprintf(stderr, "%s: duplicate declaration of %s.\n", *argv, "--socket-fd");
	      return -1;
	    }
	  arg += strlen("--socket-fd=");
	  r = strtol(arg, &endptr, 10);
	  if ((*argv == '\0') || isspace(*argv) ||
	      (endptr - arg != (ssize_t)strlen(arg))
	      || (r < 0) || (r > INT_MAX))
	    {
	      fprintf(stderr, "%s: invalid value for %s: %s.\n", *argv, "--socket-fd", arg);
	      return 1;
	    }
	  socket_fd = (int)r;
	}
      else
	/* Not recognised, it is probably for another server. */
	unparsed_args[unparsed_args_ptr++] = arg;
    }
  unparsed_args[unparsed_args_ptr] = NULL;
  
  
  /* Check that manditory arguments have been specified. */
  if (is_respawn < 0)
    {
      fprintf(stderr,
	      "%s: missing state argument, require either %s or %s.\n",
	      *argv, "--initial-spawn", "--respawn");
      return 1;
    }
  if (socket_fd < 0)
    {
      fprintf(stderr, "%s: missing socket file descriptor argument.\n", *argv);
      return 1;
    }
  
  
  if (is_respawn == 0)
    {
      pid_t pid;
      
      /* Run mdsinitrc. */
      pid = fork();
      if (pid == (pid_t)-1)
	{
	  perror(*argv);
	  return 1;
	}
      if (pid == 0) /* Child process exec:s, the parent continues without waiting for it. */
	{
	  run_initrc(unparsed_args);
	  return 1;
	}
    }
  
  
  /* Create list and table of clients. */
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
  
  
  /* Make the server update without all slaves dying on SIGUSR1. */
  {
    struct sigaction action;
    sigset_t sigset;
    
    sigemptyset(&sigset);
    action.sa_handler = sigusr1_trap;
    action.sa_mask = sigset;
    action.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &action, NULL) < 0)
      {
	perror(*argv);
	fd_table_destroy(&client_map, NULL, NULL);
	linked_list_destroy(&client_list);
	return 1;
      }
  }
  
  
  /* Create mutex and condition for slave counter. */
  pthread_mutex_init(&slave_mutex, NULL);
  pthread_cond_init(&slave_cond, NULL);
  
  
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
      pthread_mutex_lock(&slave_mutex);
      running_slaves++;
      pthread_mutex_unlock(&slave_mutex);
      
      /* Start slave thread. */
      errno = pthread_create(&_slave_thread, NULL, slave_loop, (void*)(intptr_t)client_fd);
      if (errno)
	{
	  perror(*argv);
	  pthread_mutex_lock(&slave_mutex);
	  running_slaves--;
	  pthread_mutex_unlock(&slave_mutex);
	}
    }
  if (reexecing)
    goto reexec;
  
  
  /* Wait for all slaves to close. */
  pthread_mutex_lock(&slave_mutex);
  while (running_slaves > 0)
    pthread_cond_wait(&slave_cond, &slave_mutex);
  pthread_mutex_unlock(&slave_mutex);
  
  
  /* Release resources. */
  fd_table_destroy(&client_map, NULL, NULL);
  linked_list_destroy(&client_list);
  pthread_mutex_destroy(&slave_mutex);
  pthread_cond_destroy(&slave_cond);
  
  return 0;
  
  
 reexec:
  {
    char* state_buf = NULL;
    char* state_buf_;
    size_t state_n;
    ssize_t wrote;
    int pipe_rw[2];
    char readlink_buf[PATH_MAX];
    ssize_t readlink_ptr;
    char** reexec_args;
    char** reexec_args_;
    char reexec_arg[24];
    size_t list_size;
    size_t map_size;
    size_t list_elements;
    size_t msg_size;
    ssize_t node;
    
#if INT_MAX > UINT64_MAX
# error It seems int:s are really big, you might need to increase the size of reexec_arg.
#endif
    
    /* Release resources. */
    pthread_mutex_destroy(&slave_mutex);
    pthread_cond_destroy(&slave_cond);
    
    /* Join with all slaves threads. */
    pthread_mutex_lock(&slave_mutex);
    while (running_slaves > 0)
      pthread_cond_wait(&slave_cond, &slave_mutex);
    pthread_mutex_unlock(&slave_mutex);
    
    /* Marshal the state of the server. */
    list_size = linked_list_marshal_size(&client_list);
    map_size = fd_table_marshal_size(&client_map);
    list_elements = 0;
    msg_size = 0;
    for (node = client_list.edge;; list_elements++)
      {
	mds_message_t message;
	if ((node = client_list.next[node]) == client_list.edge)
	  break;
	message = ((client_t*)(void*)(client_list.values[node]))->message;
	msg_size += mds_message_marshal_size(&message, 1);
      }
    state_n = sizeof(ssize_t) + 2 * sizeof(int) + 2 * sizeof(size_t);
    state_n *= list_elements;
    state_n += msg_size;
    state_n += 2 * sizeof(int) + 1 * sizeof(sig_atomic_t) + 3 * sizeof(size_t);
    state_buf = malloc(state_n);
    state_buf_ = state_buf;
    ((int*)state_buf_)[0] = MDS_SERVER_VARS_VERSION;
    ((int*)state_buf_)[1] = socket_fd;
    state_buf_ += 2 * sizeof(int) / sizeof(char);
    ((sig_atomic_t*)state_buf_)[0] = running;
    state_buf_ += 1 * sizeof(sig_atomic_t) / sizeof(char);
    ((size_t*)state_buf_)[0] = list_size;
    ((size_t*)state_buf_)[1] = map_size;
    ((size_t*)state_buf_)[2] = list_elements;
    state_buf_ += 3 * sizeof(size_t) / sizeof(char);
    for (node = client_list.edge;;)
      {
	size_t value_address;
	client_t* value;
	if ((node = client_list.next[node]) == client_list.edge)
	  break;
	value_address = client_list.values[node];
	value = (client_t*)(void*)value_address;
	msg_size = mds_message_marshal_size(&(value->message), 1);
	((size_t*)state_buf_)[0] = value_address;
	((ssize_t*)state_buf_)[1] = value->list_entry;
	((size_t*)state_buf_)[2] = msg_size;
	state_buf_ += 3 * sizeof(size_t) / sizeof(char);
	((int*)state_buf_)[0] = value->socket_fd;
	((int*)state_buf_)[1] = value->open;
	state_buf_ += 2 * sizeof(int) / sizeof(char);
	mds_message_marshal(&(value->message), state_buf_, 1);
	state_buf_ += msg_size / sizeof(char);
      }
    if (pipe(pipe_rw) < 0)
      goto reexec_fail;
    while (state_n > 0)
      {
	errno = 0;
	wrote = write(pipe_rw[1], state_buf, state_n);
	if (errno && (errno != EINTR))
	  goto reexec_fail;
	state_n -= (size_t)(wrote < 0 ? 0 : wrote);
	state_buf += (size_t)(wrote < 0 ? 0 : wrote);
      }
    free(state_buf);
    state_buf = malloc(list_size);
    if (state_buf == NULL)
      goto reexec_fail;
    linked_list_marshal(&client_list, state_buf);
    while (list_size > 0)
      {
	errno = 0;
	wrote = write(pipe_rw[1], state_buf, list_size);
	if (errno && (errno != EINTR))
	  goto reexec_fail;
	list_size -= (size_t)(wrote < 0 ? 0 : wrote);
	state_buf += (size_t)(wrote < 0 ? 0 : wrote);
      }
    free(state_buf);
    state_buf = malloc(map_size);
    if (state_buf == NULL)
      goto reexec_fail;
    fd_table_marshal(&client_map, state_buf);
    while (map_size > 0)
      {
	errno = 0;
	wrote = write(pipe_rw[1], state_buf, map_size);
	if (errno && (errno != EINTR))
	  goto reexec_fail;
	map_size -= (size_t)(wrote < 0 ? 0 : wrote);
	state_buf += (size_t)(wrote < 0 ? 0 : wrote);
      }
    free(state_buf);
    close(pipe_rw[1]);
    
    /* Re-exec the server. */
    readlink_ptr = readlink("/proc/self/exe", readlink_buf, PATH_MAX - 1);
    if (readlink_ptr < 0)
      goto reexec_fail;
    /* ‘readlink() does not append a null byte to buf.’ */
    readlink_buf[readlink_ptr] = '\0';
    snprintf(reexec_arg, sizeof(reexec_arg) / sizeof(char),
	     "--re-exec=%i", pipe_rw[0]);
    reexec_args = alloca(((size_t)argc + 2) * sizeof(char*));
    reexec_args_ = reexec_args;
    *reexec_args_++ = *argv;
    *reexec_args_ = reexec_arg;
    for (i = 1; i < argc; i++)
      reexec_args_[i] = argv[i];
    reexec_args_[argc] = NULL;
    execv(readlink_buf, reexec_args);
    
  reexec_fail:
    perror(*argv);
    if (state_buf != NULL)
      free(state_buf);
    close(pipe_rw[0]);
    close(pipe_rw[1]);
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
  client_t* information = NULL;
  size_t tmp;
  int r;
  
  
  /* Make the server update without all slaves dying on SIGUSR1. */
  {
    struct sigaction action;
    sigset_t sigset;
    
    sigemptyset(&sigset);
    action.sa_handler = sigusr1_trap;
    action.sa_mask = sigset;
    action.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &action, NULL) < 0)
      {
	perror(*argv);
	goto fail;
      }
  }
  
  
  /* Create information table. */
  information = malloc(sizeof(client_t));
  if (information == NULL)
    {
      perror(*argv);
      goto fail;
    }
  
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
  if (mds_message_initialise(&(information->message)))
    {
      perror(*argv);
      goto fail;
    }
  
  
  /* Fetch messages from the slave. */
  while (reexecing == 0)
    {
      r = mds_message_read(&(information->message), socket_fd);
      if (r == 0)
	{
	  /* TODO */
	}
      else
	if (r == -2)
	  {
	    fprintf(stderr, "%s: corrupt message received.\n", *argv);
	    goto fail;
	  }
	else if (errno == ECONNRESET)
	  {
	    r = mds_message_read(&(information->message), socket_fd);
	    information->open = 0;
	    if (r == 0)
	      {
		/* TODO */
	      }
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
  
  
 fail:
  /* Close socket and free resources. */
  close(socket_fd);
  if (information != NULL)
    {
      mds_message_destroy(&(information->message));
      free(information);
    }
  fd_table_remove(&client_map, socket_fd);
  
  /* Unlist client and decrease the slave count. */
  pthread_mutex_lock(&slave_mutex);
  if (entry != LINKED_LIST_UNUSED)
    linked_list_remove(&client_list, entry);
  running_slaves--;
  pthread_cond_signal(&slave_cond);
  pthread_mutex_unlock(&slave_mutex);
  
  return NULL;
  
  
 reexec:
  /* Tell the master thread that the slave has closed,
     this is done because re-exec causes a race-condition
     between the acception of a slave and the execution
     of the the slave thread. */
  pthread_mutex_lock(&slave_mutex);
  running_slaves--;
  pthread_cond_signal(&slave_cond);
  pthread_mutex_unlock(&slave_mutex);
  
  return NULL;
}


/**
 * Read an environment variable, but handle it as undefined if empty
 * 
 * @param   var  The environment variable's name
 * @return       The environment variable's value, `NULL` if empty or not defined
 */
char* getenv_nonempty(const char* var)
{
  char* rc = getenv(var);
  if ((rc == NULL) || (*rc == '\0'))
    return NULL;
  return rc;
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
  
  
  /* Test $XDG_CONFIG_HOME. */
  if ((env = getenv_nonempty("XDG_CONFIG_HOME")) != NULL)
    {
      snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/.%s", env, INITRC_FILE);
      execv(args[0], args);
    }
  
  /* Test $HOME. */
  if ((env = getenv_nonempty("HOME")) != NULL)
    {
      snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/.config/%s", env, INITRC_FILE);
      execv(args[0], args);
      
      snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/.%s", env, INITRC_FILE);
      execv(args[0], args);
    }
  
  /* Test ~. */
  pwd = getpwuid(getuid()); /* Ignore error. */
  if (pwd != NULL)
    {
      home = pwd->pw_dir;
      if ((home != NULL) && (*home != '\0'))
	{
	  snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/.config/%s", home, INITRC_FILE);
	  execv(args[0], args);
	  
	  snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/.%s", home, INITRC_FILE);
	  execv(args[0], args);
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
	      snprintf(pathname, sizeof(pathname) / sizeof(char), "%.*s/%s", len, begin, INITRC_FILE);
	      execv(args[0], args);
	    }
	  if (*end == '\0')
	    break;
	  begin = end + 1;
	}
    }
  
  /* Test /etc. */
  snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/%s", SYSCONFDIR, INITRC_FILE);
  execv(args[0], args);
  
  
  /* Everything failed. */
  fprintf(stderr, "%s: unable to run %s file, you might as well kill me.\n", *argv, INITRC_FILE);
}


/**
 * Called with the signal SIGUSR1 is caught.
 * This function should cue a re-exec of the program.
 * 
 * @param  signo  The caught signal
 */
void sigusr1_trap(int signo __attribute__((unused)))
{
  if (reexecing == 0)
    {
      reexecing = 1;
      /* TODO send the signal to all threads. */
    }
}

