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
#include "parsed.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>



/**
 * Initialise a `mds_kbdc_parsed_t*`
 * 
 * @param  this  The `mds_kbdc_parsed_t*`
 */
void mds_kbdc_parsed_initialise(mds_kbdc_parsed_t* restrict this)
{
  memset(this, 0, sizeof(mds_kbdc_parsed_t));
}


/**
 * Release all resources allocated in a `mds_kbdc_parsed_t*`
 * 
 * @param  this  The `mds_kbdc_parsed_t*`
 */
void mds_kbdc_parsed_destroy(mds_kbdc_parsed_t* restrict this)
{
  mds_kbdc_tree_free(this->tree);
  mds_kbdc_source_code_destroy(this->source_code);
  free(this->source_code);
  free(this->pathname);
  mds_kbdc_parse_error_free_all(this->errors);
  while (this->languages_ptr--)
    free(this->languages[this->languages_ptr]);
  free(this->languages);
  while (this->countries_ptr--)
    free(this->countries[this->countries_ptr]);
  free(this->countries);
  free(this->variant);
  while (this->assumed_strings_ptr--)
    free(this->assumed_strings[this->assumed_strings_ptr]);
  free(this->assumed_strings);
  memset(this, 0, sizeof(mds_kbdc_parsed_t));
}


/**
 * Check whether a fatal errors has occurred
 * 
 * @param   this  The parsing result
 * @return        Whether a fatal errors has occurred
 */
int mds_kbdc_parsed_is_fatal(mds_kbdc_parsed_t* restrict this)
{
  return this->severest_error_level >= MDS_KBDC_PARSE_ERROR_ERROR;
}


/**
 * Print all encountered errors
 * 
 * @param  this    The parsing result
 * @param  output  The output file
 */
void mds_kbdc_parsed_print_errors(mds_kbdc_parsed_t* restrict this, FILE* output)
{
  mds_kbdc_parse_error_t** errors = this->errors;
  char* env = getenv("MDS_KBDC_ERRORS_ORDER");
  if (errors == NULL)
    return;
  if (env && (strcasecmp(env, "reversed") || strcasecmp(env, "reverse")))
    {
      while (*errors)
	errors++;
      while (errors-- != this->errors)
	mds_kbdc_parse_error_print(*errors, output);
    }
  else
    while (*errors)
      mds_kbdc_parse_error_print(*errors++, output);
  
}


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
mds_kbdc_parse_error_t* mds_kbdc_parsed_new_error(mds_kbdc_parsed_t* restrict this, int severity,
						  int error_is_in_file, size_t line, size_t start, size_t end)
{
  mds_kbdc_parse_error_t* error = NULL;
  size_t old_errors_ptr = this->errors_ptr;
  int saved_errno;
  
  if (this->errors_ptr + 1 >= this->errors_size)
    {
      size_t new_errors_size = this->errors_size ? (this->errors_size << 1) : 2;
      mds_kbdc_parse_error_t** new_errors = this->errors;
      
      fail_if (xrealloc(new_errors, new_errors_size, mds_kbdc_parse_error_t*));
      this->errors = new_errors;
      this->errors_size = new_errors_size;
    }
  
  fail_if (xcalloc(error, 1, mds_kbdc_parse_error_t));
  this->errors[this->errors_ptr + 0] = error;
  this->errors[this->errors_ptr + 1] = NULL;
  this->errors_ptr++;
  
  error->severity = severity;
  if (this->severest_error_level < severity)
    this->severest_error_level = severity;
  
  fail_if ((error->pathname = strdup(this->pathname)) == NULL);
  
  if ((error->error_is_in_file = error_is_in_file))
    {
      error->line = line;
      error->start = start;
      error->end = end;
      error->code = strdup(this->source_code->real_lines[line]);
      fail_if (error->code == NULL);
    }
  
  return error;
 pfail:
  saved_errno = errno;
  free(error);
  this->errors_ptr = old_errors_ptr;
  this->errors[this->errors_ptr] = NULL;
  errno = saved_errno;
  return NULL;
}

