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
#ifndef MDS_MDS_CLIPBOARD_H
#define MDS_MDS_CLIPBOARD_H


#include "mds-base.h"

#include <stddef.h>
#include <time.h>
#include <stdint.h>



/**
 * Delete entry only when needed
 */
#define CLIPITEM_AUTOPURGE_NEVER  0

/**
 * Delete entry when the client closes, or needed
 */
#define CLIPITEM_AUTOPURGE_UPON_DEATH  1

/**
 * Delete entry when a point in time has elapsed, or needed
 */
#define CLIPITEM_AUTOPURGE_UPON_CLOCK  2

/**
 * Delete entry when the client closes or when a
 * point in time has elapsed, or when needed 
 */
#define CLIPITEM_AUTOPURGE_UPON_DEATH_OR_CLOCK  3


/**
 * A clipboard entry
 */
typedef struct clipitem
{
  /**
   * The stored content
   */
  char* content;
  
  /**
   * The length of the stored content
   */
  size_t length;
  
  /**
   * Time of planned death if `autopurge` is `CLIPITEM_AUTOPURGE_UPON_CLOCK`
   */
  struct timespec dethklok;
  
  /**
   * The client that issued the inclusion of this entry
   */
  uint64_t client;
  
  /**
   * Rule for automatic deletion
   */
  int autopurge;
  
} clipitem_t;


/**
 * The number of levels in the clipboard
 */
#define CLIPBOARD_LEVELS  3



/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Non-zero on success
 */
int full_send(const char* message, size_t length);

/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void);

/**
 * Remove expired entries
 * 
 * @return  Zero on success, -1 on error
 */
int clipboard_danger(void);

/**
 * Remove entries in the clipboard added by a client
 * 
 * @param   recv_client_id  The ID of the client
 * @return                  Zero on success, -1 on error
 */
int clipboard_death(const char* recv_client_id);

/**
 * Add a new entry to the clipboard
 * 
 * @param   level           The clipboard level
 * @param   time_to_live    When the entry should be removed
 * @param   recv_client_id  The ID of the client
 * @return                  Zero on success, -1 on error
 */
int clipboard_add(int level, const char* time_to_live, const char* recv_client_id);

/**
 * Read an entry to the clipboard
 * 
 * @param   level            The clipboard level
 * @param   index            The index of the clipstack element
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The message ID of the received message
 * @return                   Zero on success, -1 on error
 */
int clipboard_read(int level, size_t index, const char* recv_client_id, const char* recv_message_id);

/**
 * Clear a clipstack
 * 
 * @param   level  The clipboard level
 * @return         Zero on success, -1 on error
 */
int clipboard_clear(int level);

/**
 * Resize a clipstack
 * 
 * @param   level  The clipboard level
 * @param   size   The new clipstack size
 * @return         Zero on success, -1 on error
 */
int clipboard_set_size(int level, size_t size);

/**
 * Get the size of a clipstack and how many entries it contains
 * 
 * @param   level            The clipboard level
 * @param   recv_client_id   The ID of the client
 * @param   recv_message_id  The message ID of the received message
 * @return                   Zero on success, -1 on error
 */
int clipboard_get_size(int level, const char* recv_client_id, const char* recv_message_id);


#endif

