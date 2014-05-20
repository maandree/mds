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
#include "slavery.h"

#include "globals.h"
#include "client.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/linked-list.h>

#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Outout data
 */
void* slave_loop(void*);


/**
 * Receive a full message and update open status if the client closes
 * 
 * @param   client  The client
 * @return          Zero on success, -2 on failure, otherwise -1
 */
int fetch_message(client_t* client)
{
  int r = mds_message_read(&(client->message), client->socket_fd);
  if (r == 0)
    return 0;
  
  if (r == -2)
    eprint("corrupt message received.");
  else if (errno == ECONNRESET)
    {
      r = mds_message_read(&(client->message), client->socket_fd);
      client->open = 0;
      /* Connection closed. */
    }
  else if (errno != EINTR)
    {
      r = -2;
      perror(*argv);
    }
  
  return r;
}


/**
 * Create, start and detache a slave thread
 * 
 * @param   thread    The address at where to store the thread
 * @param   slave_fd  The file descriptor of the slave's socket
 * @return            Zero on success, -1 on error, error message will have been printed
 */
int create_slave(pthread_t* thread_slot, int slave_fd)
{
  if ((errno = pthread_create(thread_slot, NULL, slave_loop, (void*)(intptr_t)slave_fd)))
    {
      perror(*argv);
      with_mutex (slave_mutex, running_slaves--;);
      return -1;
    }
  if ((errno = pthread_detach(*thread_slot)))
    {
      perror(*argv);
      return -1;
    }
  return 0;
}


/**
 * Initialise a client, except for threading
 * 
 * @param   client_fd  The file descriptor of the client's socket
 * @return             The client information, NULL on error
 */
client_t* initialise_client(int client_fd)
{
  ssize_t entry = LINKED_LIST_UNUSED;
  int locked = 0;
  client_t* information;
  int errno_;
  size_t tmp;
  
  /* Create information table. */
  fail_if (xmalloc(information, 1, client_t));
  client_initialise(information);
  
  /* Add to list of clients. */
  fail_if ((errno = pthread_mutex_lock(&slave_mutex)));
  locked = 1;
  entry = linked_list_insert_end(&client_list, (size_t)(void*)information);
  fail_if (entry == LINKED_LIST_UNUSED);
  
  /* Add client to table. */
  tmp = fd_table_put(&client_map, client_fd, (size_t)(void*)information);
  fail_if ((tmp == 0) && errno);
  pthread_mutex_unlock(&slave_mutex);
  locked = 0;
  
  /* Fill information table. */
  information->list_entry = entry;
  information->socket_fd = client_fd;
  information->open = 1;
  fail_if (mds_message_initialise(&(information->message)));
  
  return information;
  
  
 pfail:
  errno_ = errno;
  if (locked)
    pthread_mutex_unlock(&slave_mutex);
  free(information);
  if (entry != LINKED_LIST_UNUSED)
    {
      with_mutex (slave_mutex, linked_list_remove(&client_list, entry););
    }
  errno = errno_;
  return NULL;
}

