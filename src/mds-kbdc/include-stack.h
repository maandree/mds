/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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
#ifndef MDS_MDS_KBDC_INCLUDE_STACK_H
#define MDS_MDS_KBDC_INCLUDE_STACK_H


#include "parsed.h"



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
#define DUMP_INCLUDE_STACK(PTR)			\
  fail_if (mds_kbdc_include_stack_dump(PTR))

/**
 * Add an error with “included from here”-notes to the error list
 * 
 * @param  NODE:const mds_kbdc_tree_t*    The node the triggered the error
 * @param  PTR:size_t                     The number of “included from here”-notes
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR_WITH_INCLUDES(NODE, PTR, SEVERITY, ...)	\
  do								\
    {								\
      NEW_ERROR_WITHOUT_INCLUDES(NODE, SEVERITY, __VA_ARGS__);	\
      DUMP_INCLUDE_STACK(PTR);					\
    }								\
  while (0)



/**
 * A saved state of the include-stack
 */
typedef struct mds_kbdc_include_stack
{
  /**
   * Stack of visited include-statements
   */
  const mds_kbdc_tree_include_t** stack;
  
  /**
   * The number elements stored in `stack` (do not edit)
   */
  size_t ptr;
  
  /**
   * The number of duplicates there are of this object
   */
  size_t duplicates;
  
} mds_kbdc_include_stack_t;



/**
 * The number elements stored by `mds_kbdc_include_stack_push`
 * but not removed by `mds_kbdc_include_stack_pop`
 */
extern size_t includes_ptr;



/**
 * Add “included from here”-notes
 * 
 * @param   ptr  The number of “included from here”-notes
 * @return       Zero on success, -1 on error
 */
int mds_kbdc_include_stack_dump(size_t ptr);

/**
 * Mark the root of the tree as included
 * 
 * @param  result  The `result` parameter of root procedure that requires the include-stack
 */
void mds_kbdc_include_stack_begin(mds_kbdc_parsed_t* restrict result);

/**
 * Mark the root of the tree as no longer being visited,
 * and release clean up after the use of this module
 */
void mds_kbdc_include_stack_end(void);

/**
 * Mark an include-statement as visited
 * 
 * @param   tree  The visisted include-statement
 * @param   data  Output parameter with stack information that should be passed to
 *                the corresponding call to `mds_kbdc_include_stack_pop`, `*data`
 *                is undefined on error
 * @return        Zero on success, -1 on error
 */
int mds_kbdc_include_stack_push(const mds_kbdc_tree_include_t* restrict tree, void** data);

/**
 * Undo the lasted not-undone call to `mds_kbdc_include_stack_push`
 * 
 * This function is guaranteed not to modify `errno`
 * 
 * @param  data  `*data` from `mds_kbdc_include_stack_push`
 */
void mds_kbdc_include_stack_pop(void* data);

/**
 * Save the current include-stack
 * 
 * @return  The include-stack, `NULL` on error
 */
mds_kbdc_include_stack_t* mds_kbdc_include_stack_save(void);

/**
 * Restore a previous include-stack
 * 
 * @param   stack  The include-stack
 * @return         Zero on success, -1 on error
 */
int mds_kbdc_include_stack_restore(mds_kbdc_include_stack_t* restrict stack);

/**
 * Destroy a previous include-stack and free its allocation
 * 
 * @param  stack  The include-stack
 */
void mds_kbdc_include_stack_free(mds_kbdc_include_stack_t* restrict stack);


#endif

