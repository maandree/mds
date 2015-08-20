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
#ifndef MDS_MDS_COLOUR_H
#define MDS_MDS_COLOUR_H


#include "mds-base.h"

#include <libmdsserver/hash-list.h>

#include <stdint.h>



/**
 * Data structure for colour
 */
typedef struct colour
{
  /**
   * The value of the red channel
   */
  uint64_t red;
  
  /**
   * The value of the green channel
   */
  uint64_t green;
  
  /**
   * The value of the blue channel
   */
  uint64_t blue;
  
  /**
   * The number of bytes with which
   * each channel is encoded
   */
  int bytes;
  
} colour_t;



/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void);

/**
 * Handle the received message after it has been
 * identified to contain `Command: list-colours`
 * 
 * @param   recv_client_id       The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id      The value of the `Message ID`-header
 * @param   recv_include_values  The value of the `Include values`-header, `NULL` if omitted
 * @return                       Zero on success, -1 on error
 */
int handle_list_colours(const char* recv_client_id, const char* recv_message_id,
			const char* recv_include_values);

/**
 * Handle the received message after it has been
 * identified to contain `Command: get-colour`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_name        The value of the `Name`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_get_colour(const char* recv_client_id, const char* recv_message_id, const char* recv_name);

/**
 * Handle the received message after it has been
 * identified to contain `Command: set-colour`
 * 
 * @param   recv_name    The value of the `Name`-header, `NULL` if omitted
 * @param   recv_remove  The value of the `Remove`-header, `NULL` if omitted
 * @param   recv_bytes   The value of the `Bytes`-header, `NULL` if omitted
 * @param   recv_red     The value of the `Red`-header, `NULL` if omitted
 * @param   recv_green   The value of the `Green`-header, `NULL` if omitted
 * @param   recv_blue    The value of the `Blue`-header, `NULL` if omitted
 * @return               Zero on success, -1 on error
 */
int handle_set_colour(const char* recv_name, const char* recv_remove, const char* recv_bytes,
		      const char* recv_red, const char* recv_green, const char* recv_blue);


CREATE_HASH_LIST_SUBCLASS(colour_list, char* restrict, const char* restrict, colour_t)


/**
 * Free the key and value of an entry in a colour list
 * 
 * @param  entry  The entry
 */
void colour_list_entry_free(colour_list_entry_t* entry);


#endif

