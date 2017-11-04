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
#include "call-stack.h"

#include <stdlib.h>



/**
 * An entry in the call-stack
 */
typedef struct mds_kbdc_call
{
  /**
   * The tree node where the call was made
   */
  const mds_kbdc_tree_t* tree;
  
  /**
   * The position of the line of the tree node
   * where the call begins
   */
  size_t start;
  
  /**
   * The position of the line of the tree node
   * where the call end
   */
  size_t end;
  
  /**
   * A snapshot of the include-stack as it
   * looked when the call was being made
   */
  mds_kbdc_include_stack_t* include_stack;
  
} mds_kbdc_call_t;



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The `result` parameter of root procedure that requires the include-stack
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
 * Stack of visited function- and macro-calls
 */
static mds_kbdc_call_t* restrict calls = NULL;

/**
 * The number elements allocated for `calls`
 */
static size_t calls_size = 0;

/**
 * The number elements stored in `calls`
 */
static size_t calls_ptr = 0;



/**
 * Add “called from here”-notes
 * 
 * @return  Zero on success, -1 on error
 */
int mds_kbdc_call_stack_dump(void)
{
  char* old_pathname = result->pathname;
  mds_kbdc_source_code_t* old_source_code = result->source_code;
  size_t ptr = calls_ptr, iptr;
  mds_kbdc_call_t* restrict call;
  mds_kbdc_include_stack_t* restrict includes;
  
  while (ptr--)
    {
      call = calls + ptr;
      includes = call->include_stack;
      iptr = includes->ptr;
      result->pathname    = iptr ? includes->stack[iptr - 1]->filename    : original_pathname;
      result->source_code = iptr ? includes->stack[iptr - 1]->source_code : original_source_code;
      NEW_ERROR_(result, NOTE, 1, call->tree->loc_line, call->start, call->end, 1, "called from here");
      DUMP_INCLUDE_STACK(iptr);
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
 * Prepare for usage of call-stacks
 * 
xo * @param  result_  The `result` parameter of root procedure that requires the call-stack
 */
void mds_kbdc_call_stack_begin(mds_kbdc_parsed_t* restrict result_)
{
  result = result_;
  original_pathname = result_->pathname;
  original_source_code = result_->source_code;
}


/**
 * Cleanup after usage of call-stacks
 */
void mds_kbdc_call_stack_end(void)
{
  result->pathname = original_pathname;
  result->source_code = original_source_code;
  free(calls), calls = NULL;
  calls_size = calls_ptr = 0;
}


/**
 * Mark an function- or macro-call
 * 
 * @param   tree   The tree node where the call was made
 * @param   start  The position of the line of the tree node where the call begins
 * @param   end    The position of the line of the tree node where the call end
 * @return         Zero on success, -1 on error
 */
int mds_kbdc_call_stack_push(const mds_kbdc_tree_t* restrict tree, size_t start, size_t end)
{
  mds_kbdc_call_t* tmp;
  mds_kbdc_call_t* call;
  
  if (calls_ptr == calls_size)
    {
      fail_if (yrealloc(tmp, calls, calls_size + 4, mds_kbdc_call_t));
      calls_size += 4;
    }
  
  call = calls + calls_ptr++;
  call->tree = tree;
  call->start = start;
  call->end = end;
  call->include_stack = mds_kbdc_include_stack_save();
  fail_if (call->include_stack == NULL);
  
  return 0;
 fail:
  return -1;
}


/**
 * Undo the lasted not-undone call to `mds_kbdc_call_stack_push`
 * 
 * This function is guaranteed not to modify `errno`
 */
void mds_kbdc_call_stack_pop(void)
{
  int saved_errno = errno;
  mds_kbdc_include_stack_free(calls[--calls_ptr].include_stack);
  errno = saved_errno;
}

