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
#ifndef MDS_MDS_KBDC_CALL_STACK_H
#define MDS_MDS_KBDC_CALL_STACK_H


#include "parsed.h"
#include "include-stack.h"



/**
 * Prepare for usage of call-stacks
 * 
 * @param  result  The `result` parameter of root procedure that requires the call-stack
 */
void mds_kbdc_call_stack_begin(mds_kbdc_parsed_t* restrict result);

/**
 * Cleanup after usage of call-stacks
 */
void mds_kbdc_call_stack_end(void);



#endif

