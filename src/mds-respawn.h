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
#ifndef MDS_MDS_RESPAWN_H
#define MDS_MDS_RESPAWN_H


#include "mds-base.h"

#include <sys/types.h>
#include <time.h>



/**
 * The server has not started yet
 */
#define UNBORN  0

/**
 * The server is up and running
 */
#define ALIVE  1

/**
 * The server has crashed and will be respawn momentarily
 */
#define DEAD  2

/**
 * The server crashed too fast, it will only respawn if SIGUSR2 is received
 */
#define DEAD_AND_BURIED  3

/**
 * The server has exited successfully, it will never be respawn again
 */
#define CREMATED  4



/**
 * Check that a state is a value state
 * 
 * @param   VALUE:int  The state
 * @return  :int       Whether the state is value
 */
#define validate_state(VALUE)  ((UNBORN <= VALUE) && (VALUE <= CREMATED))



/**
 * The state and identifier of a server
 */
typedef struct server_state
{
  /**
   * The server's process ID
   */
  pid_t pid;
  
  /**
   * The server's state
   */
  int state;
  
  /**
   * The time (monotonic) the server started
   */
  struct timespec started;
  
} server_state_t;



#endif

