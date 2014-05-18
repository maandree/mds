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
#ifndef MDS_MDS_SERVER_RECEIVING_H
#define MDS_MDS_SERVER_RECEIVING_H


#include "client.h"


/**
 * Perform actions that should be taken when
 * a message has been received from a client
 * 
 * @param   client  The client whom sent the message
 * @return          Normally zero, but 1 if exited because of re-exec or termination
 */
int message_received(client_t* client);


#endif

