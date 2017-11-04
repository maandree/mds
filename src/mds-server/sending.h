/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_MDS_SERVER_SENDING_H
#define MDS_MDS_SERVER_SENDING_H


#include "multicast.h"
#include "client.h"


/**
 * Multicast a message
 * 
 * @param  multicast  The multicast message
 */
__attribute__((nonnull))
void multicast_message(multicast_t *multicast);

/**
 * Send the next message in a clients multicast queue
 * 
 * @param  client  The client
 */
__attribute__((nonnull))
void send_multicast_queue(client_t *client);

/**
 * Send the messages that are in a clients reply queue
 * 
 * @param  client  The client
 */
__attribute__((nonnull))
void send_reply_queue(client_t *client);


#endif
