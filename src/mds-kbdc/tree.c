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
#include "tree.h"

#include <stdlib.h>


/**
 * Common procedure for `mds_kbdc_tree_destroy` and `mds_kbdc_tree_destroy_nonrecursive`
 * 
 * @param  this       The tree node
 * @param  recursive  Whether subtree should be destroyed and freed
 */
static void mds_kbdc_tree_destroy_(mds_kbdc_tree_t* restrict this, int recursive)
{
#define V(type, var)    (((type)this)->var)
#define xfree(t, v)     (free(V(t, v)), V(t, v) = NULL)
#define xdestroy(t, v)  (recursive ? (mds_kbdc_tree_destroy_(V(t, v), 1), xfree(t, v)) : (V(t, v) = NULL))
  
 again:
  if (this == NULL)
    return;
  
  switch (this->type)
    {
    case MDS_KBDC_TREE_TYPE_INFORMATION:
    case MDS_KBDC_TREE_TYPE_ASSUMPTION:
    case MDS_KBDC_TREE_TYPE_ALTERNATION:
    case MDS_KBDC_TREE_TYPE_UNORDERED:
      xdestroy(struct mds_kbdc_tree_nesting*, inner);
      break;
      
    case MDS_KBDC_TREE_TYPE_INFORMATION_LANGUAGE:
    case MDS_KBDC_TREE_TYPE_INFORMATION_COUNTRY:
    case MDS_KBDC_TREE_TYPE_INFORMATION_VARIANT:
      xfree(struct mds_kbdc_tree_information_data*, data);
      break;
      
    case MDS_KBDC_TREE_TYPE_FUNCTION:
    case MDS_KBDC_TREE_TYPE_MACRO:
      xfree(struct mds_kbdc_tree_callable*, name);
      xdestroy(struct mds_kbdc_tree_callable*, inner);
      break;
      
    case MDS_KBDC_TREE_TYPE_INCLUDE:
      xfree(mds_kbdc_tree_include_t*, filename);
      break;
      
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE:
      xfree(mds_kbdc_tree_assumption_have_t*, data);
      break;
      
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_CHARS:
      xfree(mds_kbdc_tree_assumption_have_chars_t*, chars);
      break;
      
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_RANGE:
      xfree(mds_kbdc_tree_assumption_have_range_t*, first);
      xfree(mds_kbdc_tree_assumption_have_range_t*, last);
      break;
      
    case MDS_KBDC_TREE_TYPE_FOR:
      xfree(mds_kbdc_tree_for_t*, first);
      xfree(mds_kbdc_tree_for_t*, last);
      xfree(mds_kbdc_tree_for_t*, variable);
      xdestroy(mds_kbdc_tree_for_t*, inner);
      break;
      
    case MDS_KBDC_TREE_TYPE_IF:
      xfree(mds_kbdc_tree_if_t*, condition);
      xdestroy(mds_kbdc_tree_if_t*, inner);
      xdestroy(mds_kbdc_tree_if_t*, otherwise);
      break;
      
    case MDS_KBDC_TREE_TYPE_LET:
      xfree(mds_kbdc_tree_let_t*, variable);
      xdestroy(mds_kbdc_tree_let_t*, value);
      break;
      
    case MDS_KBDC_TREE_TYPE_MAP:
      xdestroy(mds_kbdc_tree_map_t*, sequence);
      xdestroy(mds_kbdc_tree_map_t*, result);
      break;
      
    case MDS_KBDC_TREE_TYPE_ARRAY:
      xdestroy(mds_kbdc_tree_array_t*, elements);
      break;
      
    case MDS_KBDC_TREE_TYPE_KEYS:
      xfree(mds_kbdc_tree_keys_t*, keys);
      break;
      
    case MDS_KBDC_TREE_TYPE_STRING:
      xfree(mds_kbdc_tree_string_t*, string);
      break;
      
    case MDS_KBDC_TREE_TYPE_MACRO_CALL:
      xfree(mds_kbdc_tree_macro_call_t*, name);
      xdestroy(mds_kbdc_tree_macro_call_t*, arguments);
      break;
      
    default:
      break;
    }
  
  this = this->next;
  goto again;
  
#undef xdestroy
#undef xfree
#undef V
}



/**
 * Release all resources stored in a tree node,
 * without freeing the node itself or freeing
 * or destroying inner `mds_kbdc_tree_t*`:s
 * 
 * @param  this  The tree node
 */
void mds_kbdc_tree_destroy_nonrecursive(mds_kbdc_tree_t* restrict this)
{
  mds_kbdc_tree_destroy_(this, 0);
}


/**
 * Release all resources stored in a tree node,
 * without freeing or destroying inner
 * `mds_kbdc_tree_t*`:s, but free this node's
 * allocation
 * 
 * @param  this  The tree node
 */
void mds_kbdc_tree_free_nonrecursive(mds_kbdc_tree_t* restrict this)
{
  mds_kbdc_tree_destroy_nonrecursive(this);
  free(this);
}


/**
 * Release all resources stored in a tree node
 * recursively, but do not free the allocation
 * of the tree node
 * 
 * @param  this  The tree node
 */
void mds_kbdc_tree_destroy(mds_kbdc_tree_t* restrict this)
{
  mds_kbdc_tree_destroy_(this, 1);
}


/**
 * Release all resources stored in a tree node
 * recursively, and free the allocation
 * of the tree node
 * 
 * @param  this  The tree node
 */
void mds_kbdc_tree_free(mds_kbdc_tree_t* restrict this)
{
  mds_kbdc_tree_destroy(this);
  free(this);
}

