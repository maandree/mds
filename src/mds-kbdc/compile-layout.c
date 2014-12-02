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
#include "compile-layout.h"

#include "include-stack.h"



/**
 * Tree type constant shortener
 */
#define C(TYPE)  MDS_KBDC_TREE_TYPE_##TYPE

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
 * The parameter of `compile_layout`
 */
static mds_kbdc_parsed_t* restrict result;



/**
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree);



/**
 * Compile an include-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_include(mds_kbdc_tree_include_t* restrict tree)
{
  void* data;
  int r;
  if (mds_kbdc_include_stack_push(tree, &data))
    return -1;
  r = compile_subtree(tree->inner);
  mds_kbdc_include_stack_pop(data);
  return r;
}


/**
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree)
{
#define t(expr)   if (r = (expr), r < 0)  return r
#define c(type)   t (compile_##type(&(tree->type)))
#define c_(type)  t (compile_##type(&(tree->type##_)))
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case C(INFORMATION):
      t (compile_subtree(tree->information.inner));
      break;
    case C(INFORMATION_LANGUAGE): break;
    case C(INFORMATION_COUNTRY): break;
    case C(INFORMATION_VARIANT): break;
    case C(INCLUDE):
      c(include);
      break;
    case C(FUNCTION): break;
    case C(MACRO): break;
    case C(ASSUMPTION):
      t ((includes_ptr == 0) && compile_subtree(tree->assumption.inner));
      break;
    case C(ASSUMPTION_HAVE): break;
    case C(ASSUMPTION_HAVE_CHARS): break;
    case C(ASSUMPTION_HAVE_RANGE): break;
    case C(FOR): break;
    case C(IF): break;
    case C(LET): break;
    case C(MAP): break;
    case C(MACRO_CALL): break;
    case C(RETURN): break;
    case C(BREAK): break;
    case C(CONTINUE): break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
 pfail:
  return -1;
#undef c_
#undef c
#undef t
}


/**
 * Compile the layout code
 * 
 * @param   result_  `result` from `eliminate_dead_code`, will be updated
 * @return           -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int compile_layout(mds_kbdc_parsed_t* restrict result_)
{
  int r;
  mds_kbdc_include_stack_begin(result = result_);
  r = compile_subtree(result_->tree);
  return mds_kbdc_include_stack_end(), r;
}



#undef NEW_ERROR
#undef C

