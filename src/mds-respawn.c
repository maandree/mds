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
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>



#define MDS_RESPAWN_VARS_VERSION  0



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
    .sanity_check_argc = 0,
    .fork_for_safety = 0
  };



/**
 * Do not respawn crashed servers that did not live this many seconds
 */
static int interval = RESPAWN_TIME_LIMIT_SECONDS;

/**
 * The number of servers managed by this process
 */
static size_t servers = 0;

/**
 * Command line arguments, for each server — concatenated, with NULL termination
 */
static char** commands_args = NULL;

/**
 * Mapping elements in `commands_args` that are the first
 * argument for each server to run
 */
static char*** commands = NULL;

/**
 * States of managed servers
 */
static server_state_t* states = NULL;

/**
 * Whether a revive request has been received but not processed
 */
static volatile sig_atomic_t reviving = 0;

/**
 * The number of servers that are alive
 */
static size_t live_count = 0;



/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
int parse_cmdline(void)
{
  /* Parse command line arguments. */
  
  int i;
  size_t j, args = 0, stack = 0;
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm((unsigned)min(atou(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
      else if (startswith(arg, "--interval="))
	interval = min(atoi(arg + strlen("--interval=")), 60); /* At most 1 minute. */
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	is_reexec = 1;
      else if (strequals(arg, "{"))
	servers += stack++ == 0 ? 1 : 0;
      else if (strequals(arg, "}"))
	{
	  exit_if (stack-- == 0, eprint("Terminating non-started command, aborting."););
	  exit_if (stack == 0 && strequals(argv[i - 1], "{"),
		   eprint("Zero argument command specified, aborting."););
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
  
  
  /* Validate command line arguments. */
  
  exit_if (stack > 0, eprint("Non-terminated command specified, aborting."););
  exit_if (servers == 0, eprint("No programs to spawn, aborting."););
  
  
  /* Allocate arrays. */
  
  fail_if (xmalloc(commands_args, args + servers, char*));
  fail_if (xmalloc(commands, servers, char**));
  fail_if (xmalloc(states, servers, server_state_t));
  
  
  /* Fill command arrays. */
  
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
  xperror(*argv);
  return 1;
}


/**
 * Spawn a server
 * 
 * @param  index  The index of the server
 */
static void spawn_server(size_t index)
{
  struct timespec started;
  pid_t pid;
  
  /* When did the spawned server start? */
  
  if (monotone(&started) < 0)
    {
      xperror(*argv);
      eprintf("cannot read clock when starting %s, burying.", commands[index][0]);
      states[index].state = DEAD_AND_BURIED;
      return;
    }
  states[index].started = started;
  
  
  /* Fork process to spawn the server. */
  
  pid = fork();
  if (pid == (pid_t)-1)
    {
      xperror(*argv);
      eprintf("cannot fork in order to start %s, burying.", commands[index][0]);
      states[index].state = DEAD_AND_BURIED;
      return;
    }
  
  
  /* In the parent process (respawner): store spawned server information.  */
 
  if (pid)
    {
      states[index].pid = pid;
      states[index].state = ALIVE;
      live_count++;
      return;
    }
  
  /* In the child process (server): change execution image to the server..  */
  
  execvp(commands[index][0], commands[index]);
  xperror(commands[index][0]);
  _exit(1);
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
  (void) signo;
  reviving = 1;
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
#if UNBORN != 0
  size_t i;
#endif
  memset(states, 0, servers * sizeof(server_state_t));
#if UNBORN != 0
  for (i = 0; i < servers; i++)
    states[i].state = UNBORN;
#endif
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
  
  /* Spawn servers that has not been spawned yet. */
  
  for (i = 0; i < servers; i++)
    if (states[i].state == UNBORN)
      spawn_server(i);
  
  /* Forever mark newly spawned services (after this point in time)
     as respawned. */
  
  for (i = j = 0; j < servers; i++)
    if (commands_args[i] == NULL)
      j++;
    else if (strequals(commands_args[i], "--initial-spawn"))
      if ((commands_args[i] = strdup("--respawn")) == NULL)
	goto pfail;
  
  /* Respawn dead and dead and buried servers.*/
  
  for (i = 0; i < servers; i++)
    if ((states[i].state == DEAD) ||
	(states[i].state == DEAD_AND_BURIED))
      spawn_server(i);
  
  return 0;
 pfail:
  xperror(*argv);
  return 1;
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
  size_t rc = sizeof(int) + sizeof(sig_atomic_t);
  rc += sizeof(time_t) + sizeof(long);
  rc += servers * sizeof(server_state_t);
  return rc;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  size_t i;
  struct timespec antiepoch;
  antiepoch.tv_sec = 0;
  antiepoch.tv_nsec = 0;
  (void) monotone(&antiepoch);
  buf_set_next(state_buf, int, MDS_RESPAWN_VARS_VERSION);
  buf_set_next(state_buf, sig_atomic_t, reviving);
  buf_set_next(state_buf, time_t, antiepoch.tv_sec);
  buf_set_next(state_buf, long, antiepoch.tv_nsec);
  for (i = 0; i < servers; i++)
    {
      buf_set_next(state_buf, pid_t, states[i].pid);
      buf_set_next(state_buf, int, states[i].state);
      buf_set_next(state_buf, time_t, states[i].started.tv_sec);
      buf_set_next(state_buf, long, states[i].started.tv_nsec);
    }
  free(states);
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
  size_t i;
  struct timespec antiepoch;
  struct timespec epoch;
  epoch.tv_sec = 0;
  epoch.tv_nsec = 0;
  (void) monotone(&epoch);
  /* buf_get_next(state_buf, int, MDS_RESPAWN_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, sig_atomic_t, reviving);
  buf_get_next(state_buf, time_t, antiepoch.tv_sec);
  buf_get_next(state_buf, long, antiepoch.tv_nsec);
  epoch.tv_sec -= antiepoch.tv_sec;
  epoch.tv_nsec -= antiepoch.tv_nsec;
  for (i = 0; i < servers; i++)
    {
      buf_get_next(state_buf, pid_t, states[i].pid);
      buf_get_next(state_buf, int, states[i].state);
      buf_get_next(state_buf, time_t, states[i].started.tv_sec);
      buf_get_next(state_buf, long, states[i].started.tv_nsec);
      if (validate_state(states[i].state) == 0)
	{
	  states[i].state = CREMATED;
	  eprintf("invalid state unmarshallaed for `%s', cremating.", commands[i][0]);
	}
      else if (states[i].state == ALIVE)
	{
	  live_count++;
	  /* Monotonic time epoch adjusment, the epoch of the monotonic
	     clock is unspecified, so we cannot know whether an exec
	     with cause a time jump. */
	  states[i].started.tv_sec -= epoch.tv_sec;
	  states[i].started.tv_nsec -= epoch.tv_nsec;
	  if (states[i].started.tv_nsec < 0)
	    {
	      states[i].started.tv_sec -= 1;
	      states[i].started.tv_nsec += 1000000000;
	    }
	  else if (states[i].started.tv_nsec > 0)
	    {
	      states[i].started.tv_sec += 1;
	      states[i].started.tv_nsec -= 1000000000;
	    }
	}
    }
  return 0;
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int __attribute__((cold, const)) reexec_failure_recover(void)
{
  /* Re-exec cannot fail. */
  return 0;
}


/**
 * Respawn a server that has exited if appropriate
 * 
 * @param  pid     The process ID of the server that has exited
 * @param  status  The server's death status
 */
static void joined_with_server(pid_t pid, int status)
{
  struct timespec ended;
  size_t i;
  
  /* Find index of reaped server. */
  for (i = 0; i < servers; i++)
    if (states[i].pid == pid)
      break;
  if (i == servers)
    {
      eprintf("joined with unknown child process: %i", pid);
      return;
    }
  
  /* Do nothing if the server is cremated. */
  if (states[i].state == CREMATED)
    {
      eprintf("cremated child process `%s' exited, ignoring.", commands[i][0]);
      return;
    }
  
  /* Mark server as dead if it was alive.  */
  if (states[i].state == ALIVE)
    live_count--;
  states[i].state = DEAD;
  
  /* Cremate server if it exited normally or was killed nicely. */
  if (WIFEXITED(status) ? (WEXITSTATUS(status) == 0) :
      ((WTERMSIG(status) == SIGTERM) || (WTERMSIG(status) == SIGINT)))
    {
      eprintf("child process `%s' exited normally, cremating.", commands[i][0]);
      states[i].state = CREMATED;
      return;
    }
  
  /* Print exit status of the reaped server. */
  if (WIFEXITED(status))
    eprintf("`%s' exited with code %i.", commands[i][0], WEXITSTATUS(status));
  else
    eprintf("`%s' died by signal %i.", commands[i][0], WTERMSIG(status));
  
  /* When did the server exit. */
  if (monotone(&ended) < 0)
    {
      xperror(*argv);
      eprintf("`%s' died abnormally, burying because we could not read the time.", commands[i][0]);
      states[i].state = DEAD_AND_BURIED;
      return;
    }
  
  /* Bury the server if it died abnormally too fast. */
  if (ended.tv_sec - states[i].started.tv_sec < interval)
    {
      eprintf("`%s' died abnormally, burying because it died too fast.", commands[i][0]);
      states[i].state = DEAD_AND_BURIED;
      return;
    }
  
  /* Respawn server if it died abnormally in a responable time. */
  eprintf("`%s' died abnormally, respawning.", commands[i][0]);
  spawn_server(i);
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  int status, rc = 0;
  size_t i;
  
  while (!reexecing && !terminating && live_count)
    {
      pid_t pid = uninterruptable_waitpid(-1, &status, 0);
      
      if (reviving)
	for (reviving = 0, i = 0; i < servers; i++)
	  if (states[i].state == DEAD_AND_BURIED)
	    spawn_server(i);
      
      if (pid == (pid_t)-1)
	{
	  xperror(*argv);
	  rc = 1;
	  break;
	}
      
      joined_with_server(pid, status);
    }
  
  free(commands_args);
  free(commands);
  if (reexecing == 0)
    free(states);
  
  return rc;
}

