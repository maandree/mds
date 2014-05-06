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
    
    /* Release resources. */
    pthread_mutex_destroy(&slave_mutex);
    pthread_cond_destroy(&slave_cond);
    
    /* Join with all slaves threads. */
    with_mutex(slave_mutex,
	       while (running_slaves > 0)
		 pthread_cond_wait(&slave_cond, &slave_mutex););
    
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
    }
  
  
  /* Store the thread so that other threads can kill it. */
  information->thread = pthread_self();
  
  
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
  
  
  /* Fetch messages from the slave. */
  if (information->open)
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
	      eprint("corrupt message received.");
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
  
  
 fail: /* The loop does break, this done on success as well. */
  /* Close socket and free resources. */
  close(socket_fd);
  if (information != NULL)
    {
      mds_message_destroy(&(information->message));
      free(information);
    }
  fd_table_remove(&client_map, socket_fd);
  
  /* Unlist client and decrease the slave count. */
  with_mutex(slave_mutex,
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
  
  
  /* Calculate the grand size of all messages and their buffers. */
  for (node = client_list.edge;; list_elements++)
    {
      mds_message_t message;
      if ((node = client_list.next[node]) == client_list.edge)
	break;
      message = ((client_t*)(void*)(client_list.values[node]))->message;
      msg_size += mds_message_marshal_size(&message, 1);
    }
  
  /* Calculate the grand size of all client information. */
  state_n = sizeof(ssize_t) + 1 * sizeof(int) + 2 * sizeof(size_t);
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
  
  /* Marshal the program's running–exit state. */
  buf_set_next(state_buf_, sig_atomic_t, running);
  
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
  
  /* Unmarshal the program's running–exit state. */
  buf_get_next(state_buf_, sig_atomic_t, running);
  
  /* Get the marshalled size of the client list and how any clients that are marshalled. */
  buf_get_next(state_buf_, size_t, list_size);
  buf_get_next(state_buf_, size_t, list_elements);
  
  /* Unmarshal the clients. */
  for (i = 0; i < list_elements; i++)
    {
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
      /* Unmarshal the message. */
      if (mds_message_unmarshal(&(value->message), state_buf_))
	{
	  perror(*argv);
	  mds_message_destroy(&(value->message));
	  free(value);
	  buf_prev(state_buf_, int, 2);
	  buf_prev(state_buf_, size_t, 3);
	  goto clients_fail;
	}
      state_buf_ += msg_size / sizeof(char);
      
      /* Populate the remapping table. */
      hash_table_put(&unmarshal_remap_map, value_address, (size_t)(void*)value);
      
      
      /* On error, seek past all clients. */
      continue;
    clients_fail:
      with_error = 1;
      for (; i < list_elements; i++)
	{
	  /* There is not need to close the sockets, it is done by
	     the caller because there are conditions where we cannot
	     get here anyway. */
	  msg_size = ((size_t*)state_buf_)[1];
	  buf_next(state_buf_, size_t, 3);
	  buf_next(state_buf_, int, 2);
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

