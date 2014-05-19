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
#ifndef MDS_MDS_BASE_H
#define MDS_MDS_BASE_H


/**
 * Characteristics of the server
 */
typedef struct server_characteristics
{
  /**
   * Setting this to zero will cause the server to drop privileges as a security precaution
   */
  int require_privileges;
  
} server_characteristics_t;


/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave.
 */
extern server_characteristics_t server_characteristics;


/**
 * Number of elements in `argv`
 */
extern int argc;

/**
 * Command line arguments
 */
extern char** argv;

/**
 * Whether the server has been respawn
 * rather than this being the initial spawn.
 * This will be at least as true as `is_reexec`.
 */
extern int is_respawn;

/**
 * Whether the server is continuing
 * from a self-reexecution
 */
extern int is_reexec;


/**
 * The file descriptor of the socket
 * that is connected to the server
 */
extern int socket_fd;



#endif

