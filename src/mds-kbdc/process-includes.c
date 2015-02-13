/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#include <sys/types.h>
#include <sys/stat.h>



/**
 * Tree type constant shortener
 */
#define C(TYPE)  MDS_KBDC_TREE_TYPE_##TYPE

/**
 * Add an error to the error list
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
 * Stack of attributes of already included files
 */
static struct stat* restrict included;

/**
 * The number elements allocated for `included`
 */
static size_t included_size = 0;

/**
 * The number elements stored in `included`
 */
static size_t included_ptr = 0;



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
  size_t errors_ptr = 0;
  int saved_errno, annotated = 0;
  
  /* Allocate temporary list for errors. */
  if (subresult->errors_ptr == 0)
    return 0;
  fail_if (xmalloc(errors, subresult->errors_ptr * 2, mds_kbdc_parse_error_t*));
  
  /* List errors backwards, so that we can easily handle errors and add “included from here”-note. */
  while (subresult->errors_ptr--)
    {
      suberror = subresult->errors[subresult->errors_ptr];
      
      /* If it is more severe than a note, we want to say there it was included. */
      if (annotated == 0)
	{
	  NEW_ERROR(tree, NOTE, "included from here");
	  errors[errors_ptr++] = error;
	  result->errors[--(result->errors_ptr)] = NULL;
	  annotated = 1;
	}
      
      /* Include error. */
      errors[errors_ptr++] = suberror;
      subresult->errors[subresult->errors_ptr] = NULL;
      
      /* Make sure when there are nested inclusions that the outermost inclusion * is annotated last. */
      if (suberror->severity > MDS_KBDC_PARSE_ERROR_NOTE)
	annotated = 0;
    }
  
  /* Append errors. */
  for (; errors_ptr--; errors[errors_ptr] = NULL)
    {
      if (result->errors_ptr + 1 >= result->errors_size)
	{
	  size_t new_errors_size = result->errors_size ? (result->errors_size << 1) : 2;
	  mds_kbdc_parse_error_t** new_errors = result->errors;
	  
	  fail_if (xrealloc(new_errors, new_errors_size, mds_kbdc_parse_error_t*));
	  result->errors = new_errors;
	  result->errors_size = new_errors_size;
	}
      
      result->errors[result->errors_ptr++] = errors[errors_ptr];
      result->errors[result->errors_ptr] = NULL;
    }
  
  free(errors);
  return 0;
 fail:
  saved_errno = errno;
  while (errors_ptr--)
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
  fail_if (xstrdup(dirname, result->pathname));
  *(strrchr(dirname, '/')) = '\0';
  
  /* Get the current working directory. */
  /* glibc offers ways to do this in just one function call,
   * but we will not assume that glibc is used here. */
  for (;;)
    {
      fail_if (xxrealloc(old, cwd, cwd_size <<= 1, char));
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
  old = tree->filename, tree->filename = NULL;
  tree->filename = parse_raw_string(old);
  fail_if (tree->filename == NULL);
  free(old), old = NULL;
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
  tree->source_code = subresult.source_code, subresult.source_code = NULL;
  tree->inner = subresult.tree, subresult.tree = NULL;
  if (result->severest_error_level < subresult.severest_error_level)
    result->severest_error_level = subresult.severest_error_level;
  
  /* Move over errors. */
  fail_if (transfer_errors(&subresult, tree));
  
  /* Release resources. */
  mds_kbdc_parsed_destroy(&subresult);
  
  return 0;
 fail:
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
#define p(expr)  fail_if (process_includes_in_tree(tree->expr))
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
      fail_if (process_include(&(tree->include)));
      break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
 fail:
  return -1;
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
  int r, saved_errno;
  struct stat attr;
  size_t i;
  
  result = result_;
  
  fail_if (stat(result->pathname, &attr));
  
  if (included_ptr == included_size)
    {
      struct stat* old;
      if (xxrealloc(old, included, included_size += 4, struct stat))
	fail_if (included = old, 1);
    }
  
  for (i = 0; i < included_ptr; i++)
    if ((included[i].st_dev == attr.st_dev) && (included[i].st_ino == attr.st_ino))
      {
	NEW_ERROR_(result, ERROR, 0, 0, 0, 0, 1, "resursive inclusion");
	return 0;
      }
  
  included[included_ptr++] = attr;
  
  r = process_includes_in_tree(result_->tree);
  saved_errno = errno;
  
  if (--included_ptr == 0)
    free(included), included_size = 0;
  
  return errno = saved_errno, r;
 fail:
  return -1;
}



#undef NEW_ERROR
#undef C

