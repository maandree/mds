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

#include <stdlib.h>
#include <string.h>



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
 * The parameter of `simplify_tree`
 */
static mds_kbdc_parsed_t* restrict result;



/**
 * Simplify a subtree
 * 
 * @param   tree  The tree
 * @return        Zero on success, -1 on error
 */
static int simplify(mds_kbdc_tree_t* restrict tree);


/**
 * Simplify a macro call-subtree
 * 
 * @param   tree  The macro call-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  mds_kbdc_tree_t* argument = tree->arguments;
  mds_kbdc_tree_t** here;
  
  /* Simplify arguments. */
  for (argument = tree->arguments; argument; argument = argument->next)
    simplify(argument);
  
  /* Remove ‘.’:s. */
  for (here = &(tree->arguments); *here; here = &((*here)->next))
    while (*here && (*here)->type == MDS_KBDC_TREE_TYPE_NOTHING)
      {
	mds_kbdc_tree_t* temp = (*here)->next;
	(*here)->next = NULL;
	NEW_ERROR(*here, WARNING, "‘.’ outside alternation has no effect");
	mds_kbdc_tree_free(*here);
	*here = temp;
      }
  
  return 0;
 pfail:
  return -1;
}


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
      /* TODO */
      break;
      
    case MDS_KBDC_TREE_TYPE_ALTERNATION:
      /* TODO find alternations inside alternations */
      break;
      
    case MDS_KBDC_TREE_TYPE_UNORDERED:
      /* TODO find unordered and nothing inside unordered */
      break;
      
    case MDS_KBDC_TREE_TYPE_MACRO_CALL:
      if ((r = simplify_macro_call(&(tree->macro_call))))
	return r;
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
 * @param   result_  `result` from `parse_to_tree`, same sematics, will be updated
 * @return           -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int simplify_tree(mds_kbdc_parsed_t* restrict result_)
{
  result = result_;
  return simplify(result_->tree);
}



#undef NEW_ERROR

