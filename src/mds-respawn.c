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
#include "mds-respawn.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 0,
    .require_display = 0,
    .require_respawn_info = 1,
    .sanity_check_argc = 0
  };



/**
 * Do not respawn crashed servers that did not live this many seconds
 */
static int interval = 5;

/**
 * The number of servers managed by this process
 */
static size_t servers = 0;

/**
 * Command line arguments, for each server — concatenated, with NULL termination
 */
static const char** commands_args = NULL;

/**
 * Mapping elements in `commands_args` that are the first
 * argument for each server to run
 */
static const char*** commands = NULL;

/**
 * States of managed servers
 */
static server_state_t* states = NULL;



/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
int parse_cmdline(void)
{
  int i;
  size_t j, args = 0, stack = 0;
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm((unsigned)min(atoi(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
      else if (startswith(arg, "--interval="))
	interval = min(atoi(arg + strlen("--interval=")), 60); /* At most 1 minute. */
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	is_reexec = 1;
      else if (strequals(arg, "{"))
	servers += stack++ == 0 ? 1 : 0;
      else if (strequals(arg, "}"))
	{
	  exit_if (stack-- == 0, eprint("Terminating non-started command, aborting"););
	  exit_if (stack == 0 && strequals(argv[i - 1], "{"),
		   eprint("Zero argument command specified, aborting"););
	}
      else if (stack == 0)
	eprintf("Unrecognised option: %s, did you forget `='?", arg);
      else
	args++;
    }
  if (is_reexec)
    {
      is_respawn = 1;
      eprint("re-exec performed.");
    }
  
  exit_if (stack > 0, eprint("Non-terminated command specified, aborting"););
  exit_if (servers == 0, eprint("No programs to spawn, aborting"););
  
  fail_if (xmalloc(commands_args, args + servers, char*));
  fail_if (xmalloc(commands, servers, char**));
  fail_if (xmalloc(states, servers, server_state_t));
  
  for (i = 1, args = j = 0; i < argc; i++)
    {
      char* arg = argv[i];
      if (strequals(arg, "}"))  commands_args[args++] = --stack == 0 ? NULL : arg;
      else if (stack > 0)       commands_args[args++] = arg;
      else if (strequals(arg, "{") && (stack++ == 0))
	commands[j++] = commands_args + args;
    }
  
  return 0;
  
 pfail:
  perror(*argv);
  return 1;
}


/**
 * This function is called when a signal that
 * signals the program to respawn all
 * `DEAD_AND_BURIED` server is received
 * 
 * @param  signo  The signal that has been received
 */
static void received_revive(int signo)
{
  /* TODO */
  (void) signo;
  eprint("revive signal received.");
}


/**
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int preinitialise_server(void)
{
  /* Make the server revive all `DEAD_AND_BURIED` servers on SIGUSR2. */
  fail_if (xsigaction(SIGUSR2, received_revive) < 0);
  
  return 0;
 pfail:
  perror(*argv);
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
  size_t i;
  for (i = 0; i < servers; i++)
    {
      states[i].pid = 0;
      states[i].state = UNBORN;
    }
  return 0;
}


/**
 * This function will be invoked after `initialise_server` (if not re-exec:ing)
 * or after `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int postinitialise_server(void)
{
  size_t i, j;
  
  /* TODO Spawn `UNBORN` servers. */
  
  for (i = j = 0; j < servers; i++)
    if (commands_args[i] == NULL)
      j++;
    else if (strequals(commands_args[i], "--initial-spawn"))
      commands_args[i] = "--respawn";
  
  /* TODO Spawn `DEAD` and `DEAD_AND_BURIED` servers. */
  
  return 0;
}


/**
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
size_t marshal_server_size(void)
{
  /* TODO */
  return 0;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  /* TODO */
  (void) state_buf;
  return 0;
}


/**
 * Unmarshal server implementation specific data and update the servers state accordingly
 * 
 * On critical failure the program should `abort()` or exit by other means.
 * That is, do not let `reexec_failure_recover` run successfully, if it unrecoverable
 * error has occurred or one severe enough that it is better to simply respawn.
 * 
 * @param   state_buf  The marshalled data that as not been read already
 * @return             Non-zero on error
 */
int unmarshal_server(char* state_buf)
{
  /* TODO */
  (void) state_buf;
  return 0;
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int reexec_failure_recover(void)
{
  /* TODO */
  return 0;
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  /* TODO */
  return 0;
}

