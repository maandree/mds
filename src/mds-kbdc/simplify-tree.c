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
#include "simplify-tree.h"
#include "globals.h"

#include <stdlib.h>
#include <string.h>
#include <alloca.h>



/**
 * This process's value for `mds_kbdc_tree_t.processed`
 */
#define PROCESS_LEVEL  2


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
 * Remove ‘.’:s
 * 
 * @param  START:identifier           The member of `tree` that is cleaned from ‘.’:s
 * @scope  tree:mds_kbdc_tree_t*      The tree from where the ‘.’:s are being removed
 * @scope  here:mds_kbdc_tree_t**     Help variable that must be available for use
 * @scope  argument:mds_kbdc_tree_t*  Help variable that must be available for use
 */
#define REMOVE_NOTHING(START)									\
  do												\
    {												\
      long processed = tree->processed;								\
      tree->processed = PROCESS_LEVEL;								\
      for (here = &(tree->START); *here;)							\
	if ((*here)->type != C(NOTHING))							\
	  here = &((*here)->next);								\
	else											\
	  while (*here && (*here)->type == C(NOTHING))						\
	    {											\
	      argument = (*here)->next, (*here)->next = NULL;					\
	      if ((processed != PROCESS_LEVEL) && ((*here)->processed != PROCESS_LEVEL))	\
		NEW_ERROR(*here, WARNING, "‘.’ outside alternation has no effect");		\
	      mds_kbdc_tree_free(*here);							\
	      *here = argument;									\
	    }											\
    }												\
  while (0)



/**
 * Flatten an alternation of orderered subsequence, that is,
 * insert its interior in place of it and move its next
 * sibling to the next of the interior
 * 
 * @param  argument:mds_kbdc_tree_t*  The argument to flatten
 * @scope  here:mds_kbdc_tree_t**     Pointer to the space where the argument was found
 * @scope  temp:mds_kbdc_tree_t*      Help variable that must be available for use
 */
#define FLATTEN(argument)								\
  do											\
    {											\
      /* Remember the alternation/subsequence and the argument that follows it. */	\
      mds_kbdc_tree_t* eliminated_argument = argument;					\
      temp = argument->next;								\
      /* Find the last alternative/element. */						\
      for (argument->next = argument->ordered.inner; argument->next;)			\
	argument = argument->next;							\
      /* Attach the argument that was after the alternation/subsequence to the */	\
      /* end of the alternation/subsequence, that is, flatten the right side. */	\
      argument->next = temp;								\
      /* Flatten the left side. */							\
      *here = eliminated_argument->next;						\
      /* Free the memory of the alternation/subsequence. */				\
      eliminated_argument->ordered.inner = NULL;					\
      eliminated_argument->next = NULL;							\
      mds_kbdc_tree_free(eliminated_argument);						\
    }											\
  while (0)



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
 * Eliminiate an alternation
 * 
 * @param   tree            The statement where the alternation is found
 * @param   argument        The argument to eliminate
 * @param   argument_index  The index of the argument to eliminate
 * @return                  Zero on sucess, -1 on error
 */
static int eliminate_alternation(mds_kbdc_tree_t* tree, mds_kbdc_tree_t* argument, size_t argument_index)
{
  mds_kbdc_tree_t** here;
  mds_kbdc_tree_t* first;
  mds_kbdc_tree_t* last;
  mds_kbdc_tree_t* new_tree;
  mds_kbdc_tree_t* alternative;
  mds_kbdc_tree_t* next_statement;
  mds_kbdc_tree_t* next_alternative;
  mds_kbdc_tree_t* new_argument;
  size_t i;
  int saved_errno;
  
  /* Detach next statement, we do not want to duplicate all following statements. */
  next_statement = tree->next, tree->next = NULL;
  /* Detach alternation, we replace it in all duplcates,
     no need to duplicate all alternatives. */
  alternative = argument->alternation.inner, argument->alternation.inner = NULL;
  /* Eliminate. */
  for (first = last = NULL; alternative; alternative = next_alternative)
    {
      /* Duplicate statement. */
      fail_if (new_tree = mds_kbdc_tree_dup(tree), new_tree == NULL);
      /* Join trees. */
      if (last)
	last->next = new_tree;
      last = new_tree;
      first = first ? first : new_tree;
      /* Jump to the alternation. */
      here = &(new_tree->macro_call.arguments);  /* `new_tree->macro_call.arguments` and
						  * `new_tree->map.sequence` as the same address. */
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
  return 0;
 fail:
  saved_errno = errno;
  argument->alternation.inner = alternative;
  tree->next = next_statement;
  return errno = saved_errno, -1;
}


/**
 * Simplify a macro call-subtree
 * 
 * @param   tree  The macro call-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t* dup_arguments = NULL;
  mds_kbdc_tree_t** here;
  char* full_macro_name;
  size_t argument_index = 0;
  int saved_errno;
  
  /* Simplify arguments. */
  for (argument = tree->arguments; argument; argument = argument->next)
    fail_if (simplify(argument));
  
  /* Remove ‘.’:s. */
  REMOVE_NOTHING(arguments);
  
  /* Copy arguments. */
  if (tree->arguments == NULL)
    goto no_args;
  fail_if (dup_arguments = mds_kbdc_tree_dup(tree->arguments), dup_arguments == NULL);
  
  /* Eliminate alterations. */
  for (argument = dup_arguments; argument; argument = argument->next, argument_index++)
    if (argument->type == C(ALTERNATION))
      fail_if (eliminate_alternation((mds_kbdc_tree_t*)tree, argument, argument_index));
  mds_kbdc_tree_free(dup_arguments), dup_arguments = NULL;
  
  /* Add argument count suffix. */
 no_args:
  for (argument_index = 0, argument = tree->arguments; argument; argument = argument->next)
    argument_index++;
  fail_if (xasprintf(full_macro_name, "%s/%zu", tree->name, argument_index));
  free(tree->name), tree->name = full_macro_name;
  
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
   * 
   * It should also be noticed that all macro names are update to
   * with the argument count suffix.
   */
  
  return 0;
 fail:
  saved_errno = errno;
  mds_kbdc_tree_free(dup_arguments);
  return errno = saved_errno, -1;
}


/**
 * Check for bad things in a value statement for before the simplification process
 * 
 * @param   tree  The value statement-tree
 * @return        Zero on success, -1 on error
 */
static int check_value_statement_before_simplification(mds_kbdc_tree_map_t* restrict tree)
{
 again:
  /* Check for alternation. */
  if ((tree->sequence->type == C(ALTERNATION)) && (tree->processed != PROCESS_LEVEL))
    NEW_ERROR(tree->sequence, WARNING,
	      "alternated value statement is undefined unless the alternatives are identical");
  
  /* Check for unordered. */
  if (tree->sequence->type != C(UNORDERED))
    return 0;
  if (tree->processed != PROCESS_LEVEL)
    NEW_ERROR(tree->sequence, WARNING, "use of sequence in value statement is discouraged");
  
  /* Simplify argument and start over. */
  fail_if (simplify(tree->sequence));
  goto again;
  
 fail:
  return -1;
}


/**
 * Check for bad things in a value statement for after the simplification process
 * 
 * @param   tree  The value statement-tree
 * @return        Zero on success, -1 on error
 */
static int check_value_statement_after_simplification(mds_kbdc_tree_map_t* restrict tree)
{
  /* Check that there is only one value. */
  if (tree->sequence->next)
    NEW_ERROR(tree->sequence->next, ERROR, "more the one value in value statement");
  
  /* Check the type of the value */
  if (tree->sequence->type != C(STRING))
    NEW_ERROR(tree->sequence, ERROR, "bad value type");
  
  return 0;
 fail:
  return -1;
}
  

/**
 * Simplify a mapping-subtree
 * 
 * @param   tree  The mapping-subtree
 * @return        Zero on success, -1 on error
 */
static int simplify_map(mds_kbdc_tree_map_t* restrict tree)
{
  mds_kbdc_tree_t* argument;
  mds_kbdc_tree_t** here;
  mds_kbdc_tree_t* dup_sequence = NULL;
  mds_kbdc_tree_t* temp;
  size_t argument_index;
  int redo = 0, need_reelimination, saved_errno;
  
  /* Check for bad things in the result. */
  for (argument = tree->result; argument; argument = argument->next)
    if ((argument->type != C(KEYS)) && (argument->type != C(STRING)))
      NEW_ERROR(argument, ERROR, "not allowed in mapping output");
  
  /* Valid value properties. */
  if (tree->result == NULL)
    fail_if (check_value_statement_before_simplification(tree));
  
  /* Simplify sequence. */
  for (argument = tree->sequence; argument; argument = argument->next)
    fail_if (simplify(argument));
  
  /* Test predicted emptyness. */
  for (argument = tree->sequence; argument; argument = argument->next)
    if (argument->type != C(NOTHING))
      goto will_not_be_empty;
  if (tree->sequence->processed != PROCESS_LEVEL)
    {
      if (tree->result)
	NEW_ERROR(tree->sequence, ERROR, "mapping of null sequence");
      else
	NEW_ERROR(tree->sequence, ERROR, "nothing in value statement");
    }
  /* The tree parsing process will not allow a mapping statement
   * to start with a ‘.’. Thus if we select to highlight it we
   * know that it is either an empty alternation, an empty
   * unordered subsequence or a nothing inside an alternation.
   * If it is already been processed by the simplifier, it is an
   * error because it is an empty alternation or empty unordered
   * subsequence, and there is not reason to print an additional
   * error. If however it is a nothing inside an alternation we
   * know that it is the cause of the error, however possibily
   * in conjunction with additional such constructs, but those
   * are harder to locate. */
  return 0;
 will_not_be_empty:
  
  /* Remove ‘.’:s. */
  REMOVE_NOTHING(sequence);
  
  /* Because unordered are simplified to alternations of ordered subsequences, which
     in turn can contain alternations, possibiled from simplification of nested
     unordered sequenceses, we need to reeliminated until there are not alternations. */
  for (need_reelimination = 1; need_reelimination ? (need_reelimination = 0, 1) : 0; redo = 0)
    {
      /* Copy sequence. */
      fail_if (dup_sequence = mds_kbdc_tree_dup(tree->sequence), dup_sequence == NULL);
      
      /* Eliminate alterations, remember, unordered subsequences have
	 been simplified to alternations of ordered subsequences. */
      for (argument_index = 0, argument = dup_sequence; argument; argument = argument->next, argument_index++)
	if (argument->type == C(ALTERNATION))
	  fail_if (eliminate_alternation((mds_kbdc_tree_t*)tree, argument, argument_index));
      
      mds_kbdc_tree_free(dup_sequence), dup_sequence = NULL;
      
      /* Eliminated ordered subsequences. */
      for (here = &(tree->sequence); (argument = *here); redo ? (redo = 0) : (here = &(argument->next), 0))
	if (argument->type == C(ORDERED))
	  {
	    FLATTEN(argument);
	    redo = 1;
	  }
	else if (argument->type == C(ALTERNATION))
	  need_reelimination = 1;
    }
  
  /* Valid value properties. */
  if (tree->result == NULL)
    fail_if (check_value_statement_after_simplification(tree));
  
  /* Mapping statements are simplified in a manner similar
   * to how macro calls are simplified. However mapping
   * statements can also contain unordered subsequences,
   * there are translated into alternations of ordered
   * subsequences. Thus after the elimination of alternations,
   * ordered subsequences are eliminated too.
   * 
   * Example of what will happen, ‘{ }’ represents an
   * ordered subsequence:
   * 
   *   (1 2) (3 4) : 0 ## mapping 1
   * 
   *   simplify_map on mapping 1
   *     after simplification
   *       [{1 2} {2 1}] [{3 4} {4 3}] ## mapping 1
   *     after alternation elimination on argument 1
   *       {1 2} [{3 4} {4 3}] ## mapping 1
   *       {2 1} [{3 4} {4 3}] ## mapping 3
   *     after alternation elimination on argument 2
   *       {1 2} {3 4} ## mapping 1
   *       {1 2} {4 3} ## mapping 2
   *       {2 1} [{3 4} {4 3}] ## mapping 3
   *     after ordered subsequence elimination
   *       1 2 3 4 ## mapping 1
   *       {1 2} {4 3} ## mapping 2
   *       {2 1} [{3 4} {4 3}] ## mapping 3
   * 
   *   simplify_map on mapping 2
   *     no difference after simplification
   *     no difference after alternation elimination on argument 1
   *     no difference after alternation elimination on argument 2
   *     after ordered subsequence elimination
   *       1 2 3 4 ## (mapping 1)
   *       1 2 4 3 ## mapping 2
   *       {2 1} [{3 4} {4 3}] ## mapping 3
   * 
   *   simplify_map on mapping 3
   *     no difference after simplification
   *     no difference after alternation elimination on argument 1
   *     after alternation elimination on argument 2
   *       1 2 3 4 ## (mapping 1)
   *       1 2 4 3 ## (mapping 2)
   *       {2 1} {3 4} ## mapping 3
   *       {2 1} {4 3} ## mapping 4
   *     after ordered subsequence elimination
   *       1 2 3 4 ## (mapping 1)
   *       1 2 4 3 ## (mapping 2)
   *       2 1 3 4 ## mapping 3
   *       {2 1} {4 3} ## mapping 4
   * 
   *   simplify_map on mapping 4
   *     no difference after simplification
   *     no difference after alternation elimination on argument 1
   *     no difference after alternation elimination on argument 2
   *     after ordered subsequence elimination
   *       1 2 3 4 ## (mapping 1)
   *       1 2 4 3 ## (mapping 2)
   *       2 1 3 4 ## (mapping 3)
   *       2 1 4 3 ## mapping 4
   * 
   * Nothings (‘.’) are removed before processing the alternations.
   */
  
  return 0;
 fail:
  saved_errno = errno;
  mds_kbdc_tree_free(dup_sequence);
  return errno = saved_errno, -1;
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
      fail_if (simplify((mds_kbdc_tree_t*)tree));
      return 0;
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
	fail_if (simplify_alternation(&(argument->alternation)));
	if (argument->type == C(ALTERNATION))
	  FLATTEN(argument);
	redo = 1;
      }
    else if (argument->type == C(UNORDERED))
      {
	/* Nesting unordered subsequence,
	   simplifies to alternation of ordered subsequence, or simpler. */
	NEW_ERROR(argument, WARNING, "unordered subsequence inside alternation is discouraged");
	fail_if (simplify_unordered(&(argument->unordered)));
	redo = 1;
      }
  
  return 0;
 fail:
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
  int saved_errno, no_perms, stage = 0;
  
  /* Error case. */
  fail_if (elements == NULL);
  
  /* Base case. */
  if (elements->next == NULL)
    {
      fail_if ((first = mds_kbdc_tree_create(C(ORDERED))) == NULL);
      fail_if ((first->ordered.inner = mds_kbdc_tree_dup(elements)) == NULL);
      return first;
    }
  
  stage++;
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
      fail_if (no_perms ? 0 : (subperms == NULL));
      /* Join first element with subpermutations. */
      while (subperms)
	{
	  /* Join. */
	  fail_if (perm = mds_kbdc_tree_dup(&ordered), perm == NULL);
	  perm->ordered.inner->next = subperms->ordered.inner;
	  subperms->ordered.inner = NULL;
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
  
 fail:
  saved_errno = errno;
  mds_kbdc_tree_free(first);
  mds_kbdc_tree_free(subperms);
  if (stage > 0)
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
  
  /* Test for ‘(( ))’. */
  if (tree->inner && (tree->inner->next == NULL) && (tree->inner->type == C(UNORDERED)))
    {
      tree->loc_end = tree->inner->loc_end;
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
      fail_if (simplify((mds_kbdc_tree_t*)tree));
      return -1;
    }
  
  /* Remove ‘.’:s. */
  REMOVE_NOTHING(inner);
  
  /* Check that the sequnced contained anything else. */
  if (tree->inner == NULL)
    {
      NEW_ERROR(tree, ERROR, "unordered subsequence contained nothing else than ‘.’");
      tree->type = C(NOTHING);
      tree->processed = PROCESS_LEVEL;
      return 0;
    }
  
  /* Simplify. */
  for (argument = tree->inner, argument_count = 0; argument; argument = argument->next, argument_count++)
    if (argument->type == C(ALTERNATION))
      {
	fail_if (simplify_alternation(&(argument->alternation)));
	argument->processed = PROCESS_LEVEL;
      }
    else if (argument->type == C(UNORDERED))
      {
	NEW_ERROR(argument, WARNING, "unordered subsequence inside unordered subsequence is discouraged");
	fail_if (simplify_unordered(&(argument->unordered)));
	argument->processed = PROCESS_LEVEL;
      }
  
  /* Check the size of the subsequence. */
  if ((argument_count > 5) && (allow_long * argv_force == 0))
    {
      if (allow_long == 0)
	NEW_ERROR(tree->inner, ERROR,
		  "unordered subsequence longer than 5 elements need double brackets");
      else if (argv_force == 0)
	NEW_ERROR(tree->inner, ERROR,
		  "unordered subsequence of size %zu found, requires ‘--force’ to compile", argument_count);
      return 0;
    }
  
  /* Generate permutations. */
  tree->type = C(ALTERNATION);
  tree->processed = PROCESS_LEVEL;
  arguments = tree->inner;
  if (tree->inner = create_permutations(arguments), tree->inner == NULL)
    {
      if (errno == 0)
	{
	  /* `create_permutations` can return `NULL` without setting `errno`
	   * if it does not list any permutations. */
	  NEW_ERROR_(result, INTERNAL_ERROR, 0, 0, 0, 0, 1,
		     "Fail to create permutations of an unordered sequence");
	  return 0;
	}
      fail_if (tree->inner = arguments, 1);
    }
  mds_kbdc_tree_free(arguments);
  
  return 0;
 fail:
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
#define s(expr)  fail_if (simplify(tree->expr))
#define S(type)  fail_if (simplify_##type(&(tree->type)))
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
    case C(MAP):          S (map);                break;
    case C(ALTERNATION):  S (alternation);        break;
    case C(UNORDERED):    S (unordered);          break;
    case C(MACRO_CALL):   S (macro_call);         break;
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
 fail:
  return -1;
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



#undef FLATTEN
#undef REMOVE_NOTHING
#undef NEW_ERROR
#undef C
#undef PROCESS_LEVEL

