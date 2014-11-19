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
#ifndef MDS_MDS_KBDC_MAKE_TREE_H
#define MDS_MDS_KBDC_MAKE_TREE_H


#include "tree.h"
#include "parse-error.h"


/**
 * Parse a file into a syntax tree
 * 
 * @param   filename  The filename of the file to parse
 * @param   result    Output parameter for the root of the tree, `NULL` if -1 is returned
 * @param   errors    `NULL`-terminated list of found error, `NULL` if no errors were found or if -1 is returned
 * @return            -1 if an error occursed that cannot be stored in `*errors`, zero otherwise
 */
int parse_to_tree(const char* restrict filename, mds_kbdc_tree_t** restrict result,
		  mds_kbdc_parse_error_t*** restrict errors);


#endif

