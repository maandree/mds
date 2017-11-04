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
#ifndef MDS_MDS_KBDC_STRING_H
#define MDS_MDS_KBDC_STRING_H


#include <stdint.h>
#include <stddef.h>


/**
 * Data type for a character in a decoded string
 */
typedef int32_t char32_t;


/**
 * Get the length of a string
 * 
 * @param   string  The string
 * @return          The length of the string
 */
size_t string_length(const char32_t* restrict string) __attribute__((pure, nonnull));

/**
 * Convert a NUL-terminated UTF-8 string to a -1-terminated UTF-32 string
 * 
 * @param   string  The UTF-8 string
 * @return          The string in UTF-32, `NULL` on error
 */
char32_t* string_decode(const char* restrict string) __attribute__((nonnull));

/**
 * Convert a -1-terminated UTF-32 string to a NUL-terminated Modified UTF-8 string
 * 
 * @param   string  The UTF-32 string
 * @return          The string in UTF-8, `NULL` on error
 */
char* string_encode(const char32_t* restrict string) __attribute__((nonnull));

/**
 * Create duplicate of a string
 * 
 * @param   string  The string
 * @return          A duplicate of the string, `NULL` on error or if `string` is `NULL`
 */
char32_t* string_dup(const char32_t* restrict string);



#endif

