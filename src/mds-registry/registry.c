/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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
#include "registry.h"

#include "util.h"
#include "globals.h"
#include "slave.h"

#include "../mds-base.h"

#include <libmdsserver/util.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/hash-help.h>
#include <libmdsserver/client-list.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>



/**
 * Send a full message even if interrupted
 * 
 * @param   message:const char*  The message to send
 * @param   length:size_t        The length of the message
 * @return  :int                 Zero on success, -1 on error
 */
#define full_send(message, length)\
	((full_send)(socket_fd, message, length))


/**
 * Handle the received message containing a ‘Client closed’-header
 * 
 * @return  Zero on success -1 on error or interruption,
 *          `errno` will be set accordingly
 */
static int
handle_close_message(void)
{
	/* Servers do not close too often, there is no need to
	   optimise this with another hash table. Doing so would
	   also require some caution because the keys are 32-bit
	   on 32-bit computers, and the client ID:s are 64-bit. */

	size_t i, j, ptr = 0, size = 1;
	size_t *keys = NULL;
	size_t *old_keys;
	uint64_t client;
	hash_entry_t *entry;
	client_list_t *list;
	char *command;

	/* Remove server for all protocols. */
	for (i = 0; i < received.header_count; i++) {
		if (startswith(received.headers[i], "Client closed: ")) {
			client = parse_client_id(received.headers[i] + strlen("Client closed: "));

			foreach_hash_table_entry (reg_table, j, entry) {
				/* Remove server from list of servers that support the protocol,
				   once, if it is in the list. */
				list = (void *)(entry->value);
				client_list_remove(list, client);
				if (list->size)
					continue;

				/* If no servers support the protocol, list the protocol for removal. */
				fail_if (!keys && xmalloc(keys, size, size_t));
				fail_if (ptr == size ? growalloc(old_keys, keys, size, size_t) : 0);
				keys[ptr++] = entry->key;
			}

	
			/* Mark client as closed. */
			close_slaves(client);
		}
	}

	/* Close slaves those clients have closed. */
	with_mutex (slave_mutex, pthread_cond_broadcast(&slave_cond););

	/* Remove protocol that no longer have any supporting servers. */
	for (i = 0; i < ptr; i++) {
		entry = hash_table_get_entry(&reg_table, keys[i]);
		list = (void *)(entry->value);
		command = (void *)(entry->key);

		hash_table_remove(&reg_table, entry->key);

		client_list_destroy(list);
		free(list);
		free(command);
	}

	free(keys);
	return 0;
fail:
	xperror(*argv);
	free(keys);
	return -1;
}


/**
 * Add a protocol to the registry
 * 
 * @param   has_key      Whether the command is already in the registry
 * @param   command      The command
 * @param   command_key  The address of `command`
 * @param   client       The ID of the client that implements the server-side of the protocol
 * @return               Non-zero on error
 */
static int __attribute__((nonnull))
registry_action_add(int has_key, char *command, size_t command_key, uint64_t client)
{
	int saved_errno;
	client_list_t *list;
	size_t address;
	void *paddress;

	if (has_key) {
		/* Add server to protocol if the protocol is already in the table. */
		address = hash_table_get(&reg_table, command_key);
		list = (void *)address;
		fail_if (client_list_add(list, client) < 0);
	} else {
		/* If the protocol is not already in the table. */

		/* Allocate list of servers for the protocol. */
		fail_if (xmalloc(paddress = list, 1, client_list_t));
		/* Duplicate the protocol name so it can be accessed later. */
		if (xstrdup_nn(command, command)) {
			saved_errno = errno, free(list), errno = saved_errno;
			fail_if (1);
		}
		/* Create list of servers, add server to list and add the protocol to the table. */
		command_key = (size_t)(void*)command;
		if (client_list_create(list, 1) ||
		    client_list_add(list, client) ||
		    !hash_table_put(&reg_table, command_key, (size_t)paddress)) {
			saved_errno = errno;
			client_list_destroy(list);
			free(list);
			free(command);
			errno = saved_errno;
			fail_if (1);
		}
	}

	/* Notify slaves. */
	fail_if (advance_slaves(command));

	return 0;
fail:
	xperror(*argv);
	return -1;
}


/**
 * Remove a protocol from the registry
 * 
 * @param   command_key  The address of a string that contains the command
 * @param   client       The ID of the client that implements the server-side of the protocol
 * @return               Non-zero on error
 */
static void
registry_action_remove(size_t command_key, uint64_t client)
{
	hash_entry_t *entry = hash_table_get_entry(&reg_table, command_key);
	size_t address = entry->value;
	client_list_t *list = (void *)address;

	/* Remove server from protocol. */
	client_list_remove(list, client);

	/* Remove protocol if no servers support it anymore. */
	if (!list->size) {
		client_list_destroy(list);
		free(list);
		hash_table_remove(&reg_table, command_key);
		reg_table_free_key(entry->key);
	}
}


/**
 * Modify the protocol registry or list missing protocols
 * 
 * @param   command      The command
 * @param   action       -1 to remove command, +1 to add commands, 0 to
 *                       wait until the message commnds are registered
 * @param   client       The ID of the client that implements the server-side of the protocol
 * @param   wait_set     Table to fill with missing protocols if `action == 0`
 * @return               Non-zero on error
 */
static int __attribute__((nonnull))
registry_action_act(char *command, int action, uint64_t client, hash_table_t *wait_set)
{
	size_t command_key = (size_t)(void *)command;
	int has_key = hash_table_contains_key(&reg_table, command_key);
	int saved_errno;

	switch (action) {
	case 1:
		/* Register server to protocol. */
		fail_if (registry_action_add(has_key, command, command_key, client));
		break;
	case -1:
		if (has_key)
			/* Unregister server from protocol. */
			registry_action_remove(command_key, client);
		break;
	case 0:
		if (has_key)
			break;
		/* Add protocol to wait set of not present in the protocol table. */
		fail_if (xstrdup_nn(command, command));
		command_key = (size_t)(void*)command;
		if (!hash_table_put(wait_set, command_key, 1) && errno) {
			saved_errno = errno, free(command), errno = saved_errno;
			fail_if (1);
		}
		break;
	default:
		break;
	}

	return 0;
fail:
	xperror(*argv);
	if (action != 1) {
		hash_table_destroy(wait_set, (free_func *)reg_table_free_key, NULL);
		free(wait_set);
	}
	return -1;
}


/**
 * Perform an action over the registry
 * 
 * @param   length           The length of the received message
 * @param   action           -1 to remove command, +1 to add commands, 0 to
 *                           wait until the message commnds are registered
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The ID of the received message
 * @return                   Zero on success -1 on error or interruption,
 *                           `errno` will be set accordingly
 */
static int __attribute__((nonnull))
registry_action(size_t length, int action, const char *recv_client_id, const char *recv_message_id)
{
	char *payload = received.payload;
	uint64_t client = action ? parse_client_id(recv_client_id) : 0;
	hash_table_t *wait_set = NULL;
	size_t begin, len;
	int saved_errno;
	char *end, *command;

	/* If ‘Action: wait’, create a set for the protocols that are not already available. */
	if (!action) {
		fail_if (xmalloc(wait_set, 1, hash_table_t));
		fail_if (hash_table_create(wait_set));
		wait_set->key_comparator = (compare_func*)string_comparator;
		wait_set->hasher = (hash_func*)string_hash;
	}

	/* If the payload buffer is full, increase it so we can fit another character. */
	if (received.payload_size == length) {
		fail_if (growalloc(old, received.payload, received.payload_size, char));
		payload = received.payload;
	}

	/* LF-terminate the payload, perhaps it did not have a terminal LF. */
	payload[length] = '\n';

	/* For all protocols in the payload, either add or remove
	   them from or to the protocl table or the wait set. */
	for (begin = 0; begin < length;) {
		end = rawmemchr(payload + begin, '\n');
		len = (size_t)(end - payload) - begin - 1;
		command = payload + begin;

		command[len] = '\0';
		begin += len + 1;

		if (len > 0 && registry_action_act(command, action, client, wait_set))
			fail_if (wait_set = NULL, 1);
	}

	/* If ‘Action: wait’, start a new thread that waits for the protocols and the responds. */
	if (!action && start_slave(wait_set, recv_client_id, recv_message_id))
		fail_if (wait_set = NULL, 1);

	return 0;
fail:
	saved_errno = errno;
	if (wait_set)
		hash_table_destroy(wait_set, NULL, NULL), free(wait_set);
	return errno = saved_errno, -1;
}


/**
 * Send a list of all registered commands to a client
 * 
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The ID of the received message
 * @return                   Zero on success, -1 on error or interruption,
 *                           `errno` will be set accordingly
 */
static int __attribute__((nonnull))
list_registry(const char *recv_client_id, const char *recv_message_id)
{
	size_t ptr = 0, i, key, len;
	hash_entry_t *entry;
	char *command;

	/* Allocate the send buffer for the first time, it cannot be doubled if it is zero. */
	if (!send_buffer_size) {
		fail_if (xmalloc(send_buffer, 256, char));
		send_buffer_size = 256;
	}

	/* Add all protocols to the send buffer. */

	foreach_hash_table_entry (reg_table, i, entry) {
		key = entry->key;
		command = (char*)(void*)key;
		len = strlen(command);

		/* Make sure the send buffer can fit all protocols. */
		while (ptr + len + 1 >= send_buffer_size)
			fail_if (growalloc(old, send_buffer, send_buffer_size, char));

		memcpy(send_buffer + ptr, command, len * sizeof(char));
		ptr += len;
		send_buffer[ptr++] = '\n';
	}

	/* Make sure the message headers can fit the send buffer. */
	i = sizeof("To: \n"
	           "In response to: \n"
	           "Message ID: \n"
	           "Origin command: register\n"
	           "Length: \n"
	           "\n") / sizeof(char) - 1;
	i += strlen(recv_message_id) + strlen(recv_client_id) + 10 + 19;

	while (ptr + i >= send_buffer_size)
		fail_if (growalloc(old, send_buffer, send_buffer_size, char));

	/* Construct message headers. */
	sprintf(send_buffer + ptr,
	        "To: %s\n"
	        "In response to: %s\n"
	        "Message ID: %" PRIu32 "\n"
	        "Origin command: register\n"
	        "Length: %" PRIu64 "\n"
	        "\n",
	        recv_client_id, recv_message_id, message_id, ptr);

	/* Increase message ID. */
	with_mutex (slave_mutex, message_id = message_id == UINT32_MAX ? 0 : (message_id + 1););

	/* Send message. */
	fail_if (full_send(send_buffer + ptr, strlen(send_buffer + ptr)));
	fail_if (full_send(send_buffer, ptr));
	return 0;
fail:
	return -1;
}


/**
 * Handle the received message containing ‘Command: register’-header–value
 * 
 * @return  Zero on success -1 on error or interruption,
 *          `errno` will be set accordingly
 */
static int
handle_register_message(void)
{
	/* Fetch message headers. */
	const char *recv_client_id = NULL;
	const char *recv_message_id = NULL;
	const char *recv_length = NULL;
	const char *recv_action = NULL;
	size_t i, length = 0;

#define __get_header(storage, header)\
	(startswith(received.headers[i], header))\
		storage = received.headers[i] + strlen(header)

	for (i = 0; i < received.header_count; i++) {
		if      __get_header(recv_client_id,  "Client ID: ");
		else if __get_header(recv_message_id, "Message ID: ");
		else if __get_header(recv_length,     "Length: ");
		else if __get_header(recv_action,     "Action: ");
		else
			continue;

		/* Stop if we got all headers we recognised, except ‘Time to live’. */
		if (recv_client_id && recv_message_id && recv_length && recv_action)
			break;
	}

#undef __get_header

	/* Validate headers. */
	if (!recv_client_id || strequals(recv_client_id, "0:0"))
		return eprint("received message from anonymous sender, ignoring."), 0;
	else if (!strchr(recv_client_id, ':'))
		return eprint("received message from sender without a colon it its ID, ignoring, invalid ID."), 0;
	else if (!recv_length && (!recv_action || !strequals(recv_action, "list")))
		return eprint("received empty message without `Action: list`, ignoring, has no effect."), 0;
	else if (!recv_message_id)
		return eprint("received message without ID, ignoring, master server is misbehaving."), 0;

	/* Get message length, and make sure the action is defined. */
	if (recv_length)
		length = atoz(recv_length);
	if (recv_action)
		recv_action = "add";

	/* Perform action. */
#define __registry_action(action) registry_action(length, action, recv_client_id, recv_message_id)

	if      (strequals(recv_action, "add"))    return __registry_action(1);
	else if (strequals(recv_action, "remove")) return __registry_action(-1);
	else if (strequals(recv_action, "wait"))   return __registry_action(0);
	else if (strequals(recv_action, "list"))   return list_registry(recv_client_id, recv_message_id);
	else {
		eprint("received invalid action, ignoring.");
		return 0;
	}

#undef __registry_action
}


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error or interruption,
 *          `errno` will be set accordingly
 */
int handle_message(void)
{
	size_t i;
	for (i = 0; i < received.header_count; i++) {
		if (strequals(received.headers[i], "Command: register")) {
			fail_if (handle_register_message());
			return 0;
		}
	}
	fail_if (handle_close_message());
	return 0;
 fail:
	return -1;
}
