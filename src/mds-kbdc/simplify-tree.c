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
#include "globals.h"

#include <stdlib.h>
#include <string.h>
#include <alloca.h>



/**
 * This processes value for `mds_kbdc_tree_t.processed`
 */
#define PROCESS_LEVEL  2


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
 * Simplify an unordered subsequence-subtree
 * 
 * @param   tree  The unordered subsequence-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_unordered(mds_kbdc_tree_unordered_t* restrict tree);



/**
 * Simplify a macro call-subtree
 * 
 * @param   tree  The macro call-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t* alternative;
  mds_kbdc_tree_t* next_statement;
  mds_kbdc_tree_t* next_alternative;
  mds_kbdc_tree_t* first;
  mds_kbdc_tree_t* last;
  mds_kbdc_tree_t* new_tree;
  mds_kbdc_tree_t* new_argument;
  mds_kbdc_tree_t* dup_argument;
  mds_kbdc_tree_t** here;
  size_t i, argument_index = 0;
  long processed;
  
  /* Simplify arguments. */
  for (argument = tree->arguments; argument; argument = argument->next)
    simplify(argument);
  
  /* Remove ‘.’:s. */
  processed = tree->processed, tree->processed = PROCESS_LEVEL;
  for (here = &(tree->arguments); *here;)
    if ((*here)->type != C(NOTHING))
      here = &((*here)->next);
    else
      while (*here && (*here)->type == C(NOTHING))
	{
	  argument = (*here)->next, (*here)->next = NULL;
	  if ((processed != PROCESS_LEVEL) && ((*here)->processed != PROCESS_LEVEL))
	    NEW_ERROR(*here, WARNING, "‘.’ outside alternation has no effect");
	  mds_kbdc_tree_free(*here);
	  *here = argument;
	}
  
  /* Copy arguments. */
  if (tree->arguments == NULL)
    return 0;
  if (dup_argument = mds_kbdc_tree_dup(tree->arguments), dup_argument == NULL)
    return -1;
  
  /* Eliminate alterations. */
  for (argument = dup_argument; argument; argument = argument->next, argument_index++)
    {
      if (argument->type != C(ALTERNATION))
	continue;
      
      /* Detach next statement, we do not want to duplicate all following statements. */
      next_statement = tree->next, tree->next = NULL;
      /* Detach alternation, we replace it in all duplcates,
	 no need to duplicate all alternatives. */
      alternative = argument->alternation.inner, argument->alternation.inner = NULL;
      /* Eliminate. */
      for (first = last = NULL; alternative; alternative = next_alternative)
	{
	  /* Duplicate statement. */
	  if (new_tree = mds_kbdc_tree_dup((mds_kbdc_tree_t*)tree), new_tree == NULL)
	    {
	      int saved_errno = errno;
	      argument->alternation.inner = alternative;
	      tree->next = next_statement;
	      mds_kbdc_tree_free(dup_argument);
	      return errno = saved_errno, -1;
	    }
	  /* Join trees. */
	  if (last)
	    last->next = new_tree;
	  last = new_tree;
	  first = first ? first : new_tree;
	  /* Jump to the alternation. */
	  here = &(new_tree->macro_call.arguments);
	  for (new_argument = *here, i = 0; i < argument_index; i++, here = &((*here)->next))
	    new_argument = new_argument->next;
	  /* Detach alternative. */
	  next_alternative = alternative->next;
	  /* Right-join alternative. */
	  alternative->next = new_argument->next, new_argument->next = NULL;
	  mds_kbdc_tree_free(new_argument);
	  /* Left-join alternative. */
	  *here = alternative;
	}
      /* Replace the statement with the first generated statement without the alternation. */
      mds_kbdc_tree_destroy((mds_kbdc_tree_t*)tree);
      memcpy(tree, first, sizeof(mds_kbdc_tree_t));
      if (first == last)  last = (mds_kbdc_tree_t*)tree;
      free(first);
      /* Reattach the statement that followed to the last generated statement. */
      last->next = next_statement;
    }
  
  mds_kbdc_tree_free(dup_argument);
  
  /* Example of what will happend:
   * 
   *   my_macro([1 2] [1 2] [1 2]) ## call 1
   * 
   *   simplify_macro_call on call 1
   *     after processing argument 1
   *       my_macro(1 [1 2] [1 2]) ## call 1
   *       my_macro(2 [1 2] [1 2]) ## call 5
   *     after processing argument 2
   *       my_macro(1 1 [1 2]) ## call 1
   *       my_macro(1 2 [1 2]) ## call 3
   *       my_macro(2 [1 2] [1 2]) ## call 5
   *     after processing argument 3
   *       my_macro(1 1 1) ## call 1
   *       my_macro(1 1 2) ## call 2
   *       my_macro(1 2 [1 2]) ## call 3
   *       my_macro(2 [1 2] [1 2]) ## call 5
   * 
   *   no difference after simplify_macro_call on call 2
   * 
   *   simplify_macro_call on call 3
   *     no difference after processing argument 1
   *     no difference after processing argument 2
   *     after processing argument 3
   *       my_macro(1 1 1) ## (call 1)
   *       my_macro(1 1 2) ## (call 2)
   *       my_macro(1 2 1) ## call 3
   *       my_macro(1 2 1) ## call 4
   *       my_macro(2 [1 2] [1 2]) ## call 5
   * 
   *   no difference after simplify_macro_call on call 4
   * 
   *   simplify_macro_call on call 5
   *     no difference after processing argument 1
   *     after processing argument 2
   *       my_macro(1 1 1) ## (call 1)
   *       my_macro(1 1 2) ## (call 2)
   *       my_macro(1 2 1) ## (call 3)
   *       my_macro(1 2 2) ## (call 4)
   *       my_macro(2 1 [1 2]) ## call 5
   *       my_macro(2 2 [1 2]) ## call 7
   *     after processing argument 3
   *       my_macro(1 1 1) ## (call 1)
   *       my_macro(1 1 2) ## (call 2)
   *       my_macro(1 2 1) ## (call 3)
   *       my_macro(1 2 2) ## (call 4)
   *       my_macro(2 1 1) ## call 5
   *       my_macro(2 1 2) ## call 6
   *       my_macro(2 2 [1 2]) ## call 7
   * 
   *   no difference after simplify_macro_call on call 6
   * 
   *   simplify_macro_call on call 7
   *     no difference after processing argument 1
   *     no difference after processing argument 2
   *     after processing argument 3
   *       my_macro(1 1 1) ## (call 1)
   *       my_macro(1 1 2) ## (call 2)
   *       my_macro(1 2 1) ## (call 3)
   *       my_macro(1 2 2) ## (call 4)
   *       my_macro(2 1 1) ## (call 5)
   *       my_macro(2 1 2) ## (call 6)
   *       my_macro(2 2 1) ## call 7
   *       my_macro(2 2 2) ## call 8
   * 
   *   no difference after simplify_macro_call on call 8
   * 
   * Nothings (‘.’) are removed before processing the alternations.
   */
  
  return 0;
 pfail:
  return -1;
}


/**
 * Simplify an alternation-subtree
 * 
 * @param   tree  The alternation-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_alternation(mds_kbdc_tree_alternation_t* restrict tree)
{
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t* eliminated_argument;
  mds_kbdc_tree_t* first_nothing = NULL;
  mds_kbdc_tree_t* temp;
  mds_kbdc_tree_t** here;
  int redo = 0;
  
  /* Test emptyness. */
  if (tree->inner == NULL)
    {
      NEW_ERROR(tree, ERROR, "empty alternation");
      tree->type = C(NOTHING);
      tree->processed = PROCESS_LEVEL;
      return 0;
    }
  
  /* Test singletonness. */
  if (tree->inner->next == NULL)
    {
      temp = tree->inner;
      NEW_ERROR(tree, WARNING, "singleton alternation");
      memcpy(tree, temp, sizeof(mds_kbdc_tree_t));
      free(temp);
      return simplify((mds_kbdc_tree_t*)tree);
    }
  
  /* Simplify. */
  for (here = &(tree->inner); (argument = *here); redo ? (redo = 0) : (here = &(argument->next), 0))
    if ((argument->type == C(NOTHING)) && (argument->processed != PROCESS_LEVEL))
      {
	/* Test multiple nothings. */
	if (first_nothing == NULL)
	  first_nothing = argument;
	else
	  {
	    NEW_ERROR(argument, WARNING, "multiple ‘.’ inside an alternation");
	    NEW_ERROR(first_nothing, NOTE, "first ‘.’ was here");
	  }
      }
    else if (argument->type == C(ALTERNATION))
      {
	/* Alternation nesting. */
	if (argument->processed != PROCESS_LEVEL)
	  NEW_ERROR(argument, WARNING, "alternation inside alternation is unnessary");
	if (simplify_alternation(&(argument->alternation)))
	  return -1;
	if (argument->type == C(ALTERNATION))
	  {
	    /* Remember the alternation and the argument that follows it. */
	    eliminated_argument = argument;
	    temp = argument->next;
	    /* Find the last alternative. */
	    for (argument->next = argument->alternation.inner; argument->next;)
	      argument = argument->next;
	    /* Attach the argument that was after the alternation to the end of the alternation,
	       that is, flatten the right side. */
	    argument->next = temp;
	    /* Flatten the left side.  */
	    *here = eliminated_argument->next;
	    /* Free the memory of the alternation. */
	    eliminated_argument->alternation.inner = NULL;
	    eliminated_argument->next = NULL;
	    mds_kbdc_tree_free(eliminated_argument);
	  }
	redo = 1;
      }
    else if (argument->type == C(UNORDERED))
      {
	/* Nesting unordered subsequence,
	   simplifies to alternation of ordered subsequence, or simpler. */
	NEW_ERROR(argument, WARNING, "unordered subsequence inside alternation is discouraged");
	if (simplify_unordered(&(argument->unordered)))
	  return -1;
	redo = 1;
      }
  
  return 0;
 pfail:
  return -1;
}


/**
 * Create a chain of ordered subsequence covering all
 * permutations of a set of subtrees
 * 
 * @param   elements  The subtrees, chained
 * @return            Chain of ordered subsequence, `NULL` on error
 */
static mds_kbdc_tree_t* create_permutations(mds_kbdc_tree_t* elements)
{
  mds_kbdc_tree_t* first = NULL;
  mds_kbdc_tree_t** here = &first;
  mds_kbdc_tree_t** previous_next = &elements;
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t* temp;
  mds_kbdc_tree_t* subperms = NULL;
  mds_kbdc_tree_t* perm;
  mds_kbdc_tree_t ordered;
  int saved_errno, no_perms;
  
  for (previous_next = &elements; (argument = *previous_next); previous_next = &((*previous_next)->next))
    {
      /* Created ordered alternative for a permutation prototype. */
      mds_kbdc_tree_initialise(&ordered, C(ORDERED));
      /* Select the first element. */
      temp = argument->next, argument->next = NULL;
      ordered.ordered.inner = mds_kbdc_tree_dup(argument);
      argument->next = temp;
      fail_if (ordered.ordered.inner == NULL);
      /* Create subpermutations. */
      *previous_next = argument->next;
      argument->next = NULL;
      no_perms = (elements == NULL);
      subperms = create_permutations(elements);
      argument->next = *previous_next;
      *previous_next = argument;
      fail_if (no_perms ? 0 : subperms == NULL);
      /* Join first element with subpermutations. */
      while (subperms)
	{
	  /* Join. */
	  fail_if ((perm = mds_kbdc_tree_dup(&ordered), perm == NULL));
	  perm->ordered.inner = subperms->ordered.inner;
	  /* Add the permutation to the chain. */
	  *here = perm;
	  here = &(perm->next);
	  /* Select next permutation. */
	  temp = subperms;
	  subperms = subperms->next;
	  temp->next = NULL;
	  mds_kbdc_tree_free(temp);
	}
      /* Destroy prototype. */
      mds_kbdc_tree_destroy(&ordered);
    }
  
  return first;
  
 pfail:
  saved_errno = errno;
  mds_kbdc_tree_free(first);
  mds_kbdc_tree_free(subperms);
  mds_kbdc_tree_destroy(&ordered);
  errno = saved_errno;
  return NULL;
}


/**
 * Simplify an unordered subsequence-subtree
 * 
 * @param   tree  The unordered subsequence-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_unordered(mds_kbdc_tree_unordered_t* restrict tree)
{
  mds_kbdc_tree_t* arguments;
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t* temp;
  mds_kbdc_tree_t** here;
  int allow_long = 0;
  size_t argument_count;
  int argv_force = 0; /* TODO globals.h */
  
  /* Test for ‘(( ))’. */
  if (tree->inner && (tree->inner->next == NULL) && (tree->inner->type == C(UNORDERED)))
    {
      temp = tree->inner;
      tree->inner = tree->inner->unordered.inner;
      temp->unordered.inner = NULL;
      mds_kbdc_tree_free(temp);
      allow_long = 1;
    }
  
  /* Test emptyness. */
  if (tree->inner == NULL)
    {
      NEW_ERROR(tree, ERROR, "empty unordered subsequence");
      tree->type = C(NOTHING);
      tree->processed = PROCESS_LEVEL;
      return 0;
    }
  
  /* Test singletonness. */
  if (tree->inner->next == NULL)
    {
      temp = tree->inner;
      NEW_ERROR(tree, WARNING, "singleton unordered subsequence");
      memcpy(tree, temp, sizeof(mds_kbdc_tree_t));
      free(temp);
      return simplify((mds_kbdc_tree_t*)tree);
    }
  
  /* Remove ‘.’:s. */
  for (here = &(tree->inner); *here;)
    if ((*here)->type != C(NOTHING))
      here = &((*here)->next);
    else
      while (*here && (*here)->type == C(NOTHING))
	{
	  argument = (*here)->next, (*here)->next = NULL;
	  NEW_ERROR(*here, WARNING, "‘.’ inside unordered subsequences has no effect");
	  mds_kbdc_tree_free(*here);
	  *here = argument;
	}
  
  /* Simplify. */
  for (argument = tree->inner, argument_count = 0; argument; argument = argument->next, argument_count++)
    if (argument->type == C(ALTERNATION))
      {
	if (simplify_alternation(&(argument->alternation)))
	  return -1;
	argument->processed = PROCESS_LEVEL;
      }
    else if (argument->type == C(UNORDERED))
      {
	NEW_ERROR(argument, WARNING, "unordered subsequence inside unordered subsequence is discouraged");
	if (simplify_unordered(&(argument->unordered)))
	  return -1;
	argument->processed = PROCESS_LEVEL;
      }
  
  /* Check the size of the subsequence. */
  if ((argument_count > 5) && (allow_long * argv_force == 0))
    {
      if (allow_long == 0)
	NEW_ERROR(argument, ERROR, "unordered subsequence longer than 5 elements need double brackets");
      else if (argv_force == 0)
	NEW_ERROR(argument, ERROR, "unordered subsequence of size %zu found, requires ‘--force’ to compile",
		  argument_count);
      return 0;
    }
  
  /* Generate permutations. */
  tree->type = C(ALTERNATION);
  tree->processed = PROCESS_LEVEL;
  arguments = tree->inner;
  if (tree->inner = create_permutations(arguments), tree->inner == NULL)
    return tree->inner = arguments, -1;
  mds_kbdc_tree_free(arguments);
  
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
#define s(expr)  if ((r = simplify(tree->expr)))  return r
#define S(type)  if ((r = simplify_##type(&(tree->type))))  return r
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  switch (tree->type)
    {
    case C(INFORMATION):  s (information.inner);  break;
    case C(FUNCTION):     s (function.inner);     break;
    case C(MACRO):        s (macro.inner);        break;
    case C(ASSUMPTION):   s (assumption.inner);   break;
    case C(FOR):          s (for_.inner);         break;
    case C(IF):           s (if_.inner);          s (if_.otherwise);  break;
    case C(MAP):          /* TODO */              break;
    case C(ALTERNATION):  S (alternation);        break;
    case C(UNORDERED):    S (unordered);          break;
    case C(MACRO_CALL):   S (macro_call);         break;
    default:
      break;
    }
    
  tree = tree->next;
  goto again;
#undef s
#undef S
}


/**
 * Simplify a tree and generate related warnings and errors in the process
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
#undef C
#undef PROCESS_LEVEL
