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
#include "slave.h"

#include "util.h"
#include "globals.h"

#include "../mds-base.h"

#include <libmdsserver/util.h>
#include <libmdsserver/macros.h>

#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>



/**
 * Notify the waiting client that it may resume
 * 
 * @param   slave  The slave
 * @return         Non-zero, `errno` will be set accordingly
 */
static int slave_notify_client(slave_t* slave)
{
  char buf[sizeof("To: %s\nIn response to: %s\nMessage ID: %" PRIi32 "\n\n") / sizeof(char) + 40];
  size_t ptr = 0, sent, left;
  
  /* Construct message headers. */
  sprintf(buf, "To: %s\nIn response to: %s\nMessage ID: %" PRIi32 "\n\n",
	  slave->client_id, slave->message_id, message_id);
  
  /* Increase message ID. */
  message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
  
  /* Send message to client. */
  left = strlen(buf);
  while (left > 0)
    {
      sent = send_message(socket_fd, buf + ptr, left);
      if ((sent < left) && errno && (errno != EINTR))
	return -1;
      left -= sent;
      ptr += sent;
    }
  
  return 0;
}


/**
 * Master function for slave threads
 * 
 * @param   data  Input data
 * @return        Output data
 */
static void* slave_loop(void* data)
{
  slave_t* slave = data;
  
  if (slave->closed)
    goto done;
  
  /* Set up traps for especially handled signals. */
  fail_if (trap_signals() < 0);
  
  fail_if ((errno = pthread_mutex_lock(&slave_mutex)));
  
  while (!reexecing && !terminating)
    {
      if ((slave->wait_set->size == 0) || slave->closed)
	break;
      pthread_cond_wait(&slave_cond, &slave_mutex);
    }
  
  if (!(slave->closed) && (slave->wait_set->size == 0))
    slave_notify_client(slave);
  
  pthread_mutex_unlock(&slave_mutex);
  
  goto done;
  
 pfail:
  xperror(*argv);
 done:
  with_mutex (slave_mutex,
	      if (!reexecing)
		linked_list_remove(&slave_list, slave->node);
	      running_slaves--;
	      if (running_slaves == 0)
		pthread_cond_signal(&slave_cond);
	     );
  return NULL;
}


/**
 * Start a slave thread
 * 
 * @param   wait_set         Set of protocols for which to wait that they become available
 * @param   recv_client_id   The ID of the waiting client
 * @param   recv_message_id  The ID of the message that triggered the waiting
 * @return                   Non-zero on error
 */
int start_slave(hash_table_t* restrict wait_set, const char* restrict recv_client_id, const char* restrict recv_message_id)
{
  slave_t* slave = slave_create(wait_set, recv_client_id, recv_message_id);
  size_t slave_address;
  ssize_t node = LINKED_LIST_UNUSED;
  
  fail_if (slave == NULL);
  fail_if ((errno = pthread_mutex_lock(&slave_mutex)));
  
  slave_address = (size_t)(void*)slave;
  slave->node = node = linked_list_insert_end(&slave_list, slave_address);
  if (slave->node == LINKED_LIST_UNUSED)
    goto pfail_in_mutex;
  
  if ((errno = pthread_create(&(slave->thread), NULL, slave_loop, (void*)(intptr_t)slave)))
    goto pfail_in_mutex;
  
  if ((errno = pthread_detach(slave->thread)))
    xperror(*argv);
  
  running_slaves++;
  pthread_mutex_unlock(&slave_mutex);
  
  return 0;
 pfail:
  xperror(*argv);
  goto more_fail;
 pfail_in_mutex:
  xperror(*argv);
  pthread_mutex_unlock(&slave_mutex);
 more_fail:
  if (node != LINKED_LIST_UNUSED)
    linked_list_remove(&slave_list, node);
  return -1;
}


/**
 * Close all slaves associated with a client
 * 
 * @param  client  The client's ID
 */
void close_slaves(uint64_t client)
{
  ssize_t node;
  with_mutex (slave_mutex,
	      foreach_linked_list_node (slave_list, node)
	        {
		  slave_t* slave = (slave_t*)(void*)(slave_list.values[node]);
		  if (slave->client == client)
		    slave->closed = 1;
		}
	     );
}


/**
 * Notify slaves that a protocol has become available
 * 
 * @param   command  The protocol
 * @return           Non-zero on error, `ernno`will be set accordingly
 */
int advance_slaves(char* command)
{
  size_t key = (size_t)(void*)command;
  int signal_slaves = 0;
  ssize_t node;
  
  if ((errno = pthread_mutex_lock(&slave_mutex)))
    return -1;
  
  foreach_linked_list_node (slave_list, node)
    {
      slave_t* slave = (slave_t*)(void*)(slave_list.values[node]);
      if (hash_table_contains_key(slave->wait_set, key))
	{
	  hash_table_remove(slave->wait_set, key);
	  signal_slaves |= slave->wait_set == 0;
	}
    }
  
  if (signal_slaves)
    pthread_cond_broadcast(&slave_cond);

  pthread_mutex_unlock(&slave_mutex);
  return 0;
}


/**
 * Create a slave
 * 
 * @param   wait_set         Set of protocols for which to wait that they become available
 * @param   recv_client_id   The ID of the waiting client
 * @param   recv_message_id  The ID of the message that triggered the waiting
 * @return                   The slave, `NULL` on error, `errno` will be set accordingly
 */
slave_t* slave_create(hash_table_t* restrict wait_set, const char* restrict recv_client_id, const char* restrict recv_message_id)
{
  slave_t* restrict rc = NULL;
  
  if (xmalloc(rc, 1, slave_t))
    return NULL;
  
  slave_initialise(rc);
  rc->wait_set = wait_set;
  rc->client = parse_client_id(recv_client_id);
  
  if ((rc->client_id = strdup(recv_client_id)) == NULL)
    goto fail;
  
  if ((rc->message_id = strdup(recv_message_id)) == NULL)
    goto fail;
  
  return rc;
  
 fail:
  slave_destroy(rc);
  free(rc);
  return NULL;
}


/**
 * Initialise a slave
 * 
 * @param  this  Memory slot in which to store the new slave information
 */
void slave_initialise(slave_t* restrict this)
{
  this->wait_set = NULL;
  this->client_id = NULL;
  this->message_id = NULL;
  this->closed = 0;
}


/**
 * Release all resources assoicated with a slave
 * 
 * @param  this  The slave information
 */
void slave_destroy(slave_t* restrict this)
{
  if (this->wait_set != NULL)
    {
      hash_table_destroy(this->wait_set, (free_func*)reg_table_free_key, NULL);
      free(this->wait_set);
      this->wait_set = NULL;
    }
  
  free(this->client_id);
  this->client_id = NULL;
  
  free(this->message_id);
  this->message_id = NULL;
}


/**
 * Calculate the buffer size need to marshal slave information
 * 
 * @param   this  The slave information
 * @return        The number of bytes to allocate to the output buffer
 */
size_t slave_marshal_size(const slave_t* restrict this)
{
  size_t rc = 2 * sizeof(int) + sizeof(ssize_t) + sizeof(size_t) + sizeof(uint64_t);
  hash_entry_t* restrict entry;
  size_t n;
  
  rc += (strlen(this->client_id) + strlen(this->message_id) + 2) * sizeof(char);
  
  foreach_hash_table_entry (*(this->wait_set), n, entry)
    {
      char* protocol = (char*)(void*)(entry->key);
      rc += strlen(protocol) + 1;
    }
  
  return rc;
}


/**
 * Marshals slave information
 * 
 * @param   this  The slave information
 * @param   data  Output buffer for the marshalled data
 * @return        The number of bytes that have been written (everything will be written)
 */
size_t slave_marshal(const slave_t* restrict this, char* restrict data)
{
  hash_entry_t* restrict entry;
  size_t n;
  
  buf_set_next(data, int, SLAVE_T_VERSION);
  buf_set_next(data, int, this->closed);
  buf_set_next(data, ssize_t, this->node);
  buf_set_next(data, uint64_t, this->client);
  
  memcpy(data, this->client_id, (strlen(this->client_id) + 1) * sizeof(char));
  data += strlen(this->client_id) + 1;
  
  memcpy(data, this->message_id, (strlen(this->message_id) + 1) * sizeof(char));
  data += strlen(this->message_id) + 1;
  
  n = this->wait_set->size;
  buf_set_next(data, size_t, n);
  
  foreach_hash_table_entry (*(this->wait_set), n, entry)
    {
      char* restrict protocol = (char*)(void*)(entry->key);
      memcpy(data, protocol, (strlen(protocol) + 1) * sizeof(char));
      data += strlen(protocol) + 1;
    }
  
  return slave_marshal_size(this);
}


/**
 * Unmarshals slave information
 * 
 * @param   this  Memory slot in which to store the new slave information
 * @param   data  In buffer with the marshalled data
 * @return        Zero on error, errno will be set accordingly, otherwise the number of read bytes.
 *                Destroy the slave information on error.
 */
size_t slave_unmarshal(slave_t* restrict this, char* restrict data)
{
  size_t key, n, m, rc = 2 * sizeof(int) + sizeof(ssize_t) + sizeof(size_t) + sizeof(uint64_t);
  char* protocol;
  
  this->wait_set = NULL;
  this->client_id = NULL;
  this->message_id = NULL;
  
  /* buf_get_next(data, int, SLAVE_T_VERSION); */
  buf_next(data, int, 1);
  
  buf_get_next(data, int, this->closed);
  buf_get_next(data, ssize_t, this->node);
  buf_get_next(data, uint64_t, this->client);
  
  n = (strlen((char*)data) + 1) * sizeof(char);
  if ((this->client_id = malloc(n)) == NULL)
    return 0;
  memcpy(this->client_id, data, n);
  data += n, rc += n;
  
  n = (strlen((char*)data) + 1) * sizeof(char);
  if ((this->message_id = malloc(n)) == NULL)
    return 0;
  memcpy(this->message_id, data, n);
  data += n, rc += n;
  
  if ((this->wait_set = malloc(sizeof(hash_table_t))) == NULL)
    return 0;
  if (hash_table_create(this->wait_set))
    return 0;
  
  buf_get_next(data, size_t, m);
  
  while (m--)
    {
      n = (strlen((char*)data) + 1) * sizeof(char);
      if ((protocol = malloc(n)) == NULL)
	return 0;
      memcpy(protocol, data, n);
      data += n, rc += n;
      
      key = (size_t)(void*)protocol;
      if (hash_table_put(this->wait_set, key, 1) == 0)
	if (errno)
	  {
	    free(protocol);
	    return 0;
	  }
    }
  
  return rc;
}


/**
 * Pretend to unmarshal slave information
 * 
 * @param   data  In buffer with the marshalled data
 * @return        The number of read bytes
 */
size_t slave_unmarshal_skip(char* restrict data)
{
  size_t n, m, rc = 2 * sizeof(int) + sizeof(ssize_t) + sizeof(size_t) + sizeof(uint64_t);
  
  /* buf_get_next(data, int, SLAVE_T_VERSION); */
  buf_next(data, int, 1);
  
  buf_next(data, int, 1);
  buf_next(data, ssize_t, 1);
  
  n = (strlen((char*)data) + 1) * sizeof(char);
  data += n, rc += n;
  
  n = (strlen((char*)data) + 1) * sizeof(char);
  data += n, rc += n;
  
  buf_get_next(data, size_t, m);
  
  while (m--)
    {
      n = (strlen((char*)data) + 1) * sizeof(char);
      data += n, rc += n;
    }
  
  return rc;
}

