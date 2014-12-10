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
#include "include-stack.h"

#include <alloca.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The `result` parameter of root procedure that requires the include stack
 */
static mds_kbdc_parsed_t* result;

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
static const mds_kbdc_tree_include_t** restrict includes = NULL;

/**
 * The number elements allocated for `includes`
 */
static size_t includes_size = 0;

/**
 * The number elements stored in `includes`
 */
size_t includes_ptr = 0;

/**
 * The latest saved include-stack
 */
static mds_kbdc_include_stack_t* latest_save = NULL;



/**
 * Add “included from here”-notes
 * 
 * @param   ptr  The number of “included from here”-notes
 * @return       Zero on success, -1 on error
 */
int mds_kbdc_include_stack_dump(size_t ptr)
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
 fail:
  result->pathname = old_pathname;
  result->source_code = old_source_code;
  return -1;
} 


/**
 * Mark the root of the tree as included
 * 
 * @param  result_  The `result` parameter of root procedure that requires the include stack
 */
void mds_kbdc_include_stack_begin(mds_kbdc_parsed_t* restrict result_)
{
  latest_save = NULL;
  result = result_;
  original_pathname = result_->pathname;
  original_source_code = result_->source_code;
}


/**
 * Mark the root of the tree as no longer being visited,
 * and release clean up after the use of this module
 */
void mds_kbdc_include_stack_end(void)
{
  latest_save = NULL;
  result->pathname = original_pathname;
  result->source_code = original_source_code;
  free(includes), includes = NULL;
  includes_size = includes_ptr = 0;
}


/**
 * Mark an include-statement as visited
 * 
 * @param   tree  The visisted include-statement
 * @param   data  Output parameter with stack information that should be passed to
 *                the corresponding call to `mds_kbdc_include_stack_pop`, `*data`
 *                is undefined on error
 * @return        Zero on success, -1 on error
 */
int mds_kbdc_include_stack_push(const mds_kbdc_tree_include_t* restrict tree, void** data)
{
  const mds_kbdc_tree_include_t** old = NULL;
  int saved_errno;
  
  if (includes_ptr == includes_size)
    fail_if (xxrealloc(old, includes, includes_size += 4, mds_kbdc_tree_include_t*));
  
  fail_if (xmalloc(*data, 3, void*));
  
  ((char**)(*data))[0] = result->pathname;
  ((mds_kbdc_source_code_t**)(*data))[1] = result->source_code;
  ((mds_kbdc_include_stack_t**)(*data))[2] = latest_save;
  
  includes[includes_ptr++] = tree;
  result->pathname = tree->filename;
  result->source_code = tree->source_code;
  latest_save = NULL;
  
  return 0;
 fail:
  saved_errno = errno;
  free(old);
  return errno = saved_errno, -1;
}


/**
 * Undo the lasted not-undone call to `mds_kbdc_include_stack_push`
 * 
 * This function is guaranteed not to modify `errno`
 * 
 * @param  data  `*data` from `mds_kbdc_include_stack_push`
 */
void mds_kbdc_include_stack_pop(void* data)
{
  int saved_errno = errno;
  result->pathname = ((char**)data)[0];
  result->source_code = ((mds_kbdc_source_code_t**)data)[1];
  latest_save = ((mds_kbdc_include_stack_t**)data)[2];
  includes_ptr--;
  free(data);
  errno = saved_errno;
}


/**
 * Save the current include-stack
 * 
 * @return  The include-stack, `NULL` on error
 */
mds_kbdc_include_stack_t* mds_kbdc_include_stack_save(void)
{
  int saved_errno;
  
  if (latest_save)
    {
      latest_save->duplicates++;
      return latest_save;
    }
  
  fail_if (xmalloc(latest_save, 1, mds_kbdc_include_stack_t));
  
  latest_save->stack = NULL;
  latest_save->ptr = includes_ptr;
  latest_save->duplicates = 0;
  
  if (latest_save->ptr == 0)
    return latest_save;
  
  fail_if (xmemdup(latest_save->stack, includes, latest_save->ptr, const mds_kbdc_tree_include_t*));
  
  return latest_save;
 fail:
  saved_errno = errno;
  if (latest_save)
    free(latest_save->stack), latest_save = NULL;
  errno = saved_errno;
  return NULL;
}


/**
 * Restore a previous include-stack
 * 
 * @param   stack  The include-stack
 * @return         Zero on success, -1 on error
 */
int mds_kbdc_include_stack_restore(mds_kbdc_include_stack_t* restrict stack)
{
  const mds_kbdc_tree_include_t** tmp;
  
  latest_save = stack;
  
  if (stack->ptr > includes_size)
    {
      fail_if (yrealloc(tmp, includes, stack->ptr, const mds_kbdc_tree_include_t*));
      includes_size = stack->ptr;
    }
  
  memcpy(includes, stack->stack, stack->ptr * sizeof(const mds_kbdc_tree_include_t*));
  includes_ptr = stack->ptr;
  return 0;
 fail:
  return -1;
}


/**
 * Destroy a previous include-stack and free its allocation
 * 
 * @param  stack  The include-stack
 */
void mds_kbdc_include_stack_free(mds_kbdc_include_stack_t* restrict stack)
{
  if ((stack == NULL) || (stack->duplicates--))
    return;
  free(stack->stack);
  free(stack);
  if (latest_save == stack)
    latest_save = NULL;
}

