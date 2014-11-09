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
#ifndef MDS_MDS_KBDC_PARSE_ERROR_H
#define MDS_MDS_KBDC_PARSE_ERROR_H


#include <stddef.h>
#include <stdio.h>


/**
 * Not an error, simply a note about the previous error or warning
 */
#define  MDS_KBDC_PARSE_ERROR_NOTE  1

/**
 * A warning, must likely an error that is not fatal to the compilation
 */
#define  MDS_KBDC_PARSE_ERROR_WARNING  2

/**
 * An error, the compilation will halt
 */
#define  MDS_KBDC_PARSE_ERROR_ERROR  3

/**
 * Internal compiler error or system error, compilation halts
 */
#define  MDS_KBDC_PARSE_ERROR_INTERNAL_ERROR  4



/**
 * Description of an parsing error
 */
typedef struct mds_kbdc_parse_error
{
  /**
   * Either of:
   * - `MDS_KBDC_PARSE_ERROR_NOTE`
   * - `MDS_KBDC_PARSE_ERROR_WARNING`
   * - `MDS_KBDC_PARSE_ERROR_ERROR`
   * - `MDS_KBDC_PARSE_ERROR_INTERNAL_ERROR`
   */
  int severity;
  
  /**
   * If zero, disregard `.line`, `.start`, `.end` and `.code`
   */
  int error_is_in_file;
  
  /**
   * The pathname of the file with the error
   */
  char* pathname;
  
  /**
   * The line where the error occurred, zero-based
   */
  size_t line;
  
  /**
   * The byte where the error started, inclusive, zero-based
   */
  size_t start;
  
  /**
   * The byte where the error ended, exclusive, zero-based
   */
  size_t end;
  
  /**
   * The code on the line where the error occurred
   */
  char* code;
  
  /**
   * Description of the error
   */
  char* description;
  
} mds_kbdc_parse_error_t;



/**
 * Print information about a parsing error
 * 
 * @param  this    The error structure
 * @param  output  The output file
 */
void mds_kbdc_parse_error_print(const mds_kbdc_parse_error_t* restrict this, FILE* restrict output);


/**
 * Release all resources allocated in a `mds_kbdc_parse_error_t*`
 * 
 * @param  this  The error structure
 */
void mds_kbdc_parse_error_destroy(mds_kbdc_parse_error_t* restrict this);

/**
 * Release all resources allocated in a `mds_kbdc_parse_error_t*`
 * and release the allocation of the structure itself
 * 
 * @param  this  The error structure
 */
void mds_kbdc_parse_error_free(mds_kbdc_parse_error_t* restrict this);

/**
 * Release all resources allocated in a `NULL`-terminated group
 * of `mds_kbdc_parse_error_t*`:s
 * 
 * @param  this  The group of error structures
 */
void mds_kbdc_parse_error_destroy_all(mds_kbdc_parse_error_t** restrict these);

/**
 * Release all resources allocated in a `NULL`-terminated group of
 * `mds_kbdc_parse_error_t*`:s and release the allocation of the group itself
 * 
 * @param  this  The group of error structures
 */
void mds_kbdc_parse_error_free_all(mds_kbdc_parse_error_t** restrict these);


#endif

