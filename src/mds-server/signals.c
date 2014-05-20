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

#include "globals.h"
#include "client.h"

#include <libmdsserver/linked-list.h>
#include <libmdsserver/macros.h>

#include <stdio.h>
#include <pthread.h>
#include <errno.h>


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
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `reexecing` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void received_reexec(int signo)
{
  if (reexecing == 0)
    {
      terminating = reexecing = 1;
      eprint("re-exec signal received.");
      signal_all(signo);
    }
}


/**
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void received_terminate(int signo)
{
  if (terminating == 0)
    {
      terminating = 1;
      eprint("terminate signal received.");
      signal_all(signo);
    }
}

