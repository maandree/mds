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
#include "mds-base.h"

#include <libmdsserver/config.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>


#define  try(INSTRUCTION)  if ((r = INSTRUCTION))  goto fail


int argc = 0;
char** argv = NULL;
int is_respawn = 0;
int is_reexec = 0;
pthread_t master_thread;

volatile sig_atomic_t terminating = 0;
volatile sig_atomic_t reexecing = 0;

int socket_fd = -1;



/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
static int parse_cmdline(void)
{
  int i;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 2)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 2.
#endif
  
  
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
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	is_reexec = 1;
      else if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm((unsigned)min(atoi(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
    }
  if (is_reexec)
    {
      is_respawn = 1;
      eprint("re-exec performed.");
    }
  
  /* Check that manditory arguments have been specified. */
  exit_if (is_respawn < 0,
	   eprintf("missing state argument, require either %s or %s.",
		   "--initial-spawn", "--respawn"););
  
  return 0;
}


/**
 * Connect to the display
 * 
 * @return  Non-zero on error
 */
static int connect_to_display(void)
{
  char* display;
  char pathname[PATH_MAX];
  struct sockaddr_un address;
  
  display = getenv("MDS_DISPLAY");
  exit_if ((display == NULL) || (strchr(display, ':') == NULL),
	   eprint("MDS_DISPLAY has not set."););
  exit_if (display[0] != ':',
	   eprint("Remote mds sessions are not supported."););
  xsnprintf(pathname, "%s/%s.socket", MDS_RUNTIME_ROOT_DIRECTORY, display + 1);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, pathname);
  fail_if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0);
  fail_if (connect(socket_fd, (struct sockaddr*)(&address), sizeof(address)) < 0);
  
  return 0;
  
 pfail:
  perror(*argv);
  if (socket_fd >= 0)
    close(socket_fd);
  return 1;
}


/**
 * Unmarshal the server's saved state
 * 
 * @return  Non-zero on error
 */
static int base_unmarshal(void)
{
  pid_t pid = getpid();
  int reexec_fd, r;
  char shm_path[NAME_MAX + 1];
  char* state_buf;
  char* state_buf_;
  
  /* Acquire access to marshalled data. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
  reexec_fd = shm_open(shm_path, O_RDONLY, S_IRWXU);
  fail_if (reexec_fd < 0); /* Critical. */
  
  /* Read the state file. */
  fail_if ((state_buf = state_buf_ = full_read(reexec_fd)) == NULL);
  
  /* Release resources. */
  close(reexec_fd);
  shm_unlink(shm_path);
  
  
  /* Unmarshal state. */
  
  /* Get the marshal protocal version. Not needed, there is only the one version right now. */
  /* buf_get(state_buf_, int, 0, MDS_BASE_VARS_VERSION); */
  buf_next(state_buf_, int, 1);
  
  buf_get_next(state_buf_, int, socket_fd);
  r = unmarshal_server(state_buf_);
  
  
  /* Release resources. */
  free(state_buf);
  
  /* Recover after failure. */
  if (r)
    fail_if (reexec_failure_recover());
  
  return 0;
 pfail:
  perror(*argv);
  return 1;
}


/**
 * Marshal the server's state
 * 
 * @param   reexec_fd  The file descriptor of the file into which the state shall be saved
 * @return             Non-zero on error
 */
static int base_marshal(int reexec_fd)
{
  size_t state_n;
  char* state_buf;
  char* state_buf_;
  
  /* Calculate the size of the state data when it is marshalled. */
  state_n = 2 * sizeof(int);
  state_n += marshal_server_size();
  
  /* Allocate a buffer for all data. */
  state_buf = state_buf_ = malloc(state_n);
  fail_if (state_buf == NULL);
  
  
  /* Marshal the state of the server. */
  
  /* Tell the new version of the program what version of the program it is marshalling. */
  buf_set_next(state_buf_, int, MDS_BASE_VARS_VERSION);
  
  /* Store the state. */
  buf_set_next(state_buf_, int, socket_fd);
  fail_if (marshal_server(state_buf_));
  
  
  /* Send the marshalled data into the file. */
  fail_if (full_write(reexec_fd, state_buf, state_n) < 0);
  
  free(state_buf);
  return 0;
  
 pfail:
  perror(*argv);
  free(state_buf);
  return 1;
}


/**
 * Marshal and re-execute the server
 * 
 * This function only returns on error,
 * in which case the error will have been printed.
 */
static void perform_reexec(void)
{
  pid_t pid = getpid();
  int reexec_fd;
  char shm_path[NAME_MAX + 1];
  
  /* Marshal the state of the server. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
  reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
  if (reexec_fd < 0)
    {
      perror(*argv);
      return;
    }
  if (base_marshal(reexec_fd) < 0)
    goto fail;
  close(reexec_fd);
  reexec_fd = -1;
  
  /* Re-exec the server. */
  reexec_server(argc, argv, is_reexec);
  perror(*argv);
  
 fail:
  if (reexec_fd >= 0)
    close(reexec_fd);
  shm_unlink(shm_path);
}


/**
 * Entry point of the server
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  int r = 1;
  
  argc = argc_;
  argv = argv_;
  
  
  if (server_characteristics.require_privileges == 0)
    /* Drop privileges like it's hot. */
    fail_if (drop_privileges());
  
  
  /* Sanity check the number of command line arguments. */
  exit_if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT,
	   eprint("that number of arguments is ridiculous, I will not allow it."););
  
  /* Parse command line arguments. */
  try (parse_cmdline());
  
  
  /* Store the current thread so it can be killed from elsewhere. */
  master_thread = pthread_self();
  
  /* Set up signal traps for all especially handled signals. */
  trap_signals();
  
  
  /* Initialise the server. */
  try (preinitialise_server());
  
  if (is_reexec == 0)
    {
      if (server_characteristics.require_display)
	/* Connect to the display. */
	try (connect_to_display());
      
      /* Initialise the server. */
      try (initialise_server());
    }
  else
    {
      /* Unmarshal the server's saved state. */
      try (base_unmarshal());
    }
  
  /* Initialise the server. */
  try (postinitialise_server());
  
  
  /* Run the server. */
  try (master_loop());
  
  
  /* Re-exec server if signal to re-exec. */
  if (reexecing)
    {
      perform_reexec();
      goto fail;
    }
  
  close(socket_fd);
  return 0;
  
  
 pfail:
  perror(*argv);
  r = 1;
 fail:
  if (socket_fd >= 0)
    close(socket_fd);
  return r;
}


/**
 * Set up signal traps for all especially handled signals
 * 
 * @return  Non-zero on error
 */
int trap_signals(void)
{
  /* Make the server update without all slaves dying on SIGUSR1. */
  fail_if (xsigaction(SIGUSR1, received_reexec) < 0);
  
  /* Implement clean exit on SIGTERM. */
  fail_if (xsigaction(SIGTERM, received_terminate) < 0);
  
  /* Implement clean exit on SIGINT. */
  fail_if (xsigaction(SIGINT, received_terminate) < 0);
  
  return 0;
 pfail:
  return 1;
}


#undef try
