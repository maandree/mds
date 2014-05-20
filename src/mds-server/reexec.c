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
#include "reexec.h"

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
 * Marshal the server's state into a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int marshal_server(int fd)
{
  size_t list_size = linked_list_marshal_size(&client_list);
  size_t map_size = fd_table_marshal_size(&client_map);
  size_t list_elements = 0;
  size_t state_n = 0;
  char* state_buf = NULL;
  char* state_buf_;
  ssize_t node;
  
  
  /* Calculate the grand size of all client information. */
  for (node = client_list.edge;; list_elements++)
    {
      if ((node = client_list.next[node]) == client_list.edge)
	break;
      state_n += client_marshal_size((client_t*)(void*)(client_list.values[node]));
    }
  
  /* Add the size of the rest of the program's state. */
  state_n += sizeof(int) + sizeof(sig_atomic_t) + 2 * sizeof(uint64_t) + 2 * sizeof(size_t);
  state_n += list_elements * sizeof(size_t) + list_size + map_size;
  
  /* Allocate a buffer for all data. */
  state_buf = state_buf_ = malloc(state_n);
  fail_if (state_buf == NULL);
  
  
  /* Tell the new version of the program what version of the program it is marshalling. */
  buf_set_next(state_buf_, int, MDS_SERVER_VARS_VERSION);
  
  /* Marshal the miscellaneous state data. */
  buf_set_next(state_buf_, sig_atomic_t, running);
  buf_set_next(state_buf_, uint64_t, next_client_id);
  buf_set_next(state_buf_, uint64_t, next_modify_id);
  
  /* Tell the program how large the marshalled client list is and how any clients are marshalled. */
  buf_set_next(state_buf_, size_t, list_size);
  buf_set_next(state_buf_, size_t, list_elements);
  
  /* Marshal the clients. */
  foreach_linked_list_node (client_list, node)
    {
      /* Get the memory address of the client. */
      size_t value_address = client_list.values[node];
      /* Get the client's information. */
      client_t* value = (client_t*)(void*)value_address;
      
      /* Marshal the address, it is used the the client list and the client map, that will be marshalled. */
      buf_set_next(state_buf_, size_t, value_address);
      /* Marshal the client informationation. */
      state_buf_ += client_marshal(value, state_buf_) / sizeof(char);
    }
  
  /* Marshal the client list. */
  linked_list_marshal(&client_list, state_buf_);
  state_buf_ += list_size / sizeof(char);
  /* Marshal the client map. */
  fd_table_marshal(&client_map, state_buf_);
  
  /* Send the marshalled data into the file. */
  fail_if (full_write(fd, state_buf, state_n) < 0);
  free(state_buf);
  
  return 0;
  
 pfail:
  perror(*argv);
  free(state_buf);
  return -1;
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
static size_t unmarshal_remapper(size_t old)
{
  return hash_table_get(&unmarshal_remap_map, old);
}

/**
 * Unmarshal the server's state from a file
 * 
 * @param   fd  The file descriptor
 * @return      Negative on error
 */
int unmarshal_server(int fd)
{
  int with_error = 0;
  char* state_buf;
  char* state_buf_;
  size_t list_size;
  size_t list_elements;
  size_t i;
  ssize_t node;
  pthread_t slave_thread;
  
  
  /* Read the file. */
  if ((state_buf = state_buf_ = full_read(fd)) == NULL)
    {
      perror(*argv);
      return -1;
    }
  
  /* Create memory address remapping table. */
  if (hash_table_create(&unmarshal_remap_map))
    {
      perror(*argv);
      free(state_buf);
      hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
      return -1;
    }
  
  
  /* Get the marshal protocal version. Not needed, there is only the one version right now. */
  /* buf_get(state_buf_, int, 0, MDS_SERVER_VARS_VERSION); */
  buf_next(state_buf_, int, 1);
  
  /* Unmarshal the miscellaneous state data. */
  buf_get_next(state_buf_, sig_atomic_t, running);
  buf_get_next(state_buf_, uint64_t, next_client_id);
  buf_get_next(state_buf_, uint64_t, next_modify_id);
  
  /* Get the marshalled size of the client list and how any clients that are marshalled. */
  buf_get_next(state_buf_, size_t, list_size);
  buf_get_next(state_buf_, size_t, list_elements);
  
  /* Unmarshal the clients. */
  for (i = 0; i < list_elements; i++)
    {
      size_t n;
      size_t value_address;
      client_t* value;
      
      /* Allocate the client's information. */
      if (xmalloc(value, 1, client_t))
	goto clients_fail;
      
      /* Unmarshal the address, it is used the the client list and the client map, that are also marshalled. */
      buf_get_next(state_buf_, size_t, value_address);
      /* Unmarshal the client information. */
      n = client_unmarshal(value, state_buf_);
      if (n == 0)
	goto clients_fail;
      
      /* Populate the remapping table. */
      if (hash_table_put(&unmarshal_remap_map, value_address, (size_t)(void*)value) == 0)
	if (errno)
	  goto clients_fail;
      
      /* Delayed seeking. */
      state_buf_ += n / sizeof(char);
      
      
      /* On error, seek past all clients. */
      continue;
    clients_fail:
      perror(*argv);
      with_error = 1;
      if (value != NULL)
	{
	  buf_prev(state_buf_, size_t, 1);
	  free(value);
	}
      for (; i < list_elements; i++)
	/* There is not need to close the sockets, it is done by
	   the caller because there are conditions where we cannot
	   get here anyway. */
	state_buf_ += client_unmarshal_skip(state_buf_) / sizeof(char);
      break;
    }
  
  /* Unmarshal the client list. */
  if (linked_list_unmarshal(&client_list, state_buf_))
    goto critical_fail;
  state_buf_ += list_size / sizeof(char);
  
  /* Unmarshal the client map. */
  if (fd_table_unmarshal(&client_map, state_buf_, unmarshal_remapper))
    goto critical_fail;
  
  /* Release the raw data. */
  free(state_buf);
  
  /* Remove non-found elements from the fd table. */
#define __bit(I, _OP_)  client_map.used[I / 64] _OP_ ((uint64_t)1 << (I % 64))
  if (with_error)
    for (i = 0; i < client_map.capacity; i++)
      if ((__bit(i, &)) && (client_map.values[i] == 0))
	/* Lets not presume that fd-table actually initialise its allocations. */
	__bit(i, &= ~);
#undef __bit
  
  /* Remap the linked list and remove non-found elements, and start the clients. */
  foreach_linked_list_node (client_list, node)
    {
      /* Remap the linked list and remove non-found elements. */
      size_t new_address = unmarshal_remapper(client_list.values[node]);
      client_list.values[node] = new_address;
      if (new_address == 0) /* Returned if missing (or if the address is the invalid NULL.) */
	linked_list_remove(&client_list, node);
      else
	{
	  /* Start the clients. (Errors do not need to be reported.) */
	  client_t* client = (client_t*)(void*)new_address;
	  int socket_fd = client->socket_fd;
	  
	  /* Increase number of running slaves. */
	  with_mutex (slave_mutex, running_slaves++;);
	  
	  /* Start slave thread. */
	  create_slave(&slave_thread, socket_fd);
	}
    }
  
  /* Release the remapping table's resources. */
  hash_table_destroy(&unmarshal_remap_map, NULL, NULL);
  
  return -with_error;

 critical_fail:
  perror(*argv);
  abort();
}


/**
 * Marshal and re-execute the server
 * 
 * @param  reexec  Whether the server was previously re-executed
 */
void perform_reexec(int reexec)
{
  pid_t pid = getpid();
  int reexec_fd;
  char shm_path[NAME_MAX + 1];
  ssize_t node;
  
  /* Join with all slaves threads. */
  with_mutex (slave_mutex,
	      while (running_slaves > 0)
		pthread_cond_wait(&slave_cond, &slave_mutex););
  
  /* Release resources. */
  pthread_mutex_destroy(&slave_mutex);
  pthread_cond_destroy(&slave_cond);
  pthread_mutex_destroy(&modify_mutex);
  pthread_cond_destroy(&modify_cond);
  hash_table_destroy(&modify_map, NULL, NULL);
  
  /* Marshal the state of the server. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
  reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
  if (reexec_fd < 0)
    {
      perror(*argv);
      return;
    }
  fail_if (marshal_server(reexec_fd) < 0);
  close(reexec_fd);
  reexec_fd = -1;
  
  /* Release resources. */
  foreach_linked_list_node (client_list, node)
    {
      client_t* client = (client_t*)(void*)(client_list.values[node]);
      client_destroy(client);
    }
  fd_table_destroy(&client_map, NULL, NULL);
  linked_list_destroy(&client_list);
  
  /* Re-exec the server. */
  reexec_server(argc, argv, reexec);
  
 pfail:
  perror(*argv);
  if (reexec_fd >= 0)
    close(reexec_fd);
  shm_unlink(shm_path);
}


/**
 * Attempt to reload the server after a re-execution
 * 
 * @param  socket_fd  The file descriptor of the server socket
 */
void complete_reexec(int socket_fd)
{
  pid_t pid = getpid();
  int reexec_fd, r;
  char shm_path[NAME_MAX + 1];
  
  /* Acquire access to marshalled data. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
  reexec_fd = shm_open(shm_path, O_RDONLY, S_IRWXU);
  fail_if (reexec_fd < 0); /* Critical. */
  
  /* Unmarshal state. */
  r = unmarshal_server(reexec_fd);
  
  /* Close and unlink marshalled data. */
  close(reexec_fd);
  shm_unlink(shm_path);
  
  /* Recover after failure. */
  if (r < 0)
    {
      /* Close all files (hopefully sockets) we do not know what they are. */
      close_files((fd > 2) && (fd != socket_fd) && (fd_table_contains_key(&client_map, fd) == 0));
    }
  
  return;
 pfail:
  perror(*argv);
  abort();
}

