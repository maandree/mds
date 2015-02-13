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
#ifndef MDS_MDS_KBDC_PATHS_H
#define MDS_MDS_KBDC_PATHS_H


/**
 * Get the current working directory
 * 
 * @return  The current working directory
 */
char* curpath(void);

/**
 * Get the absolute path of a file
 * 
 * @param   path  The filename of the file
 * @return        The file's absolute path, `NULL` on error
 */
char* abspath(const char* path);

/**
 * Get a relative path of a file
 * 
 * @param   path  The filename of the file
 * @param   base  The pathname of the base directory,
 *                `NULL` for the current working directroy
 * @return        The file's relative path, `NULL` on error
 */
char* relpath(const char* path, const char* base);


#endif

