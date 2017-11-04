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
#include "slavery.h"

#include <libmdsserver/linked-list.h>
#include <libmdsserver/hash-table.h>
#include <libmdsserver/fd-table.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>



/**
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
size_t
marshal_server_size(void)
{
	size_t list_size = linked_list_marshal_size(&client_list);
	size_t map_size = fd_table_marshal_size(&client_map);
	size_t list_elements = 0;
	size_t state_n = 0;
	ssize_t node;

	/* Calculate the grand size of all client information. */
	foreach_linked_list_node (client_list, node) {
		state_n += client_marshal_size((void *)(client_list.values[node]));
		list_elements++;
	}

	/* Add the size of the rest of the program's state. */
	state_n += sizeof(int) + sizeof(sig_atomic_t) + 2 * sizeof(uint64_t) + 2 * sizeof(size_t);
	state_n += list_elements * sizeof(size_t) + list_size + map_size;

	return state_n;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int
marshal_server(char *state_buf)
{
	size_t list_size = linked_list_marshal_size(&client_list);
	size_t list_elements = 0;
	ssize_t node;
	size_t value_address;
	client_t *value, *client;


	/* Release resources. */
	pthread_mutex_destroy(&slave_mutex);
	pthread_cond_destroy(&slave_cond);
	pthread_mutex_destroy(&modify_mutex);
	pthread_cond_destroy(&modify_cond);
	hash_table_destroy(&modify_map, NULL, NULL);


	/* Count the number of clients that online. */
	foreach_linked_list_node (client_list, node)
		list_elements++;

	/* Tell the new version of the program what version of the program it is marshalling. */
	buf_set_next(state_buf, int, MDS_SERVER_VARS_VERSION);

	/* Marshal the miscellaneous state data. */
	buf_set_next(state_buf, sig_atomic_t, running);
	buf_set_next(state_buf, uint64_t, next_client_id);
	buf_set_next(state_buf, uint64_t, next_modify_id);

	/* Tell the program how large the marshalled client list is and how any clients are marshalled. */
	buf_set_next(state_buf, size_t, list_size);
	buf_set_next(state_buf, size_t, list_elements);

	/* Marshal the clients. */
	foreach_linked_list_node (client_list, node) {
		/* Get the memory address of the client. */
		value_address = client_list.values[node];
		/* Get the client's information. */
		value = (void *)value_address;

		/* Marshal the address, it is used the the client list and the client map, that will be marshalled. */
		buf_set_next(state_buf, size_t, value_address);
		/* Marshal the client informationation. */
		state_buf += client_marshal(value, state_buf) / sizeof(char);
	}

	/* Marshal the client list. */
	linked_list_marshal(&client_list, state_buf);
	state_buf += list_size / sizeof(char);
	/* Marshal the client map. */
	fd_table_marshal(&client_map, state_buf);


	/* Release resources. */
	foreach_linked_list_node (client_list, node) {
		client = (void *)(client_list.values[node]);
		client_destroy(client);
	}
	fd_table_destroy(&client_map, NULL, NULL);
	linked_list_destroy(&client_list);

	return 0;
}


/**
 * Address translation table used by `unmarshal_server` and `remapper`
 */
static hash_table_t unmarshal_remap_map;

/**
 * Address translator for `unmarshal_server`
 * 
 * @param   old  The old address
 * @return       The new address
 */
static size_t
unmarshal_remapper(size_t old)
{
	return hash_table_get(&unmarshal_remap_map, old);
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
int
unmarshal_server(char *state_buf)
{
	int with_error = 0;
	size_t list_size;
	size_t list_elements;
	size_t i;
	ssize_t node;
	pthread_t slave_thread;
	size_t n, value_address, new_address;
	client_t *value, *client;
	int slave_fd;

#define fail soft_fail

	/* Create memory address remapping table. */
	fail_if (hash_table_create(&unmarshal_remap_map));

#undef fail
#define fail clients_fail
  
	/* Get the marshal protocal version. Not needed, there is only the one version right now. */
	/* buf_get(state_buf, int, 0, MDS_SERVER_VARS_VERSION); */
	buf_next(state_buf, int, 1);

	/* Unmarshal the miscellaneous state data. */
	buf_get_next(state_buf, sig_atomic_t, running);
	buf_get_next(state_buf, uint64_t, next_client_id);
	buf_get_next(state_buf, uint64_t, next_modify_id);

	/* Get the marshalled size of the client list and how any clients that are marshalled. */
	buf_get_next(state_buf, size_t, list_size);
	buf_get_next(state_buf, size_t, list_elements);

	/* Unmarshal the clients. */
	for (i = 0; i < list_elements; i++) {
		/* Allocate the client's information. */
		fail_if (xmalloc(value, 1, client_t));

		/* Unmarshal the address, it is used the the client list and the client map, that are also marshalled. */
		buf_get_next(state_buf, size_t, value_address);
		/* Unmarshal the client information. */
		fail_if (n = client_unmarshal(value, state_buf), n == 0);

		/* Populate the remapping table. */
		if (!hash_table_put(&unmarshal_remap_map, value_address, (size_t)(void *)value))
			fail_if (errno);

		/* Delayed seeking. */
		state_buf += n / sizeof(char);

		/* On error, seek past all clients. */
		continue;
	clients_fail:
		xperror(*argv);
		with_error = 1;
		if (value) {
			buf_prev(state_buf, size_t, 1);
			free(value);
		}
		for (; i < list_elements; i++)
			/* There is not need to close the sockets, it is done by
			   the caller because there are conditions where we cannot
			   get here anyway. */
			state_buf += client_unmarshal_skip(state_buf) / sizeof(char);
		break;
	}

#undef fail
#define fail  critical_fail

	/* Unmarshal the client list. */
	fail_if (linked_list_unmarshal(&client_list, state_buf));
	state_buf += list_size / sizeof(char);

	/* Unmarshal the client map. */
	fail_if (fd_table_unmarshal(&client_map, state_buf, unmarshal_remapper));

	/* Remove non-found elements from the fd table. */
#define __bit(I, _OP_) (client_map.used[I / 64] _OP_ ((uint64_t)1 << (I % 64)))
	if (with_error)
		for (i = 0; i < client_map.capacity; i++)
			if (__bit(i, &) && !client_map.values[i])
				/* Lets not presume that fd-table actually initialise its allocations. */
				__bit(i, &= ~);
#undef __bit

	/* Remap the linked list and remove non-found elements, and start the clients. */
	foreach_linked_list_node (client_list, node) {
		/* Remap the linked list and remove non-found elements. */
		new_address = unmarshal_remapper(client_list.values[node]);
		client_list.values[node] = new_address;
		if (new_address == 0) { /* Returned if missing (or if the address is the invalid NULL.) */
			linked_list_remove(&client_list, node);
		} else {
			/* Start the clients. (Errors do not need to be reported.) */
			client = (client_t*)(void*)new_address;
			slave_fd = client->socket_fd;

			/* Increase number of running slaves. */
			with_mutex (slave_mutex, running_slaves++;);

			/* Start slave thread. */
			create_slave(&slave_thread, slave_fd);
		}
	}

	/* Release the remapping table's resources. */
	hash_table_destroy(&unmarshal_remap_map, NULL, NULL);

	return with_error;
#undef fail

soft_fail:
	xperror(*argv);
	hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
	return -1;
critical_fail:
	xperror(*argv);
	abort();
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int
reexec_failure_recover(void)
{
	/* Close all files (hopefully sockets) we do not know what they are. */
	close_files(fd > 2 && fd != socket_fd && !fd_table_contains_key(&client_map, fd));
	return 0;
}
