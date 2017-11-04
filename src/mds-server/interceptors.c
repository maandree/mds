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
#include "interceptors.h"

#include "globals.h"
#include "interception-condition.h"
#include "client.h"
#include "queued-interception.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/hash-help.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


/**
 * Remove interception condition by index
 * 
 * @param  client  The intercepting client
 * @param  index   The index of the condition
 */
static void __attribute__((nonnull))
remove_intercept_condition(client_t *client, size_t index)
{
	interception_condition_t *conds = client->interception_conditions;
	size_t n = client->interception_conditions_count;

	/* Remove the condition from the list. */
	free(conds[index].condition);
	memmove(conds + index, conds + index + 1, --n - index);
	client->interception_conditions_count--;

	/* Shrink the list. */
	if (!client->interception_conditions_count) {
		free(conds);
		client->interception_conditions = NULL;
	} else if (xrealloc(conds, n, interception_condition_t)) {
		xperror(*argv);
	} else {
		client->interception_conditions = conds;
	}
}


/**
 * Add an interception condition for a client
 * 
 * @param  client     The client
 * @param  condition  The header, optionally with value, to look for, or empty (not NULL) for all messages
 * @param  priority   Interception priority
 * @param  modifying  Whether the client may modify the messages
 * @param  stop       Whether the condition should be removed rather than added
 */
void
add_intercept_condition(client_t *client, char *condition, int64_t priority, int modifying, int stop)
{
	size_t n = client->interception_conditions_count;
	interception_condition_t *conds = client->interception_conditions;
	ssize_t nonmodifying = -1;
	char *header = condition;
	char *colon = NULL;
	char *value;
	size_t hash, i;
	interception_condition_t temp;

	/* Split header and value apart. */
	if ((value = strchr(header, ':'))) {
		*value = '\0'; /* NUL-terminate header. */
		colon = value; /* End of header. */
		value += 2;    /* Skip over delimiter.  */
	}

	/* Calcuate header hash (comparison optimisation) */
	hash = string_hash(header);
	/* Undo header–value splitting. */
	if (colon)
		*colon = ':';

	/* Remove of update condition of already registered,
	   also look for non-modifying condition to swap position
	   with for optimisation. */
	for (i = 0; i < n; i++) {
		if ((conds[i].header_hash != hash) || !strequals(conds[i].condition, condition)) {
			/* Look for the first non-modifying, this is a part of the
			   optimisation where we put all modifying conditions at the
			   beginning. */
			if (nonmodifying < 0 && conds[i].modifying)
				nonmodifying = (ssize_t)i;
			continue;
		}

		if (stop) {
			remove_intercept_condition(client, i);
		} else {
			/* Update parameters. */
			conds[i].priority = priority;
			conds[i].modifying = modifying;

			if (modifying && nonmodifying >= 0) {
				/* Optimisation: put conditions that are modifying
				   at the beginning. When a client is intercepting
				   we most know if any satisfying condition is
				   modifying. With this optimisation the first
				   satisfying condition will tell us if there is
				   any satisfying condition that is modifying. */
				temp = conds[nonmodifying];
				conds[nonmodifying] = conds[i];
				conds[i] = temp;
			}
		}
		return;
	}

	if (stop) {
		eprint("client tried to stop intercepting messages that it does not intercept.");
	} else {
		/* Duplicate condition string. */
		fail_if (xstrdup_nn(condition, condition));

		/* Grow the interception condition list. */
		fail_if (xrealloc(conds, n + 1, interception_condition_t));
		client->interception_conditions = conds; 
		/* Store condition. */
		client->interception_conditions_count++;
		conds[n].condition = condition;
		conds[n].header_hash = hash;
		conds[n].priority = priority;
		conds[n].modifying = modifying;

		if (modifying && nonmodifying >= 0) {
			/* Optimisation: put conditions that are modifying
			   at the beginning. When a client is intercepting
			   we most know if any satisfying condition is
			   modifying. With this optimisation the first
			   satisfying condition will tell us if there is
			   any satisfying condition that is modifying. */
			temp = conds[nonmodifying];
			conds[nonmodifying] = conds[n];
			conds[n] = temp;
		}
	}
  
	return;
fail:
	xperror(*argv);
	free(condition);
	return;
}


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
int
is_condition_matching(interception_condition_t *cond, size_t *hashes,
                      char **keys, char **headers, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++) {
		if (*cond->condition == '\0')
			return 1;
		else if ((cond->header_hash == hashes[i]) &&
		         (strequals(cond->condition, keys[i]) ||
		          strequals(cond->condition, headers[i])))
			return 1;
	}
	return 0;
}


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
int
find_matching_condition(client_t *client, size_t *hashes, char **keys, char **headers,
                        size_t count, queued_interception_t *interception_out)
{
	pthread_mutex_t mutex = client->mutex;
	interception_condition_t *conds = client->interception_conditions;
	size_t n = 0, i;

	fail_if ((errno = pthread_mutex_lock(&(mutex))));

	/* Look for a matching condition. */
	if (client->open)
		n = client->interception_conditions_count;
	for (i = 0; i < n; i++) {
		if (is_condition_matching(conds + i, hashes, keys, headers, count)) {
			/* Report matching condition. */
			interception_out->client    = client;
			interception_out->priority  = conds[i].priority;
			interception_out->modifying = conds[i].modifying;
			break;
		}
	}

	pthread_mutex_unlock(&mutex);

	return i < n;
fail:
	return -1;
}


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
queued_interception_t *
get_interceptors(client_t *sender, size_t *hashes, char **keys, char **headers,
                 size_t count, size_t *interceptions_count_out)
{
	queued_interception_t *interceptions = NULL;
	size_t interceptions_count = 0, n = 0;
	ssize_t node;
	int saved_errno, r;
	client_t *client;

	/* Count clients. */
	foreach_linked_list_node (client_list, node)
		n++;

	/* Allocate interceptor list. */
	fail_if (xmalloc(interceptions, n, queued_interception_t));

	/* Search clients. */
	foreach_linked_list_node (client_list, node) {
		client = (void *)(client_list.values[node]);

		/* Look for and list a matching condition. */
		if (client->open && (client != sender)) {
			r = find_matching_condition(client, hashes, keys, headers, count,
			                            interceptions + interceptions_count);
			fail_if (r == -1);
			if (r)
				/* List client of there was a matching condition. */
				interceptions_count++;
		}
	}

	*interceptions_count_out = interceptions_count;
	return interceptions;

fail:
	saved_errno = errno;
	free(interceptions);
	return errno = saved_errno, NULL;
}
