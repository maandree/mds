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
#include "process-includes.h"

#include "make-tree.h"
#include "simplify-tree.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>



/**
 * Tree type constant shortener
 */
#define C(TYPE)  MDS_KBDC_TREE_TYPE_##TYPE

/**
 * Add an error the to error list
 * 
 * @param  NODE:const mds_kbdc_tree_t*    The node the triggered the error
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(NODE, SEVERITY, ...)					\
  NEW_ERROR_(result, SEVERITY, 1, (NODE)->loc_line,			\
	     (NODE)->loc_start, (NODE)->loc_end, 1, __VA_ARGS__)



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The parameter of `process_includes`
 */
static mds_kbdc_parsed_t* restrict result;



/**
 * Transfer errors from an included tree
 * 
 * @param  subresult  The results of the processed include
 * @param  tree       The include statement
 */
static int transfer_errors(mds_kbdc_parsed_t* restrict subresult, mds_kbdc_tree_include_t* restrict tree)
{
  mds_kbdc_parse_error_t** errors = NULL;
  mds_kbdc_parse_error_t* suberror;
  size_t errors_ptr = 0, i;
  int saved_errno;
  
  /* List errors backwards, so that we can easily insert “included from here”-notes. */
  fail_if (xmalloc(errors, subresult->errors_ptr * 2, mds_kbdc_parse_error_t*));
  while (subresult->errors_ptr--)
    {
      suberror = subresult->errors[subresult->errors_ptr];
      if (suberror->severity > MDS_KBDC_PARSE_ERROR_NOTE)
	{
	  NEW_ERROR(tree, NOTE, "included from here");
	  errors[errors_ptr++] = error;
	  result->errors[--(result->errors_ptr)] = NULL;
	}
      errors[errors_ptr++] = suberror;
      subresult->errors[subresult->errors_ptr] = NULL;
    }
  
  /* Append errors. */
  for (i = 0; i < errors_ptr; errors[i++] = NULL)
    {
      if (result->errors_ptr + 1 >= result->errors_size)
	{
	  size_t new_errors_size = result->errors_size ? (result->errors_size << 1) : 2;
	  mds_kbdc_parse_error_t** new_errors = result->errors;
	  
	  fail_if (xrealloc(new_errors, new_errors_size, mds_kbdc_parse_error_t*));
	  result->errors = new_errors;
	  result->errors_size = new_errors_size;
	}
  
      result->errors[result->errors_ptr++] = errors[i];
      result->errors[result->errors_ptr] = NULL;
    }
  
  free(errors);
  return 0;
 pfail:
  saved_errno = errno;
  while (errors_ptr--)
    if (errors[errors_ptr] == NULL)
      break;
    else
      mds_kbdc_parse_error_free(errors[errors_ptr]);
  free(errors);
  return errno = saved_errno, -1;
}


/**
 * Process an include-statement
 * 
 * @param   tree  The include-statement
 * @return        Zero on success, -1 on error
 */
static int process_include(mds_kbdc_tree_include_t* restrict tree)
{
#define process(expr)				\
  fail_if ((expr) < 0);				\
  if (mds_kbdc_parsed_is_fatal(&subresult))	\
    goto stop;
  
  mds_kbdc_parsed_t subresult;
  mds_kbdc_parsed_t* our_result;
  char* dirname = NULL;
  char* cwd = NULL;
  char* old = NULL;
  size_t cwd_size = 4096 >> 1;
  int saved_errno;
  
  /* Initialise result structure for the included file. */
  mds_kbdc_parsed_initialise(&subresult);
  
  /* Get dirname of current file. */
  fail_if ((dirname = strdup(result->pathname)) == NULL);
  *(strrchr(dirname, '/')) = '\0';
  
  /* Get the current working directory. */
  /* glibc offers ways to do this in just one function call,
   * but we will not assume that glibc is used here. */
  for (;;)
    {
      fail_if (!xxrealloc(old, cwd, cwd_size <<= 1, char));
      if (getcwd(cwd, cwd_size))
	break;
      else
	fail_if (errno != ERANGE);
    }
  
  /* Switch working directory. */
  fail_if (chdir(dirname));
  free(dirname), dirname = NULL;
  
  /* Store `result` as it will be switched by the inner `process_includes`. */
  our_result = result;
  
  /* Process include. */
  process (parse_to_tree(tree->filename, &subresult));
  process (simplify_tree(&subresult));
  process (process_includes(&subresult));
 stop:
  
  /* Switch back `result`. */
  result = our_result;
  
  /* Switch back to the old working directory. */
  fail_if (chdir(cwd));
  free(cwd), cwd = NULL;
  
  /* Move over data to our result. */
  free(tree->filename);
  tree->filename = subresult.pathname, subresult.pathname = NULL;
  tree->inner = subresult.tree, subresult.tree = NULL;
  if (result->severest_error_level < subresult.severest_error_level)
    result->severest_error_level = subresult.severest_error_level;
  
  /* Move over errors. */
  fail_if (transfer_errors(&subresult, tree));
  
  /* Release resources. */
  mds_kbdc_parsed_destroy(&subresult);
  
  return 0;
 pfail:
  saved_errno = errno;
  free(dirname);
  free(cwd);
  free(old);
  mds_kbdc_parsed_destroy(&subresult);
  return errno = saved_errno, -1;
#undef process
}


/**
 * Process a subtree
 * 
 * @param   tree  The tree
 * @return        Zero on success, -1 on error
 */
static int process_includes_in_tree(mds_kbdc_tree_t* restrict tree)
{
#define p(expr)  if ((r = process_includes_in_tree(tree->expr)))  return r
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case C(INFORMATION):  p (information.inner);  break;
    case C(FUNCTION):     p (function.inner);     break;
    case C(MACRO):        p (macro.inner);        break;
    case C(ASSUMPTION):   p (assumption.inner);   break;
    case C(FOR):          p (for_.inner);         break;
    case C(IF):           p (if_.inner);          p (if_.otherwise);  break;
    case C(INCLUDE):
      if ((r = process_include(&(tree->include))))
	return r;
      break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
#undef p
}


/**
 * Include included files and process them upto this level
 * 
 * @param   result_  `result` from `simplify_tree`, will be updated
 * @return           -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int process_includes(mds_kbdc_parsed_t* restrict result_)
{
  result = result_;
  return process_includes_in_tree(result_->tree);
}



#undef NEW_ERROR
#undef C

