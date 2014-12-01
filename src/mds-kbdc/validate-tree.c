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
#include "validate-tree.h"

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
 * The parameter of `validate_tree`
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
 * The number visited for-statements
 */
static size_t fors = 0;

/**
 * The function definition that is currently being visited
 */
static mds_kbdc_tree_function_t* function = NULL;

/**
 * The macro definition that is currently being visited
 */
static mds_kbdc_tree_macro_t* macro = NULL;

/**
 * The information clause that is currently being visited
 */
static mds_kbdc_tree_information_t* information = NULL;

/**
 * The assumption clause that is currently being visited
 */
static mds_kbdc_tree_assumption_t* assumption = NULL;

/**
 * The value `includes_ptr` had when `function`,
 * `macro`, `information` or `assumption` was set
 */
static size_t def_includes_ptr = 0;



/**
 * Validate that a part of the structure of the compilation unit
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_subtree(mds_kbdc_tree_t* restrict tree);



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
 * Validate an include-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_include(mds_kbdc_tree_include_t* restrict tree)
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
  r = validate_subtree(tree->inner);
  result->pathname = pathname;
  result->source_code = source_code;
  return includes_ptr--, r;
}


/**
 * Validate a function definition
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_function(mds_kbdc_tree_function_t* restrict tree)
{
  int r;
  if (function)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function defined here");
      return 0;
    }
  else if (macro)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "function definition inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (information)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "function definition inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (assumption)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "function definition inside assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  function = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return function = NULL, r;
 pfail:
  return -1;
}


/**
 * Validate a macro definition
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_macro(mds_kbdc_tree_macro_t* restrict tree)
{
  int r;
  if (function)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "macro definition inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (macro)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (information)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "macro definition inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (assumption)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "macro definition inside assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  macro = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return macro = NULL, r;
 pfail:
  return -1;
}


/**
 * Validate an information clause
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_information(mds_kbdc_tree_information_t* restrict tree)
{
  int r;
  if (function)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (macro)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (information)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (assumption)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  information = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return information = NULL, r;
 pfail:
  return -1;
}


/**
 * Validate an assumption clause
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_assumption(mds_kbdc_tree_assumption_t* restrict tree)
{
  int r;
  if (function)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (macro)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (information)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (assumption)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  assumption = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return assumption = NULL, r;
 pfail:
  return -1;
}


/**
 * Validate a mapping- or value-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_map(mds_kbdc_tree_map_t* restrict tree)
{
  int is_value = tree->result == NULL;
  if (is_value);
    /* We do not want value-statments outside function
     * definitions, however, we do want \set/3 to be usable,
     * from anywhere, even indirectly, therefore we cannot,
     * at this process level, determine whether a
     * value-statement is used correctly or not.
     */
  else if (information)
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside information clause");
  else if (assumption)
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside assumption clause");
  else if (function)
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside function definition");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate a macro call
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  if (information)
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside information clause");
  else if (assumption)
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside assumption clause");
  else if (function)
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside function definition");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate a for-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_for(mds_kbdc_tree_for_t* restrict tree)
{
  int r;
  fors++;
  r = validate_subtree(tree->inner);
  return fors--, r;
}


/**
 * Validate a if-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_if(mds_kbdc_tree_if_t* restrict tree)
{
  return -(validate_subtree(tree->inner) ||
	   validate_subtree(tree->otherwise));
}


/**
 * Validate a return-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_return(mds_kbdc_tree_return_t* restrict tree)
{
  if ((function == NULL) && (macro == NULL))
    NEW_ERROR(tree, includes_ptr, ERROR, "‘return’ outside function and macro definition");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate a break-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_break(mds_kbdc_tree_break_t* restrict tree)
{
  if (fors == 0)
    NEW_ERROR(tree, includes_ptr, ERROR, "‘break’ outside ‘for’");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate a continue-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_continue(mds_kbdc_tree_continue_t* restrict tree)
{
  if (fors == 0)
    NEW_ERROR(tree, includes_ptr, ERROR, "‘continue’ outside ‘for’");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate an assumption-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_assumption_data(mds_kbdc_tree_t* restrict tree)
{
  if (assumption == NULL)
    NEW_ERROR(tree, includes_ptr, ERROR, "assumption outside assumption clause");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate an information-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_information_data(mds_kbdc_tree_t* restrict tree)
{
  if (information == NULL)
    NEW_ERROR(tree, includes_ptr, ERROR, "information outside information clause");
  return 0;
 pfail:
  return -1;
}


/**
 * Validate that a part of the structure of the compilation unit
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_subtree(mds_kbdc_tree_t* restrict tree)
{
#define v(type)  if ((r = validate_##type(&(tree->type))))     return r
#define V(type)  if ((r = validate_##type(&(tree->type##_))))  return r
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case C(INFORMATION):           v(information);  break;
    case C(INCLUDE):               v(include);      break;
    case C(FUNCTION):              v(function);     break;
    case C(MACRO):                 v(macro);        break;
    case C(ASSUMPTION):            v(assumption);   break;
    case C(FOR):                   V(for);          break;
    case C(IF):                    V(if);           break;
    case C(MAP):                   v(map);          break;
    case C(MACRO_CALL):            v(macro_call);   break;
    case C(RETURN):                V(return);       break;
    case C(BREAK):                 V(break);        break;
    case C(CONTINUE):              V(continue);     break;
    case C(INFORMATION_LANGUAGE):
    case C(INFORMATION_COUNTRY):
    case C(INFORMATION_VARIANT):
      if ((r = validate_information_data(tree)))
	return r;
      break;
    case C(ASSUMPTION_HAVE):
    case C(ASSUMPTION_HAVE_CHARS):
    case C(ASSUMPTION_HAVE_RANGE):
      if ((r = validate_assumption_data(tree)))
	return r;
      break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
#undef V
#undef v
}


/**
 * Validate that the structure of the compilation unit
 * 
 * @param   result_  `result` from `process_includes`, will be updated
 * @return           -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int validate_tree(mds_kbdc_parsed_t* restrict result_)
{
  int r, saved_errno;
  result = result_;
  original_pathname = result_->pathname;
  original_source_code = result_->source_code;
  r = validate_subtree(result_->tree);
  saved_errno = errno;
  result_->pathname = original_pathname;
  result_->source_code = original_source_code;
  free(includes), includes = NULL;
  includes_size = includes_ptr = 0;
  fors = 0;
  return errno = saved_errno, r;
}



#undef NEW_ERROR
#undef DUMP_INCLUDE_STACK
#undef NEW_ERROR_WITHOUT_INCLUDES
#undef C

