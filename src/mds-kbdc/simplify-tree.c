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
#include "simplify-tree.h"

#include <libmdsserver/macros.h>


/**
 * Wrapper around `asprintf` that makes sure that first
 * argument gets set to `NULL` on error and that zero is
 * returned on success rather than the number of printed
 * characters
 * 
 * @param   VAR:char**            The output parameter for the string
 * @param   ...:const char*, ...  The format string and arguments
 * @return  :int                  Zero on success, -1 on error
 */
#define xasprintf(VAR, ...)					\
  (asprintf(&(VAR), __VA_ARGS__) < 0 ? (VAR = NULL, -1) : 0)


/**
 * Pointer to the beginning of the current line
 */
#define LINE			\
  (source_code.lines[line_i])


/**
 * Add an error the to error list
 * 
 * @param  ERROR_IS_IN_FILE:int  Whether the error is in the layout code
 * @param  SEVERITY:identifier   * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...  Error description format string and arguments
 */
#define NEW_ERROR(ERROR_IS_IN_FILE, SEVERITY, ...)						\
  do												\
    {												\
      if (errors_ptr + 1 >= errors_size)							\
	{											\
	  errors_size = errors_size ? (errors_size << 1) : 2;					\
	  fail_if (xxrealloc(old_errors, *errors, errors_size, mds_kbdc_parse_error_t*));	\
	  old_errors = NULL;									\
	}											\
      fail_if (xcalloc(error, 1, mds_kbdc_parse_error_t));					\
      (*errors)[errors_ptr + 0] = error;							\
      (*errors)[errors_ptr + 1] = NULL;								\
      errors_ptr++;										\
      error->line  = line_i;									\
      error->severity = MDS_KBDC_PARSE_ERROR_##SEVERITY;					\
      error->error_is_in_file = ERROR_IS_IN_FILE;						\
      error->start = (size_t)(line - LINE);							\
      error->end   = (size_t)(end  - LINE);							\
      fail_if ((error->pathname = strdup(pathname)) == NULL);					\
      if (ERROR_IS_IN_FILE)									\
	fail_if ((error->code = strdup(source_code.real_lines[line_i])) == NULL);		\
      fail_if (xasprintf(error->description, __VA_ARGS__));					\
    }												\
   while (0)



/**
 * Temporary storage variable for the new error,
 * it is made available so that it is easy to
 * make adjustments to the error after calling
 * `NEW_ERROR`
 */
static mds_kbdc_parse_error_t* error = NULL;

/**
 * The number of elements allocated for `*errors`
 */
static size_t errors_size = 0;

/**
 * The number of elements stored in `*errors`
 */
static size_t errors_ptr = 0;

/**
 * Pointer to the list of errors
 */
static mds_kbdc_parse_error_t*** errors;

/**
 * The old `*errors` used temporary when reallocating `*errors`
 */
static mds_kbdc_parse_error_t** old_errors = NULL;



/**
 * Simplify a subtree
 * 
 * @param   tree  The tree
 * @return        Zero on success, -1 on error
 */
static int simplify(mds_kbdc_tree_t* restrict tree)
{
#define s(expr)  if ((r = simplify(tree->expr)))  return r;
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case MDS_KBDC_TREE_TYPE_INFORMATION:  s (information.inner);  break;
    case MDS_KBDC_TREE_TYPE_FUNCTION:     s (function.inner);     break;
    case MDS_KBDC_TREE_TYPE_MACRO:        s (macro.inner);        break;
    case MDS_KBDC_TREE_TYPE_ASSUMPTION:   s (assumption.inner);   break;
    case MDS_KBDC_TREE_TYPE_FOR:          s (for_.inner);         break;
    case MDS_KBDC_TREE_TYPE_IF:
      s (if_.inner);
      s (if_.otherwise);
      break;
      
    case MDS_KBDC_TREE_TYPE_MAP:
      break;
      
    case MDS_KBDC_TREE_TYPE_ALTERNATION:
      break;
      
    case MDS_KBDC_TREE_TYPE_UNORDERED:
      break;
      
    case MDS_KBDC_TREE_TYPE_MACRO_CALL:
      break;
      
    default:
      break;
    }
    
  tree = tree->next;
  goto again;
#undef s
}


/**
 * Simplify a tree and generate related warnings in the process
 * 
 * @param   tree     The tree, it may be modified
 * @param   errors_  `NULL`-terminated list of found error, `NULL` if no errors were found or if -1 is returned
 * @return           -1 if an error occursed that cannot be stored in `*errors`, zero otherwise
 */
int simplify_tree(mds_kbdc_tree_t* restrict tree, mds_kbdc_parse_error_t*** restrict errors_)
{
  int r, saved_errno;
  
  errors = errors_;
  *errors = NULL;
  
  if (r = simplify(tree), !r)
    return 0;
  
  saved_errno = errno;
  mds_kbdc_parse_error_free_all(old_errors);
  mds_kbdc_parse_error_free_all(*errors), *errors = NULL;
  errno = saved_errno;
  return r;
}



#undef NEW_ERROR
#undef LINE
#undef xasprintf

