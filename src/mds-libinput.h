/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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
#ifndef MDS_MDS_LIBINPUT_H
#define MDS_MDS_LIBINPUT_H


#include "mds-base.h"

#include <libinput.h>
#include <libudev.h>



/**
 * The event listener thread's main function
 * 
 * @param   data  Input data
 * @return        Output data
 */
void *event_loop(void *data);

/**
 * Handle an event from libinput
 * 
 * @return  Zero on success, -1 on error
 */
int handle_event(void);

/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void);

/**
 * Used by libinput to open a device
 * 
 * @param   path      The filename of the device
 * @param   flags     The flags to open(3)
 * @param   userdata  Not used
 * @return            The file descriptor, or `-errno` on error
 */
int open_restricted(const char *path, int flags, void *userdata);

/**
 * Used by libinput to close device
 * 
 * @param  fd        The file descriptor of the device
 * @param  userdata  Not used
 */
void close_restricted(int fd, void *userdata);

/**
 * Acquire access of input devices
 * 
 * @return  Zero on success, -1 on error
 */
int initialise_libinput(void);

/**
 * Release access of input devices
 */
void terminate_libinput(void);

/**
 * Add a device to the device list
 * 
 * @param   dev  The device
 * @return       Zero on success, -1 on error
 */
int add_device(struct libinput_device *dev);

/**
 * Remove a device from the device list
 * 
 * @param  dev  The device
 */
void remove_device(struct libinput_device *dev);

/**
 * Pack the device list
 */
void pack_devices(void);

/**
 * The the state of the server
 */
void dump_info(void);


#endif
