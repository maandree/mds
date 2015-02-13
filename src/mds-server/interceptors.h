/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_MDS_SERVER_INTERCEPTORS_H
#define MDS_MDS_SERVER_INTERCEPTORS_H


#include "interception-condition.h"
#include "client.h"
#include "queued-interception.h"

#include <stddef.h>
#include <stdint.h>


/**
 * Add an interception condition for a client
 * 
 * @param  client     The client
 * @param  condition  The header, optionally with value, to look for, or empty (not NULL) for all messages
 * @param  priority   Interception priority
 * @param  modifying  Whether the client may modify the messages
 * @param  stop       Whether the condition should be removed rather than added
 */
void add_intercept_condition(client_t* client, char* condition, int64_t priority, int modifying, int stop);


/**
 * Check if a condition matches any of a set of accepted patterns
 * 
 * @param   cond     The condition
 * @param   hashes   The hashes of the accepted header names
 * @param   keys     The header names
 * @param   headers  The header name–value pairs
 * @param   count    The number of accepted patterns
 * @return           Evaluates to true if and only if a matching pattern was found
 */
int is_condition_matching(interception_condition_t* cond, size_t* hashes,
			  char** keys, char** headers, size_t count) __attribute__((pure));


/**
 * Find a matching condition to any of a set of acceptable conditions
 * 
 * @param   client            The intercepting client
 * @param   hashes            The hashes of the accepted header names
 * @param   keys              The header names
 * @param   headers           The header name–value pairs
 * @param   count             The number of accepted patterns
 * @param   interception_out  Storage slot for found interception
 * @return                    -1 on error, otherwise: evalutes to true iff a matching condition was found
 */
int find_matching_condition(client_t* client, size_t* hashes, char** keys, char** headers,
			    size_t count, queued_interception_t* interception_out);


/**
 * Get all interceptors who have at least one condition matching any of a set of acceptable patterns
 * 
 * @param   sender                   The original sender of the message
 * @param   hashes                   The hashes of the accepted header names
 * @param   keys                     The header names
 * @param   headers                  The header name–value pairs
 * @param   count                    The number of accepted patterns
 * @param   interceptions_count_out  Slot at where to store the number of found interceptors
 * @return                           The found interceptors, `NULL` on error
 */
queued_interception_t* get_interceptors(client_t* sender, size_t* hashes, char** keys, char** headers,
					size_t count, size_t* interceptions_count_out);

#endif

