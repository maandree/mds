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
#ifndef MDS_MDS_SERVER_QUEUED_INTERCEPTION_H
#define MDS_MDS_SERVER_QUEUED_INTERCEPTION_H


#include "client.h"

#include <stdint.h>


/**
 * A queued interception
 */
typedef struct queued_interception
{
  /**
   * The intercepting client
   */
  client_t* client;
  
  /**
   * The interception priority
   */
  int64_t priority;
  
  /**
   * Whether the messages may get modified by the client
   */
  int modifying;
  
} queued_interception_t;


#endif

