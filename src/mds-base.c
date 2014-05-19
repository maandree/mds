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
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


#define  try(INSTRUCTION)  if ((r = INSTRUCTION))  return r


int argc = 0;
char** argv = NULL;
int is_respawn = 0;
int is_reexec = 0;

int socket_fd = -1;
pthread_t master_thread;



/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
static int parse_cmdline(void)
{
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 2)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 2.
#endif
  
  int i;
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
 * Entry point of the server
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  int r;
  
  
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
  
  
  /* Connect to the display. */
  if (is_reexec == 0)
    try (connect_to_display());
  
  
  close(socket_fd);
  return 0;
  
 pfail:
  perror(*argv);
  if (socket_fd >= 0)
    close(socket_fd);
  return 1;
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

