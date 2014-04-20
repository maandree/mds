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
 * The root directory of all runtime data stored by MDS
 */
#ifndef MDS_RUNTIME_ROOT_DIRECTORY
#define MDS_RUNTIME_ROOT_DIRECTORY "/run/mds"
#endif


/**
 * The user ID for the root user
 */
#ifndef ROOT_USER_UID
#define ROOT_USER_UID  0
#endif


#endif

