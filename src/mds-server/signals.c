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
#include "signals.h"

#include "globals.h"
#include "client.h"

#include <libmdsserver/linked-list.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>


/**
 * Send a singal to all threads except the current thread
 * 
 * @param  signo  The signal
 */
static void signal_all(int signo)
{      
  pthread_t current_thread;
  ssize_t node;
  
  current_thread = pthread_self();
  
  if (pthread_equal(current_thread, master_thread) == 0)
    pthread_kill(master_thread, signo);
  
  with_mutex (slave_mutex,
	      foreach_linked_list_node (client_list, node)
	        {
		  client_t* value = (client_t*)(void*)(client_list.values[node]);
		  if (pthread_equal(current_thread, value->thread) == 0)
		    pthread_kill(value->thread, signo);
		}
	      );
}


/**
 * Called with the signal SIGUSR1 is caught.
 * This function should cue a re-exec of the program.
 * 
 * @param  signo  The caught signal
 */
static void sigusr1_trap(int signo)
{
  if (reexecing == 0)
    {
      terminating = reexecing = 1;
      eprint("re-exec signal received.");
      signal_all(signo);
    }
}


/**
 * Called with the signal SIGTERM is caught.
 * This function should cue a termination of the program.
 * 
 * @param  signo  The caught signal
 */
static void sigterm_trap(int signo)
{
  if (terminating == 0)
    {
      terminating = 1;
      eprint("terminate signal received.");
      signal_all(signo);
    }
}


/**
 * Set up signal traps for all especially handled signals
 * 
 * @return  Zero on success, -1 on error
 */
int trap_signals(void)
{
  /* Make the server update without all slaves dying on SIGUSR1. */
  if (xsigaction(SIGUSR1, sigusr1_trap) < 0)  return -1;
  
  /* Implement clean exit on SIGTERM. */
  if (xsigaction(SIGTERM, sigterm_trap) < 0)  return -1;
  
  return 0;
}

