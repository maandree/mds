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

#include <libmdsserver/macros.h>
#undef xfree

#include <stdlib.h>
#include <string.h>
#include <errno.h>



/* Helper `typedef`:s */
typedef struct mds_kbdc_tree_nesting mds_kbdc_tree_nesting_t;
typedef struct mds_kbdc_tree_callable mds_kbdc_tree_callable_t;
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_data_t;



/**
 * Tree type constant shortener
 */
#define C(t)  MDS_KBDC_TREE_TYPE_##t


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
  mds_kbdc_tree_t* this;
  fail_if (xmalloc(this, 1, mds_kbdc_tree_t));
  mds_kbdc_tree_initialise(this, type);
  return this;
 fail:
  return NULL;
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
    case C(INFORMATION):
    case C(ASSUMPTION):
    case C(ALTERNATION):
    case C(UNORDERED):
    case C(ORDERED):
      xdestroy(mds_kbdc_tree_nesting_t*, inner);
      break;
      
    case C(INFORMATION_LANGUAGE):
    case C(INFORMATION_COUNTRY):
    case C(INFORMATION_VARIANT):
      xfree(mds_kbdc_tree_information_data_t*, data);
      break;
      
    case C(FUNCTION):
    case C(MACRO):
      xfree(mds_kbdc_tree_callable_t*, name);
      xdestroy(mds_kbdc_tree_callable_t*, inner);
      break;
      
    case C(INCLUDE):
      xfree(mds_kbdc_tree_include_t*, filename);
      xdestroy(mds_kbdc_tree_include_t*, inner);
      mds_kbdc_source_code_free(this->include.source_code);
      break;
      
    case C(ASSUMPTION_HAVE):
      xdestroy(mds_kbdc_tree_assumption_have_t*, data);
      break;
      
    case C(ASSUMPTION_HAVE_CHARS):
      xfree(mds_kbdc_tree_assumption_have_chars_t*, chars);
      break;
      
    case C(ASSUMPTION_HAVE_RANGE):
      xfree(mds_kbdc_tree_assumption_have_range_t*, first);
      xfree(mds_kbdc_tree_assumption_have_range_t*, last);
      break;
      
    case C(FOR):
      xfree(mds_kbdc_tree_for_t*, first);
      xfree(mds_kbdc_tree_for_t*, last);
      xfree(mds_kbdc_tree_for_t*, variable);
      xdestroy(mds_kbdc_tree_for_t*, inner);
      break;
      
    case C(IF):
      xfree(mds_kbdc_tree_if_t*, condition);
      xdestroy(mds_kbdc_tree_if_t*, inner);
      xdestroy(mds_kbdc_tree_if_t*, otherwise);
      break;
      
    case C(LET):
      xfree(mds_kbdc_tree_let_t*, variable);
      xdestroy(mds_kbdc_tree_let_t*, value);
      break;
      
    case C(MAP):
      xdestroy(mds_kbdc_tree_map_t*, sequence);
      xdestroy(mds_kbdc_tree_map_t*, result);
      break;
      
    case C(ARRAY):
      xdestroy(mds_kbdc_tree_array_t*, elements);
      break;
      
    case C(KEYS):
    case C(STRING):
    case C(COMPILED_KEYS):
    case C(COMPILED_STRING):
      xfree(mds_kbdc_tree_keys_t*, keys);
      /* We are abusing the similaries of the structures. */
      break;
      
    case C(MACRO_CALL):
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
 * Duplicate a subtree and goto `pfail` on failure
 * 
 * @param  member:identifer  The member in the tree to duplicate
 */
#define T(member)  fail_if (t->member && (n->member = mds_kbdc_tree_dup(t->member), n->member == NULL))


/**
 * Duplicate a string and goto `pfail` on failure
 * 
 * @param  member:identifer  The member in the tree to duplicate
 */
#define S(member)  fail_if (t->member && (n->member = strdup(t->member), n->member == NULL))


/**
 * Duplicate an UTF-32-string and goto `pfail` on failure
 * 
 * @param  member:identifer  The member in the tree to duplicate
 */
#define Z(member)  fail_if (t->member && (n->member = string_dup(t->member), n->member == NULL))


/**
 * Duplicate a source code structure and goto `pfail` on failure
 * 
 * @param  member:identifer  The member in the tree to copied
 */
#define R(member)  fail_if (t->member && (n->member = mds_kbdc_source_code_dup(t->member), n->member == NULL))



/**
 * Create a duplicate of a tree node and its children
 * 
 * @param   this  The tree node
 * @return        A duplicate of `this`, `NULL` on error
 */
mds_kbdc_tree_t* mds_kbdc_tree_dup(const mds_kbdc_tree_t* restrict this)
{
#define  t  this
#define  n  (*node)
  mds_kbdc_tree_t* rc = NULL;
  mds_kbdc_tree_t** node = &rc;
  int saved_errno;
  
 again:
  if (t == NULL)  return rc;
  fail_if (xcalloc(n, 1, mds_kbdc_tree_t));
  
  n->type      = t->type;
  n->loc_line  = t->loc_line;
  n->loc_start = t->loc_start;
  n->loc_end   = t->loc_end;
  n->processed = t->processed;
  
  switch (this->type)
    {
    case C(INFORMATION):
    case C(ASSUMPTION):
    case C(ALTERNATION):
    case C(UNORDERED):
    case C(ORDERED):                T(ordered.inner);                                               break;
    case C(FUNCTION):
    case C(MACRO):                  S(macro.name); T(macro.inner);                                  break;
    case C(ASSUMPTION_HAVE):        T(have.data);                                                   break;
    case C(ARRAY):                  T(array.elements);                                              break;
    case C(LET):                    S(let.variable); T(let.value);                                  break;
    case C(MACRO_CALL):             S(macro_call.name); T(macro_call.arguments);                    break;
    case C(INFORMATION_LANGUAGE):
    case C(INFORMATION_COUNTRY):
    case C(INFORMATION_VARIANT):    S(variant.data);                                                break;
    case C(INCLUDE):                S(include.filename); T(include.inner); R(include.source_code);  break;
    case C(ASSUMPTION_HAVE_CHARS):  S(have_chars.chars);                                            break;
    case C(KEYS):                   S(keys.keys);                                                   break;
    case C(STRING):                 S(string.string);                                               break;
    case C(COMPILED_KEYS):          Z(compiled_keys.keys);                                          break;
    case C(COMPILED_STRING):        Z(compiled_string.string);                                      break;
    case C(ASSUMPTION_HAVE_RANGE):  S(have_range.first); S(have_range.last);                        break;
    case C(FOR):                    S(for_.first); S(for_.last); S(for_.variable); T(for_.inner);   break;
    case C(IF):                     S(if_.condition); T(if_.inner); T(if_.otherwise);               break;
    case C(MAP):                    T(map.sequence); T(map.result);                                 break;
    default:
      break;
    }
  
  t = t->next;
  node = &(n->next);
  goto again;
  
 fail:
  saved_errno = errno;
  mds_kbdc_tree_free(rc);
  return errno = saved_errno, NULL;
#undef n
#undef t
}


#undef R
#undef Z
#undef S
#undef T



/**
 * Convert the tree to a specialised subtype and
 * prints its type and code location
 * 
 * @param  LOWERCASE:identifer   The name of subtype
 * @param  NOTATION:const char*  The notation for the subtype
 */
#define NODE(LOWERCASE, NOTATION)					\
  const mds_kbdc_tree_##LOWERCASE##_t* node;				\
  node = (const mds_kbdc_tree_##LOWERCASE##_t*)this;			\
  fprintf(output, "%*.s(\033[01m%s\033[00m", indent, "", NOTATION);	\
  fprintf(output, " \033[36m(@ %zu %zu-%zu)\033[00m",			\
	  node->loc_line + 1, node->loc_start, node->loc_end)


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
#define SIMPLE(MEMBER)		\
  STRING(MEMBER);		\
  fprintf(output, ")\n")


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
	  this->loc_line + 1, this->loc_start, this->loc_end);		\
  fprintf(output, ")\n");						\
  break



/**
 * Print a tree into a file
 * 
 * @param  this    The tree node
 * @param  output  The output file
 * @param  indent  The indent
 */
static void mds_kbdc_tree_print_indented(const mds_kbdc_tree_t* restrict this, FILE* output, int indent)
{
 again:
  if (this == NULL)
    return;
  
  switch (this->type)
    {
      /* These have their break built into their macro. */
    case C(INFORMATION):            NESTING(information, "information", inner);
    case C(INFORMATION_LANGUAGE):   SIMPLEX(information_language, "language", data);
    case C(INFORMATION_COUNTRY):    SIMPLEX(information_country, "country", data);
    case C(INFORMATION_VARIANT):    SIMPLEX(information_variant, "variant", data);
    case C(INCLUDE):                NAMED_NESTING(include, "include", filename, inner);
    case C(FUNCTION):               NAMED_NESTING(function, "function", name, inner);
    case C(MACRO):                  NAMED_NESTING(macro, "macro", name, inner);
    case C(ASSUMPTION):             NESTING(assumption, "assumption", inner);
    case C(ASSUMPTION_HAVE):        NESTING(assumption_have, "have", data);
    case C(ASSUMPTION_HAVE_CHARS):  SIMPLEX(assumption_have_chars, "have_chars", chars);
    case C(ASSUMPTION_HAVE_RANGE):  DUPLEX(assumption_have_range, "have_range", first, last);
    case C(LET):                    NAMED_NESTING(let, "let", variable, value);
    case C(ARRAY):                  NESTING(array, "array", elements);
    case C(KEYS):                   SIMPLEX(keys, "keys", keys);
    case C(STRING):                 SIMPLEX(string, "string", string);
    case C(NOTHING):                NOTHING("nothing");
    case C(ALTERNATION):            NESTING(alternation, "alternation", inner);
    case C(UNORDERED):              NESTING(unordered, "unordered", inner);
    case C(ORDERED):                NESTING(ordered, "ordered", inner);
    case C(MACRO_CALL):             NAMED_NESTING(macro_call, "macro_call", name, arguments);
    case C(RETURN):                 NOTHING("return");
    case C(BREAK):                  NOTHING("break");
    case C(CONTINUE):               NOTHING("continue");
      
    case C(COMPILED_KEYS):
      {
	NODE(compiled_keys, "compiled_keys");
	if (node->keys)
	  fprintf(output, " ‘\033[32m%s\033[00m’", string_encode(node->keys));
	else
	  fprintf(output, " \033[35mnil\033[00m");
	fprintf(output, ")\n");
      }
      break;
      
    case C(COMPILED_STRING):
      {
	NODE(compiled_string, "compiled_string");
	if (node->string)
	  fprintf(output, " ‘\033[32m%s\033[00m’", string_encode(node->string));
	else
	  fprintf(output, " \033[35mnil\033[00m");
	fprintf(output, ")\n");
      }
      break;
      
    case C(FOR):
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
      
    case C(IF):
      {
	NODE(if, "if");
	STRING(condition);
	BRANCH(inner);
	BRANCH(otherwise);
	COMPLEX;
      }
      break;
      
    case C(MAP):
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
void mds_kbdc_tree_print(const mds_kbdc_tree_t* restrict this, FILE* output)
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

#undef C

