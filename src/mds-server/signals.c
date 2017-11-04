/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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

#include <pthread.h>


/**
 * Send a singal to all threads except the current thread
 * 
 * @param  signo  The signal
 */
void signal_all(int signo)
{      
	pthread_t current_thread;
	ssize_t node;
	client_t *value;

	current_thread = pthread_self();

	if (pthread_equal(current_thread, master_thread) == 0)
		pthread_kill(master_thread, signo);
	
	with_mutex (slave_mutex,
	            foreach_linked_list_node (client_list, node) {
	                    value = (client_t*)(void*)(client_list.values[node]);
	                    if (!pthread_equal(current_thread, value->thread))
	                            pthread_kill(value->thread, signo);
	            }
	           );
}
