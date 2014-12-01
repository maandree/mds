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
#include "eliminate-dead-code.h"

#include <stdlib.h>
#include <errno.h>



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
#define NEW_ERROR_WITHOUT_INCLUDES(NODE, SEVERITY, ...)			\
  NEW_ERROR_(result, SEVERITY, 1, (NODE)->loc_line,			\
	     (NODE)->loc_start, (NODE)->loc_end, 1, __VA_ARGS__)

/**
 * Add “included from here”-notes
 * 
 * @param  PTR:size_t  The number of “included from here”-notes
 */
#define DUMP_INCLUDE_STACK(PTR)		\
  fail_if (dump_include_stack(PTR))

/**
 * Add an error with “included from here”-notes to the error list
 * 
 * @param  NODE:const mds_kbdc_tree_t*    The node the triggered the error
 * @param  PTR:size_t                     The number of “included from here”-notes
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(NODE, PTR, SEVERITY, ...)			\
  do								\
    {								\
      NEW_ERROR_WITHOUT_INCLUDES(NODE, SEVERITY, __VA_ARGS__);	\
      DUMP_INCLUDE_STACK(PTR);					\
    }								\
  while (0)



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The parameter of `eliminate_dead_code`
 */
static mds_kbdc_parsed_t* restrict result;

/**
 * The original value of `result->pathname`
 */
static char* original_pathname;

/**
 * The original value of `result->source_code`
 */
static mds_kbdc_source_code_t* original_source_code;

/**
 * Stack of visited include-statements
 */
static mds_kbdc_tree_include_t** restrict includes = NULL;

/**
 * The number elements allocated for `includes`
 */
static size_t includes_size = 0;

/**
 * The number elements stored in `includes`
 */
static size_t includes_ptr = 0;



/**
 * Validate that a part of the structure of the compilation unit
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int eliminate_subtree(mds_kbdc_tree_t* restrict tree);



/**
 * Add “included from here”-notes
 * 
 * @param   ptr  The number of “included from here”-notes
 * @return       Zero on success, -1 on error
 */
static int dump_include_stack(size_t ptr)
{
  char* old_pathname = result->pathname;
  mds_kbdc_source_code_t* old_source_code = result->source_code;
  while (ptr--)
    {
      result->pathname = ptr ? includes[ptr - 1]->filename : original_pathname;
      result->source_code = ptr ? includes[ptr - 1]->source_code : original_source_code;
      NEW_ERROR_WITHOUT_INCLUDES(includes[ptr], NOTE, "included from here");
    }
  result->pathname = old_pathname;
  result->source_code = old_source_code;
  return 0;
 pfail:
  result->pathname = old_pathname;
  result->source_code = old_source_code;
  return -1;
} 

  
/**
 * Eliminate dead code in an include-statement
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int eliminate_include(mds_kbdc_tree_include_t* restrict tree)
{
  mds_kbdc_tree_include_t** old;
  char* pathname = result->pathname;
  mds_kbdc_source_code_t* source_code = result->source_code;
  int r, saved_errno;
  if (includes_ptr == includes_size)
    if (xxrealloc(old, includes, includes_size += 4, mds_kbdc_tree_include_t*))
      return saved_errno = errno, free(old), errno = saved_errno, -1;
  includes[includes_ptr++] = tree;
  result->pathname = tree->filename;
  result->source_code = tree->source_code;
  r = eliminate_subtree(tree->inner);
  result->pathname = pathname;
  result->source_code = source_code;
  return includes_ptr--, r;
}


/**
 * Eliminate dead code in a subtree
 * 
 * @param   tree  The tree to reduce
 * @return        Zero on success, -1 on error
 */
static int eliminate_subtree(mds_kbdc_tree_t* restrict tree)
{
#define e(type)  if ((r = eliminate_##type(&(tree->type))))     return r
#define E(type)  if ((r = eliminate_##type(&(tree->type##_))))  return r
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case C(INCLUDE):  e(include);  break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
#undef E
#undef e
}


/**
 * Eliminate and warn about dead code
 * 
 * @param   result_  `result` from `validate_tree`, will be updated
 * @return            -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int eliminate_dead_code(mds_kbdc_parsed_t* restrict result_)
{
  int r, saved_errno;
  result = result_;
  original_pathname = result_->pathname;
  original_source_code = result_->source_code;
  r = eliminate_subtree(result_->tree);
  saved_errno = errno;
  result_->pathname = original_pathname;
  result_->source_code = original_source_code;
  free(includes), includes = NULL;
  includes_size = includes_ptr = 0;
  return errno = saved_errno, r;
}



#undef NEW_ERROR
#undef DUMP_INCLUDE_STACK
#undef NEW_ERROR_WITHOUT_INCLUDES
#undef C

 
