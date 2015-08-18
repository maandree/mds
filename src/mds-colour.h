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


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void);


/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Zero on success, -1 on error
 */
int full_send(const char* message, size_t length);


#endif

