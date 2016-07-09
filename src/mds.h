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
#ifndef MDS_MDS_H
#define MDS_MDS_H


#include <sys/types.h>
#include <stdio.h>


/**
 * Read a PID-file and determine whether it refers to an non-existing process
 * 
 * @param   f  The PID-file
 * @return     Whether the PID-file is not longer used
 */
__attribute__((nonnull))
int is_pid_file_reusable(FILE* f);

/**
 * Parse an LF-terminated string as a non-negative `pid_t`
 * 
 * @param   str  The string
 * @param   n    The length of the string, excluding LF-termination
 * @return       The pid, `(pid_t)-1` if malformated
 */
__attribute__((pure, nonnull))
pid_t parse_pid_t(const char* str, size_t n);

/**
 * Start master server and respawn it if it crashes
 * 
 * @param   fd  The file descriptor of the socket
 * @return      Non-zero on error
 */
int spawn_and_respawn_server(int fd);

/**
 * Create a directory owned by the root user and root group
 * 
 * @param   pathname  The pathname of the directory to create
 * @return            Non-zero on error
 */
__attribute__((nonnull))
int create_directory_root(const char* pathname);

/**
 * Create a directory owned by the real user and nobody group
 * 
 * @param   pathname  The pathname of the directory to create
 * @return            Non-zero on error
 */
__attribute__((nonnull))
int create_directory_user(const char* pathname);

/**
 * Recursively remove a directory
 * 
 * @param   pathname  The pathname of the directory to remove
 * @return            Non-zero on error
 */
__attribute__((nonnull))
int unlink_recursive(const char* pathname);


#endif

