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

#include "util.h"
#include "globals.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/hash-help.h>
#include <libmdsserver/client-list.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



/**
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
size_t marshal_server_size(void)
{
  size_t i, rc = 2 * sizeof(int) + sizeof(int32_t) + 3 * sizeof(size_t);
  hash_entry_t* entry;
  
  rc += mds_message_marshal_size(&received);
  
  foreach_hash_table_entry (reg_table, i, entry)
    {
      char* command = (char*)(void*)(entry->key);
      size_t len = strlen(command) + 1;
      client_list_t* list = (client_list_t*)(void*)(entry->value);
      
      rc += len + sizeof(size_t) + client_list_marshal_size(list);
    }
  
  return rc;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  size_t i, n = mds_message_marshal_size(&received);
  hash_entry_t* entry;
  
  buf_set_next(state_buf, int, MDS_REGISTRY_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, int32_t, message_id);
  buf_set_next(state_buf, size_t, n);
  mds_message_marshal(&received, state_buf);
  state_buf += n / sizeof(char);
  
  buf_set_next(state_buf, size_t, reg_table.capacity);
  buf_set_next(state_buf, size_t, reg_table.size);
  foreach_hash_table_entry (reg_table, i, entry)
    {
      char* command = (char*)(void*)(entry->key);
      size_t len = strlen(command) + 1;
      client_list_t* list = (client_list_t*)(void*)(entry->value);
      
      memcpy(state_buf, command, len * sizeof(char));
      state_buf += len;
      
      n = client_list_marshal_size(list);
      buf_set_next(state_buf, size_t, n);
      client_list_marshal(list, state_buf);
      state_buf += n / sizeof(char);
    }
  
  hash_table_destroy(&reg_table, (free_func*)reg_table_free_key, (free_func*)reg_table_free_value);
  mds_message_destroy(&received);
  return 0;
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
int unmarshal_server(char* state_buf)
{
  char* command;
  client_list_t* list;
  size_t i, n, m;
  int stage = 0;
  
  /* buf_get_next(state_buf, int, MDS_REGISTRY_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, int32_t, message_id);
  buf_get_next(state_buf, size_t, n);
  fail_if (mds_message_unmarshal(&received, state_buf));
  state_buf += n / sizeof(char);
  stage = 1;
  
  buf_get_next(state_buf, size_t, n);
  fail_if (hash_table_create_tuned(&reg_table, n));
  buf_get_next(state_buf, size_t, n);
  for (i = 0; i < n; i++)
    {
      stage = 1;
      fail_if ((command = strdup(state_buf)) == NULL);
      state_buf += strlen(command) + 1;
      
      stage = 2;
      fail_if ((list = malloc(sizeof(client_list_t))) == NULL);
      buf_get_next(state_buf, size_t, m);
      stage = 3;
      fail_if (client_list_unmarshal(list, state_buf));
      state_buf += m / sizeof(char);
      
      hash_table_put(&reg_table, (size_t)(void*)command, (size_t)(void*)list);
      fail_if (errno);
    }
  
  reg_table.key_comparator = (compare_func*)string_comparator;
  reg_table.hasher = (hash_func*)string_hash;
  
  return 0;
 pfail:
  perror(*argv);
  mds_message_destroy(&received);
  if (stage >= 1)
    hash_table_destroy(&reg_table, (free_func*)reg_table_free_key, (free_func*)reg_table_free_value);
  if (stage >= 2)
    free(command);
  if (stage >= 3)
    {
      client_list_destroy(list);
      free(list);
    }
  abort();
  return -1;
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) reexec_failure_recover(void)
{
  return -1;
}

