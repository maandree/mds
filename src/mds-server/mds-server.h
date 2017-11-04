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
#ifndef MDS_MDS_SERVER_H
#define MDS_MDS_SERVER_H


#include "client.h"

#include <stddef.h>


/**
 * Accept an incoming and start a slave thread for it
 * 
 * @return  Zero normally, 1 if terminating
 */
int accept_connection(void);

/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Output data
 */
void *slave_loop(void *data);

/**
 * Queue a message for multicasting
 * 
 * @param  message  The message
 * @param  length   The length of the message
 * @param  sender   The original sender of the message
 */
__attribute__((nonnull))
void queue_message_multicast(char *message, size_t length, client_t *sender);

/**
 * Exec into the mdsinitrc script
 * 
 * @param  args  The arguments to the child process
 */
__attribute__((noreturn, nonnull))
void run_initrc(char **args);


#endif
