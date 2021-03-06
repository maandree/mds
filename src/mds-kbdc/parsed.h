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
#ifndef MDS_MDS_KBDC_PARSED_H
#define MDS_MDS_KBDC_PARSED_H


#include "tree.h"
#include "raw-data.h"
#include "parse-error.h"
#include "string.h"

#include <libmdsserver/macros.h>

#include <stddef.h>
#include <stdio.h>



/**
 * @param  RESULT:mds_kbdc_parsed_t*      The parsing result
 * @param  SEVERITY:int                   * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ERROR_IS_IN_FILE:int           Whether the error is in the layout code
 * @param  LINE:size_t                    The line where the error occurred, zero-based
 * @param  START:size_t                   The byte where the error started, on the line, inclusive, zero-based
 * @param  END:size_t                     The byte where the error ended, on the line, exclusive, zero-based
 * @param  WITH_DESCRIPTION:int           Whether a description should be stored
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR_(RESULT, SEVERITY, ERROR_IS_IN_FILE, LINE, START, END, WITH_DESCRIPTION, ...)\
	do {\
		error = mds_kbdc_parsed_new_error(RESULT, MDS_KBDC_PARSE_ERROR_##SEVERITY,\
		                                  ERROR_IS_IN_FILE, LINE, START, END);\
		fail_if (!error);\
		fail_if (WITH_DESCRIPTION && xasprintf(error->description, __VA_ARGS__));\
	} while (0)



/**
 * Structure with parsed tree, error list
 * source code and the file's pathname
 */
typedef struct mds_kbdc_parsed {
	/**
	 * The parsed tree
	 */
	mds_kbdc_tree_t *tree;

	/**
	 * The source code of the parsed file
	 */
	mds_kbdc_source_code_t *source_code;

	/**
	 * A non-relative pathname to the parsed file.
	 * Relative filenames can be misleading as the
	 * program can have changed working directory
	 * to be able to resolve filenames.
	 */
	char *pathname;

	/**
	 * `NULL`-terminated list of found errors
	 * `NULL` if no errors that could be listed
	 * were found
	 */
	mds_kbdc_parse_error_t **errors;

	/**
	 * The number of elements allocated to `errors`
	 */
	size_t errors_size;

	/**
	 * The number of elements stored in `errors`
	 */
	size_t errors_ptr;

	/**
	 * The level of the severest countered error,
	 * 0 if none has been encountered.
	 */
	int severest_error_level;

	/**
	 * List of languages for which the layout is designed
	 */
	char **languages;

	/**
	 * The number of elements allocated to `languages`
	 */
	size_t languages_size;

	/**
	 * The number of elements stored in `languages`
	 */
	size_t languages_ptr;

	/**
	 * List of countries for which the layout is designed
	 */
	char **countries;

	/**
	 * The number of elements allocated to `countries`
	 */
	size_t countries_size;

	/**
	 * The number of elements stored in `countries`
	 */
	size_t countries_ptr;

	/**
	 * The variant of the keyboard for the languages/countries,
	 * `NULL` if not specified
	 */
	char *variant;

	/**
	 * List of strings the assembler should assume are provided
	 */
	char32_t **assumed_strings;

	/**
	 * The number of elements allocated to `assumed_strings`
	 */
	size_t assumed_strings_size;

	/**
	 * The number of elements stored in `assumed_strings`
	 */
	size_t assumed_strings_ptr;

	/**
	 * List of keys the assembler should assume are provided
	 */
	char32_t **assumed_keys;

	/**
	 * The number of elements allocated to `assumed_keys`
	 */
	size_t assumed_keys_size;

	/**
	 * The number of elements stored in `assumed_keys`
	 */
	size_t assumed_keys_ptr;
} mds_kbdc_parsed_t;



/**
 * Initialise a `mds_kbdc_parsed_t*`
 * 
 * @param  this  The `mds_kbdc_parsed_t*`
 */
void mds_kbdc_parsed_initialise(mds_kbdc_parsed_t *restrict this);

/**
 * Release all resources allocated in a `mds_kbdc_parsed_t*`
 * 
 * @param  this  The `mds_kbdc_parsed_t*`
 */
void mds_kbdc_parsed_destroy(mds_kbdc_parsed_t *restrict this);

/**
 * Check whether a fatal errors has occurred
 * 
 * @param   this  The parsing result
 * @return        Whether a fatal errors has occurred
 */
__attribute__((pure))
int mds_kbdc_parsed_is_fatal(mds_kbdc_parsed_t *restrict this);

/**
 * Print all encountered errors
 * 
 * @param  this    The parsing result
 * @param  output  The output file
 */
void mds_kbdc_parsed_print_errors(mds_kbdc_parsed_t *restrict this, FILE *output);

/**
 * Add a new error to the list
 * 
 * @param   this              The parsing result
 * @param   severity          A `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param   error_is_in_file  Whether the error is in the layout code
 * @param   line              The line where the error occurred, zero-based
 * @param   start             The byte where the error started, on the line, inclusive, zero-based
 * @param   end               The byte where the error ended, on the line, exclusive, zero-based
 * @return                    The new error on success, `NULL` on error
 */
mds_kbdc_parse_error_t *mds_kbdc_parsed_new_error(mds_kbdc_parsed_t *restrict this, int severity,
                                                  int error_is_in_file, size_t line, size_t start, size_t end);


#endif
