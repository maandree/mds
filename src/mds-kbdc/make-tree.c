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
#include "make-tree.h"

#include "raw-data.h"

#include <libmdsserver/macros.h>

#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>



/**
 * Parse a file into a syntex tree
 * 
 * @param   filename  The filename of the file to parse
 * @param   result    Output parameter for the root of the tree, `NULL` if -1 is returned
 * @param   errors    `NULL`-terminated list of found error, `NULL` if no errors were found or if -1 is returned
 * @return            -1 if an error occursed that cannot be stored in `*errors`, zero otherwise
 */
int parse_to_tree(const char* restrict filename, mds_kbdc_tree_t** restrict result,
		  mds_kbdc_parse_error_t*** restrict errors)
{
#define xasprintf(var, ...)  (asprintf(&(var), __VA_ARGS__) < 0 ? (var = NULL, -1) : 0)
#define NEW_ERROR(ERROR_IS_IN_FILE, SEVERITY, ...)					\
  if (errors_ptr + 1 >= errors_size)							\
    {											\
      errors_size = errors_size ? (errors_size << 1) : 2;				\
      fail_if (xxrealloc(old_errors, *errors, errors_size, mds_kbdc_parse_error_t*));	\
    }											\
  fail_if (xcalloc(error, 1, mds_kbdc_parse_error_t));					\
  (*errors)[errors_ptr + 0] = error;							\
  (*errors)[errors_ptr + 1] = NULL;							\
  errors_ptr++;										\
  error->line  = line_i;								\
  error->severity = MDS_KBDC_PARSE_ERROR_##SEVERITY;					\
  error->error_is_in_file = ERROR_IS_IN_FILE;						\
  error->start = (size_t)(line - source_code.lines[line_i]);				\
  error->end   = (size_t)(end  - source_code.lines[line_i]);				\
  fail_if ((error->pathname = strdup(pathname)) == NULL);				\
  fail_if ((error->code = strdup(source_code.real_lines[line_i])) == NULL);		\
  fail_if (xasprintf(error->description, __VA_ARGS__))
  
  mds_kbdc_parse_error_t* error;
  mds_kbdc_parse_error_t** old_errors = NULL;
  char* pathname;
  source_code_t source_code;
  size_t errors_size = 0;
  size_t errors_ptr = 0;
  size_t line_i, line_n;
  const char** keyword_stack = NULL;
  size_t stack_ptr = 0;
  int saved_errno;
  
  *result = NULL;
  *errors = NULL;
  source_code_initialise(&source_code);
  
  /* Get a non-relative pathname for the file, relative filenames
   * can be misleading as the program can have changed working
   * directroy to be able to resolve filenames. */
  pathname = realpath(filename, NULL);
  fail_if (pathname == NULL);
  
  /* Check that the file exists and can be read. */
  if (access(pathname, R_OK) < 0)
    {
      saved_errno = errno;
      fail_if (xmalloc(*errors, 2, mds_kbdc_parse_error_t*));
      fail_if (xmalloc(**errors, 1, mds_kbdc_parse_error_t));
      (*errors)[1] = NULL;
      
      (**errors)->severity = MDS_KBDC_PARSE_ERROR_ERROR;
      (**errors)->error_is_in_file = 0;
      (**errors)->pathname = pathname, pathname = NULL;
      (**errors)->line = 0;
      (**errors)->start = 0;
      (**errors)->end = 0;
      (**errors)->code = NULL;
      (**errors)->description = strdup(strerror(saved_errno));
      fail_if ((**errors)->description == NULL);
      return 0;
    }
  
  /* Read the file and simplify it a bit. */
  fail_if (read_source_lines(pathname, &source_code) < 0);
  /* TODO '\t':s should be expanded into ' ':s. */
  
  fail_if (xmalloc(keyword_stack, source_code.line_count, const char*));
  
  for (line_i = 0, line_n = source_code.line_count; line_i < line_n; line_i++)
    {
      char* line = source_code.lines[line_i];
      char* end;
      char prev_end_char;
      
      while (*line && (*line == ' '))
	line++;
      end = strchrnul(line, ' ');
      if (end == line)
	continue;
      prev_end_char = *end;
      *end = '\0';
      
      if      (!strcmp(line, "information"))  ;
      else if (!strcmp(line, "language"))     ;
      else if (!strcmp(line, "country"))      ;
      else if (!strcmp(line, "variant"))      ;
      else if (!strcmp(line, "include"))      ;
      else if (!strcmp(line, "function"))     ;
      else if (!strcmp(line, "macro"))        ;
      else if (!strcmp(line, "assumption"))   ;
      else if (!strcmp(line, "if"))           ;
      else if (!strcmp(line, "else"))         ;
      else if (!strcmp(line, "for"))          ;
      else if (!strcmp(line, "return"))       ;
      else if (!strcmp(line, "continue"))     ;
      else if (!strcmp(line, "break"))        ;
      else if (!strcmp(line, "let"))          ;
      else if (!strcmp(line, "have"))         ;
      else if (!strcmp(line, "have_chars"))   ;
      else if (!strcmp(line, "have_range"))   ;
      else if (!strcmp(line, "end"))
	{
	  char* original = line;
	  if (stack_ptr == 0)
	    {
	      NEW_ERROR(1, ERROR, "stray ‘end’ statement");
	      goto next;
	    }
	  *end = prev_end_char;
	  stack_ptr--;
	  line += 3;
	  while (*line && (*line == ' '))
	    line++;
	  if (*line == '\0')
	    {
	      line = original;
	      NEW_ERROR(1, ERROR, "expecting a keyword after ‘end’");
	    }
	  else if (!strcmp(line, keyword_stack[stack_ptr]))
	    {
	      NEW_ERROR(1, ERROR, "expected ‘%s’ but got ‘%s’", keyword_stack[stack_ptr], line);
	    }
	}
      else if (strchr("\"<([", *line) || strchr(line, '('))  ;
      else
	{
	  NEW_ERROR(1, ERROR, "invalid keyword ‘%s’", line);
	}
      
    next:
      *end = prev_end_char;
    }
  
  free(pathname);
  free(keyword_stack);
  source_code_destroy(&source_code);
  return 0;
  
 pfail:
  saved_errno = errno;
  free(pathname);
  free(keyword_stack);
  source_code_destroy(&source_code);
  mds_kbdc_parse_error_free_all(old_errors);
  mds_kbdc_parse_error_free_all(*errors), *errors = NULL;
  mds_kbdc_tree_free(*result), *result = NULL;
  return errno = saved_errno, -1;
  
#undef NEW_ERROR
#undef xasprintf
}

