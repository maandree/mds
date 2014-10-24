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
#ifndef MDS_MDS_KBDC_FUNCTIONS_H
#define MDS_MDS_KBDC_FUNCTIONS_H


#include "string.h"

#include <stddef.h>
#include <stdarg.h>


/**
 * Check whether a function is defined.
 * 
 * @param   name       The name of the function.
 * @param   arg_count  The number of arguments to pass to the function.
 * @return             Whether the function is defined for the selected number of arguments.
 */
int function_check_defined(const char32_t* restrict name, size_t arg_count) __attribute__((nonnull));

/**
 * Invoke a function defined in the keyboard layout source code, or that is builtin.
 * 
 * @param   name           The name of the function.
 * @param   arg_count      The number of arguments to pass to the function.
 * @param   ...:char32_t*  The arguments to pass, do not end with a sentinel.
 * @return                 The return value of the function, `NULL` on error.
 */
char32_t* function_invoke(const char32_t* restrict name, size_t arg_count, ...) __attribute__((nonnull));

/* TODO define functions, check if function is built in */


#endif

