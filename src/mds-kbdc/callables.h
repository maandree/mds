/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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
#ifndef MDS_MDS_KBDC_CALLABLES_H
#define MDS_MDS_KBDC_CALLABLES_H


#include "tree.h"
#include "include-stack.h"


/**
 * Destroy the callable storage
 */
void callables_terminate(void);

/**
 * Store a callable
 * 
 * @param   name                    The name of the callable
 * @param   arg_count               The number of arguments the callable takes
 *                                  if `name` is suffixless, otherwise zero
 * @param   callable                The callable
 * @param   callable_include_stack  The include-stack for the callable
 * @return                          Zero on success, -1 on error
 */
int callables_set(const char *restrict name, size_t arg_count, mds_kbdc_tree_t *restrict callable,
                  mds_kbdc_include_stack_t *restrict callable_include_stack);

/**
 * Get a stored callable
 * 
 * @param  name                    The name of the callable
 * @param  arg_count               The number of arguments the callable takes
 *                                 if `name` is suffixless, otherwise zero
 * @param  callable                Output parameter for the callable
 * @param  callable_include_stack  Output parameter for the include-stack for the callable
 */
void callables_get(const char *restrict name, size_t arg_count, mds_kbdc_tree_t **restrict callable,
                   mds_kbdc_include_stack_t **restrict callable_include_stack);


#endif
