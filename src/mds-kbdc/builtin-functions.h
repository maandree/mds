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
#ifndef MDS_MDS_KBDC_BUILTIN_FUNCTIONS_H
#define MDS_MDS_KBDC_BUILTIN_FUNCTIONS_H


#include "string.h"

#include <stddef.h>
#include <stdarg.h>



/**
 * Check whether a function is a builtin function
 * 
 * @param   name       The name of the function
 * @param   arg_count  The number of arguments to pass to the function
 * @return             Whether the described function is a builtin function
 */
int builtin_function_defined(const char* restrict name, size_t arg_count) __attribute__((pure));

/**
 * Invoke a builtin function
 * 
 * The function will abort if an non-builtin function is addressed
 * 
 * Before invoking set/3 or get/2, please check the arguments,
 * please also check that either all or none of the arguments
 * are empty string for any of the other builtin functions
 * 
 * @param   name       The name of the function
 * @param   arg_count  The number of arguments to pass to the function
 * @param   args       The arguments to pass
 * @return             The return value of the function, `NULL` on error
 */
char32_t* builtin_function_invoke(const char* restrict name, size_t arg_count, const char32_t** restrict args);



#endif

