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
#include "validate-tree.h"

#include "include-stack.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>



/**
 * Tree type constant shortener
 */
#define C(TYPE)  MDS_KBDC_TREE_TYPE_##TYPE

/**
 * Check the value of `innermost_visit`
 */
#define VISITING(TYPE)  (innermost_visit == MDS_KBDC_TREE_TYPE_##TYPE)

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
  NEW_ERROR_WITH_INCLUDES(NODE, PTR, SEVERITY, __VA_ARGS__)



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The parameter of `validate_tree`
 */
static mds_kbdc_parsed_t* restrict result;

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
 * The type of the currently innermost visited node
 * of `C(FUNCTION)`, `C(MACRO)`, `C(INFORMATION)` and
 * `C(ASSUMPTION)`, -1 if none
 */
static int innermost_visit = -1;



/**
 * Validate that a part of the structure of the compilation unit
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_subtree(mds_kbdc_tree_t* restrict tree);



/**
 * Validate an include-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_include(mds_kbdc_tree_include_t* restrict tree)
{
  void* data;
  int r;
  fail_if (mds_kbdc_include_stack_push(tree, &data));
  r = validate_subtree(tree->inner);
  mds_kbdc_include_stack_pop(data);
  fail_if (r);
  return 0;
 fail:
  return -1;
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
  if (VISITING(FUNCTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function defined here");
      return 0;
    }
  else if (VISITING(MACRO))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "function definition inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (VISITING(INFORMATION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "function definition inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  innermost_visit = tree->type;
  function = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return function = NULL, r;
 fail:
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
  if (VISITING(FUNCTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "macro definition inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (VISITING(MACRO))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (VISITING(INFORMATION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "macro definition inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  innermost_visit = tree->type;
  macro = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return macro = NULL, r;
 fail:
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
  if (VISITING(FUNCTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (VISITING(MACRO))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (VISITING(INFORMATION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (VISITING(ASSUMPTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "information clause inside assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  innermost_visit = tree->type;
  information = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return information = NULL, r;
 fail:
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
  if (VISITING(FUNCTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside function definition");
      NEW_ERROR(function, def_includes_ptr, NOTE, "outer function definition defined here");
      return 0;
    }
  else if (VISITING(MACRO))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside macro definition");
      NEW_ERROR(macro, def_includes_ptr, NOTE, "outer macro defined here");
      return 0;
    }
  else if (VISITING(INFORMATION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "assumption clause inside information clause");
      NEW_ERROR(information, def_includes_ptr, NOTE, "outer information clause defined here");
      return 0;
    }
  else if (VISITING(ASSUMPTION))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "nested assumption clause");
      NEW_ERROR(assumption, def_includes_ptr, NOTE, "outer assumption clause defined here");
      return 0;
    }
  innermost_visit = tree->type;
  assumption = tree;
  def_includes_ptr = includes_ptr;
  r = validate_subtree(tree->inner);
  return assumption = NULL, r;
 fail:
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
  else if (VISITING(INFORMATION))
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside information clause");
  else if (VISITING(ASSUMPTION))
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside assumption clause");
  else if (VISITING(FUNCTION))
    NEW_ERROR(tree, includes_ptr, ERROR, "mapping-statement inside function definition");
  return 0;
 fail:
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
  if (VISITING(INFORMATION))
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside information clause");
  else if (VISITING(ASSUMPTION))
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside assumption clause");
  else if (VISITING(FUNCTION))
    NEW_ERROR(tree, includes_ptr, ERROR, "macro call inside function definition");
  return 0;
 fail:
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
  fors++, r = validate_subtree(tree->inner), fors--;
  fail_if (r);
  return 0;
 fail:
  return -1;
}


/**
 * Validate a if-statement
 * 
 * @param   tree  The tree to validate
 * @return        Zero on success, -1 on error
 */
static int validate_if(mds_kbdc_tree_if_t* restrict tree)
{
  fail_if ((validate_subtree(tree->inner) || validate_subtree(tree->otherwise)));
  return 0;
 fail:
  return -1;
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
 fail:
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
 fail:
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
 fail:
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
 fail:
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
 fail:
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
#define v(type)  fail_if (validate_##type(&(tree->type)))
#define V(type)  fail_if (validate_##type(&(tree->type##_)))
  int old_innermost_visit = innermost_visit;
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
      fail_if (validate_information_data(tree));
      break;
    case C(ASSUMPTION_HAVE):
    case C(ASSUMPTION_HAVE_CHARS):
    case C(ASSUMPTION_HAVE_RANGE):
      fail_if (validate_assumption_data(tree));
      break;
    default:
      break;
    }
  
  innermost_visit = old_innermost_visit;
  tree = tree->next;
  goto again;
 fail:
  return innermost_visit = old_innermost_visit, -1;
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
  int r;
  mds_kbdc_include_stack_begin(result = result_);
  r = validate_subtree(result_->tree);
  fors = 0;
  mds_kbdc_include_stack_end();
  fail_if (r);
  return 0;
 fail:
  return -1;
}



#undef NEW_ERROR
#undef VISITING
#undef C

