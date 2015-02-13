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
#ifndef MDS_MDS_KKBD_H
#define MDS_MDS_KKBD_H


#include "mds-base.h"


/**
 * The keyboard listener thread's main function
 * 
 * @param   data  Input data
 * @return        Output data
 */
void* keyboard_loop(void* data);

/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void);

/**
 * Handle the received message after it has been
 * identified to contain `Command: enumerate-keyboards`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_modify_id   The value of the `Modify ID`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_enumerate_keyboards(const char* recv_client_id, const char* recv_message_id,
			       const char* recv_modify_id);

/**
 * Handle the received message after it has been
 * identified to contain `Command: keyboard-enumeration`
 * 
 * @param   recv_modify_id  The value of the `Modify ID`-header, `NULL` if omitted
 * @return                  Zero on success, -1 on error
 */
int handle_keyboard_enumeration(const char* recv_modify_id);

/**
 * Handle the received message after it has been
 * identified to contain `Command: set-keyboard-leds`
 * 
 * @param   recv_active    The value of the `Active`-header, `NULL` if omitted
 * @param   recv_mask      The value of the `Mask`-header, `NULL` if omitted
 * @param   recv_keyboard  The value of the `Keyboard`-header, `NULL` if omitted
 * @return                 Zero on success, -1 on error
 */
int handle_set_keyboard_leds(const char* recv_active, const char* recv_mask,
			     const char* recv_keyboard);

/**
 * Handle the received message after it has been
 * identified to contain `Command: get-keyboard-leds`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_keyboard    The value of the `Keyboard`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_get_keyboard_leds(const char* recv_client_id, const char* recv_message_id,
			     const char* recv_keyboard);

/**
 * Handle the received message after it has been
 * identified to contain `Command: keycode-map`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_action      The value of the `Action`-header, `NULL` if omitted
 * @param   recv_keyboard    The value of the `Keyboard`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_keycode_map(const char* recv_client_id, const char* recv_message_id,
		       const char* recv_action, const char* recv_keyboard);

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

/**
 * Send a response with an error number
 * 
 * @param   error            The error number
 * @param   recv_client_id   The client's ID
 * @param   recv_message_id  The message ID of the message the client sent
 * @return                   Zero on success, -1 on error
 */
int send_errno(int error, const char* recv_client_id, const char* recv_message_id);

/**
 * Attempt to shrink `mapping`
 */
void shrink_map(void);


#endif

