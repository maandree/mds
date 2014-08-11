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
#ifndef MDS_MDS_KKBD_H
#define MDS_MDS_KKBD_H


#include "mds-base.h"


/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Non-zero on success
 */
int full_send(const char* message, size_t length);

/**
 * Acquire access of the keyboard's LED:s
 * 
 * @return  Zero on success, -1 on error
 */
int open_leds(void);

/**
 * Release access of the keyboard's LED:s
 */
void close_leds(void);

/**
 * Get active LED:s on the keyboard
 * 
 * @return  Active LED:s, -1 on error
 */
int get_leds(void);

/**
 * Set active LED:s on the keyboard
 * 
 * @param   leds  Active LED:s
 * @return        Zero on success, -1 on error
 */
int set_leds(int leds);

/**
 * Acquire access of keyboard input
 * 
 * @return  Zero on success, -1 on error
 */
int open_input(void);

/**
 * Release access of keyboard input
 */
void close_input(void);

/**
 * Broadcast a keyboard input event
 * 
 * @param   scancode  The scancode
 * @param   trio      Whether the scancode has three integers rather than one
 * @return            Zero on success, -1 on error
 */
int send_key(int* restrict scancode, int trio);

/**
 * Fetch and broadcast keys until interrupted
 * 
 * @return  Zero on success, -1 on error
 */
int fetch_keys(void);


#endif

