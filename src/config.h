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
#ifndef MDS_CONFIG_H
#define MDS_CONFIG_H


/**
 * The name under which this package is installed
 */
#ifndef PKGNAME
#define PKGNAME  "mds"
#endif


/**
 * The root directory of all runtime data stored by MDS
 */
#ifndef MDS_RUNTIME_ROOT_DIRECTORY
#define MDS_RUNTIME_ROOT_DIRECTORY  "/run/" PKGNAME
#endif


/**
 * The user ID for the root user
 */
#ifndef ROOT_USER_UID
#define ROOT_USER_UID  0
#endif


/**
 * The group ID for the root group
 */
#ifndef ROOT_GROUP_GID
#define ROOT_GROUP_GID  0
#endif


/**
 * The group ID for the nobody group
 */
#ifndef NOBODY_GROUP_GID
#define NOBODY_GROUP_GID  ROOT_GROUP_GID
#endif

/* There three names above are redundant, but hat is to avoid errors. */


/**
 * The byte length of the authentication token
 */
#ifndef TOKEN_LENGTH
#define TOKEN_LENGTH  1024
#endif


/**
 * Random number generator to use for generating a token
 */
#ifndef TOKEN_RANDOM
#define TOKEN_RANDOM  "/dev/urandom"
#endif


/**
 * The maximum number of command line arguments to allow
 */
#ifndef ARGC_LIMIT
#define ARGC_LIMIT  50
#endif


/**
 * The number of additional arguments a libexec server may have
 */
#ifndef LIBEXEC_ARGC_EXTRA_LIMIT
#define LIBEXEC_ARGC_EXTRA_LIMIT  5
#endif


/**
 * The maximum number of display allowed on the system
 */
#ifndef DISPLAY_MAX
#define DISPLAY_MAX  1000
#endif


/**
 * The name of the environment variable that
 * indicates the index of the display
 */
#ifndef DISPLAY_ENV
#define DISPLAY_ENV  "MDS_DISPLAY"
#endif


/**
 * The directory where all servers are installed
 */
#ifndef LIBEXECDIR
#define LIBEXECDIR  "/usr/libexec"
#endif


/**
 * The minimum time that most have elapsed for respawning to be allowed.
 */
#ifndef RESPAWN_TIME_LIMIT_SECONDS
#define RESPAWN_TIME_LIMIT_SECONDS  5
#endif


#endif

