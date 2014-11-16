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
#include <string.h>



/**
 * Initialise a tree node
 * 
 * @param  this  The memory slot for the tree node
 * @param  type  The type of the node
 */
void mds_kbdc_tree_initialise(mds_kbdc_tree_t* restrict this, int type)
{
  memset(this, 0, sizeof(mds_kbdc_tree_t));
  this->type = type;
}


/**
 * Create a tree node
 * 
 * @param   type  The type of the node
 * @return        The tree node, `NULL` on error
 */
mds_kbdc_tree_t* mds_kbdc_tree_create(int type)
{
  mds_kbdc_tree_t* this = malloc(sizeof(mds_kbdc_tree_t));
  if (this == NULL)
    return NULL;
  mds_kbdc_tree_initialise(this, type);
  return this;
}


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
  
  mds_kbdc_tree_t* prev = NULL;
  mds_kbdc_tree_t* first = this;
  
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
      xdestroy(mds_kbdc_tree_assumption_have_t*, data);
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
  
  prev = this;
  this = this->next;
  if (prev != first)
    free(prev);
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



/**
 * Convert the tree to a specialised subtype and
 * prints its type and code location
 * 
 * @param  LOWERCASE:identifer   The name of subtype
 * @param  NOTATION:const char*  The notation for the subtype
 */
#define NODE(LOWERCASE, NOTATION)					\
  mds_kbdc_tree_##LOWERCASE##_t* node;					\
  node = (mds_kbdc_tree_##LOWERCASE##_t*)this;				\
  fprintf(output, "%*.s(\033[01m%s\033[00m", indent, "", NOTATION);	\
  fprintf(output, " \033[36m(@ %zu %zu-%zu)\033[00m",			\
	  node->loc_line, node->loc_start, node->loc_end)


/**
 * Print a member for `node` which is a subtree
 * 
 * @param  MEMBER:identifier  The tree structure's member
 */
#define BRANCH(MEMBER)								\
  if (node->MEMBER)								\
    {										\
      fprintf(output, "\n%*.s(.%s\n", indent + 2, "", #MEMBER);			\
      mds_kbdc_tree_print_indented(node->MEMBER, output, indent + 4);		\
      fprintf(output, "%*.s)", indent + 2, "");					\
    }										\
  else										\
    fprintf(output, "\n%*.s(.%s \033[35mnil\033[00m)", indent + 2, "", #MEMBER)


/**
 * End a tree which has at least one member that is a subtree
 */
#define COMPLEX					\
  fprintf(output, "\n%*.s)\n", indent, "")


/**
 * Print a member for `node` which is a string
 * 
 * @param  MEMBER:identifier  The tree structure's member
 */
#define STRING(MEMBER)						\
  if (node->MEMBER)						\
    fprintf(output, " ‘\033[32m%s\033[00m’", node->MEMBER);	\
  else								\
    fprintf(output, " \033[35mnil\033[00m")


/**
 * Print a member for `node` which is a string,
 * and end the tree
 * 
 * @param  MEMBER:identifier  The tree structure's member
 */
#define SIMPLE(MEMBER)				\
  STRING(MEMBER);				\
  fprintf(output, ")\n", node->MEMBER)


/**
 * Print a tree which has only one member,
 * and whose member is a string
 * 
 * @param  LOWERCASE:identifier  See `NODE`
 * @param  NOTATION:const char*  See `NODE`
 * @param  MEMBER:identifier     See `STRING`
 */
#define SIMPLEX(LOWERCASE, NOTATION, MEMBER)	\
  {						\
    NODE(LOWERCASE, NOTATION);			\
    SIMPLE(MEMBER);				\
  }						\
  break


/**
 * Print a tree which has exactly two members,
 * and whose members is are strings
 * 
 * @param  LOWERCASE:identifier  See `NODE`
 * @param  NOTATION:const char*  See `NODE`
 * @param  FIRST:identifier      See `STRING`, the first member
 * @param  LAST:identifier       See `STRING`, the second member
 */
#define DUPLEX(LOWERCASE, NOTATION, FIRST, LAST)	\
  {							\
    NODE(LOWERCASE, NOTATION);				\
    STRING(FIRST);					\
    SIMPLE(LAST);					\
  }							\
  break


/**
 * Print a tree which has exactly one member,
 * and whose members is a subtree
 * 
 * @param  LOWERCASE:identifier  See `NODE`
 * @param  NOTATION:const char*  See `NODE`
 * @param  MEMBER:identifier     See `BRANCH`
 */
#define NESTING(LOWERCASE, NOTATION, MEMBER)	\
  {						\
    NODE(LOWERCASE, NOTATION);			\
    BRANCH(MEMBER);				\
    COMPLEX;					\
  }						\
  break


/**
 * Print a tree which has exactly two members,
 * and whose first member is a string and second
 * member is a subtree
 * 
 * @param  LOWERCASE:identifier  See `NODE`
 * @param  NOTATION:const char*  See `NODE`
 * @param  NAMER:identifier      See `STRING`
 * @param  MEMBER:identifier     See `BRANCH`
 */
#define NAMED_NESTING(LOWERCASE, NOTATION, NAMER, MEMBER)	\
  {								\
    NODE(LOWERCASE, NOTATION);					\
    STRING(NAMER);						\
    BRANCH(MEMBER);						\
    COMPLEX;							\
  }								\
  break


/**
 * Print a tree which has no members
 * 
 * @param  NOTATION:const char*  See `NODE`
 */
#define NOTHING(NOTATION)						\
  fprintf(output, "%*.s(\033[01m%s\033[00m", indent, "", NOTATION);	\
  fprintf(output, " \033[36m(@ %zu %zu-%zu)\033[00m",			\
	  this->loc_line, this->loc_start, this->loc_end);		\
  fprintf(output, ")\n");						\
  break


/**
 * Print a tree into a file
 * 
 * @param  this    The tree node
 * @param  output  The output file
 * @param  indent  The indent
 */
static void mds_kbdc_tree_print_indented(mds_kbdc_tree_t* restrict this, FILE* output, int indent)
{
 again:
  if (this == NULL)
    return;
  
  switch (this->type)
    {
      /* These have their break built into their macro. */
    case MDS_KBDC_TREE_TYPE_INFORMATION:            NESTING(information, "information", inner);
    case MDS_KBDC_TREE_TYPE_INFORMATION_LANGUAGE:   SIMPLEX(information_language, "language", data);
    case MDS_KBDC_TREE_TYPE_INFORMATION_COUNTRY:    SIMPLEX(information_country, "country", data);
    case MDS_KBDC_TREE_TYPE_INFORMATION_VARIANT:    SIMPLEX(information_variant, "variant", data);
    case MDS_KBDC_TREE_TYPE_INCLUDE:                SIMPLEX(include, "include", filename);
    case MDS_KBDC_TREE_TYPE_FUNCTION:               NAMED_NESTING(function, "function", name, inner);
    case MDS_KBDC_TREE_TYPE_MACRO:                  NAMED_NESTING(macro, "macro", name, inner);
    case MDS_KBDC_TREE_TYPE_ASSUMPTION:             NESTING(assumption, "assumption", inner);
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE:        NESTING(assumption_have, "have", data);
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_CHARS:  SIMPLEX(assumption_have_chars, "have_chars", chars);
    case MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_RANGE:  DUPLEX(assumption_have_range, "have_range", first, last);
    case MDS_KBDC_TREE_TYPE_LET:                    NAMED_NESTING(let, "let", variable, value);
    case MDS_KBDC_TREE_TYPE_ARRAY:                  NESTING(array, "array", elements);
    case MDS_KBDC_TREE_TYPE_KEYS:                   SIMPLEX(keys, "keys", keys);
    case MDS_KBDC_TREE_TYPE_STRING:                 SIMPLEX(string, "string", string);
    case MDS_KBDC_TREE_TYPE_NOTHING:                NOTHING("nothing");
    case MDS_KBDC_TREE_TYPE_ALTERNATION:            NESTING(alternation, "alternation", inner);
    case MDS_KBDC_TREE_TYPE_UNORDERED:              NESTING(unordered, "unordered", inner);
    case MDS_KBDC_TREE_TYPE_MACRO_CALL:             NAMED_NESTING(macro_call, "macro_call", name, arguments);
    case MDS_KBDC_TREE_TYPE_RETURN:                 NOTHING("return");
    case MDS_KBDC_TREE_TYPE_BREAK:                  NOTHING("break");
    case MDS_KBDC_TREE_TYPE_CONTINUE:               NOTHING("continue");
      
    case MDS_KBDC_TREE_TYPE_FOR:
      {
	NODE(for, "for");
	STRING(first);
	STRING(last);
	fprintf(output, " (.variable");
	STRING(variable);
	fprintf(output, ")");
	BRANCH(inner);
	COMPLEX;
      }
      break;
      
    case MDS_KBDC_TREE_TYPE_IF:
      {
	NODE(if, "if");
	STRING(condition);
	BRANCH(inner);
	BRANCH(otherwise);
	COMPLEX;
      }
      break;
      
    case MDS_KBDC_TREE_TYPE_MAP:
      {
	NODE(map, "map");
	BRANCH(sequence);
	BRANCH(result);
	COMPLEX;
      }
      break;
      
    default:
      abort();
      break;
    }
  
  this = this->next;
  goto again;
}


/**
 * Print a tree into a file
 * 
 * @param  this    The tree node
 * @param  output  The output file
 */
void mds_kbdc_tree_print(mds_kbdc_tree_t* restrict this, FILE* output)
{
  mds_kbdc_tree_print_indented(this, output, 0);
}


#undef NOTHING
#undef NAMED_NESTING
#undef NESTING
#undef DUPLEX
#undef SIMPLEX
#undef SIMPLE
#undef STRING
#undef COMPLEX
#undef BRANCH
#undef NODE

