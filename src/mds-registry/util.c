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
#include "util.h"

#include "../mds-base.h"

#include <libmdsserver/util.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/client-list.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>



/**
 * Convert a client ID string into a client ID integer
 * 
 * @param   str  The client ID string
 * @return       The client ID integer
 */
uint64_t parse_client_id(const char* str)
{
  char client_words[22];
  char* client_high;
  char* client_low;
  uint64_t client;
  
  strcpy(client_high = client_words, str);
  client_low = rawmemchr(client_words, ':');
  *client_low++ = '\0';
  client = (uint64_t)atoll(client_high);
  client <<= 32;
  client |= (uint64_t)atoll(client_low);
  
  return client;
}


/**
 * Free a key from a table
 * 
 * @param  obj  The key
 */
void reg_table_free_key(size_t obj)
{
  char* command = (char*)(void*)obj;
  free(command);
}


/**
 * Free a value from a table
 * 
 * @param  obj  The value
 */
void reg_table_free_value(size_t obj)
{
  client_list_t* list = (client_list_t*)(void*)obj;
  client_list_destroy(list);
  free(list);
}


/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Non-zero on success
 */
int full_send(const char* message, size_t length)
{
  size_t sent;
  
  while (length > 0)
    {
      sent = send_message(socket_fd, message, length);
      if (sent > length)
	{
	  eprint("Sent more of a message than exists in the message, aborting.");
	  return -1;
	}
      else if ((sent < length) && (errno != EINTR))
	{
	  perror(*argv);
	  return -1;
	}
      message += sent;
      length -= sent;
    }
  return 0;
}

