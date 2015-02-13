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
#ifndef MDS_MDS_KBDC_VARIABLES_H
#define MDS_MDS_KBDC_VARIABLES_H


#include "tree.h"
#include "string.h"



/**
 * Destroy the variable storage
 */
void variables_terminate(void);

/**
 * Push the variable-stack, making it
 * possible to shadow all variables
 * 
 * @return  Zero on success, -1 on error
 */
void variables_stack_push(void);

/**
 * Undo the actions of `variables_stack_push`
 * and all additions to the variable storage
 * since it was last called (without a
 * corresponding call to this function)
 */
void variables_stack_pop(void);

/**
 * Check whether a let will override a variable
 * rather the define or shadow it
 * 
 * @param   variable  The variable index
 * @return            Whether a let will override the variable
 */
int variables_let_will_override(size_t variable) __attribute__((pure));

/**
 * Assign a value to a variable, and define or shadow it in the process
 * 
 * @param   variable  The variable index
 * @param   value     The variable's new value
 * @return            Zero on success, -1 on error
 */
int variables_let(size_t variable, mds_kbdc_tree_t* restrict value);

/**
 * Get the value currently assigned to a variable
 * 
 * The function cannot fail, `NULL` is however returned
 * if the variable is not defined
 * 
 * @param   variable  The variable index
 * @return            The variable's value, `NULL` if not defined
 */
mds_kbdc_tree_t* variables_get(size_t variable) __attribute__((pure));

/**
 * Mark a variable as having been unsed in a for-loop in the current scope
 * 
 * @param   variable  The variable index, must already be defined
 * @return            Zero on success, -1 on error
 */
int variables_was_used_in_for(size_t variable);

/**
 * Check whether a variable has been used in a for-loop in the current scope
 * 
 * @param   variable  The variable index, must already be defined
 * @return            Whether `variables_was_used_in_for` has been unused on the variable
 */
int variables_has_been_used_in_for(size_t variable) __attribute__((pure));


#endif

