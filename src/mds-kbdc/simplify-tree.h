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
#ifndef MDS_MDS_KBDC_SIMPLIFY_TREE_H
#define MDS_MDS_KBDC_SIMPLIFY_TREE_H


#include "parsed.h"


/**
 * Simplify a tree and generate related warnings and errors in the process
 * 
 * @param   result  `result` from `parse_to_tree`, same sematics, will be updated
 * @return          -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int simplify_tree(mds_kbdc_parsed_t* restrict result);


#endif

