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
#include "callables.h"

#include <stdlib.h>
#include <string.h>



/**
 * Map, by index, from argument count, to list of callable's names
 */
static char*** restrict names = NULL;

/**
 * If `callable_list[callabes[i][j]]` and `callable_include_stack_list[callabes[i][j]]`
 * describe the callable named by `named[i][j]` with either `i` parameters, or the
 * number of parameters specified by the suffix in `named[i][j]`
 */
static size_t** restrict callables = NULL;

/**
 * Map the the number of elements in the, by index, corresponding element in `names`
 */
static size_t* restrict bucket_sizes = NULL;

/**
 * List of callables
 */
static mds_kbdc_tree_t** restrict callable_list = NULL;

/**
 * List of callables' include-stacks
 */
static mds_kbdc_include_stack_t** restrict callable_include_stack_list = NULL;

/**
 * The number of buckets in `names` and `bucket_sizes`
 */
static size_t buckets = 0;

/**
 * The index of the next item in `callable_list` and `callable_include_stack_list`
 */
static size_t list_ptr = 0;



/**
 * Destroy the callable storage
 */
void callables_terminate(void)
{
  size_t i, j, n;
  char** bucket;
  for (i = 0; i < buckets; i++)
    {
      bucket = names[i];
      for (j = 0, n = bucket_sizes[i]; j < n; j++)
	free(bucket[j]);
      free(bucket);
      free(callables[i]);
    }
  for (i = 0; i < list_ptr; i++)
    mds_kbdc_include_stack_free(callable_include_stack_list[i]);
  free(callables), callables = NULL;
  free(names), names = NULL;
  free(bucket_sizes), bucket_sizes = NULL;
  free(callable_list), callable_list = NULL;
  free(callable_include_stack_list), callable_include_stack_list = NULL;
  buckets = list_ptr = 0;
}


/**
 * Store a callable
 * 
 * @param   name                    The name of the callable
 * @param   arg_count               The number of arguments the callable takes
 *                                  if `name` is suffixless, otherwise zero
 * @param   callable                The callable
 * @param   callable_include_stack  The include-stack for the callable
 * @return                          Zero on success, -1 on error
 */
int callables_set(const char* restrict name, size_t arg_count, mds_kbdc_tree_t* restrict callable,
		  mds_kbdc_include_stack_t* restrict callable_include_stack)
{
#define _yrealloc(var, elements, type)  (yrealloc(tmp_##var, var, elements, type))
  
  char* dupname = NULL;
  char*** tmp_names = NULL;
  size_t** tmp_callables = NULL;
  size_t* tmp_bucket_sizes = NULL;
  char** old_names = NULL;
  size_t* old_callables = NULL;
  mds_kbdc_tree_t** tmp_callable_list = NULL;
  mds_kbdc_include_stack_t** tmp_callable_include_stack_list = NULL;
  int saved_errno;
  
  fail_if (xstrdup(dupname, name));
  
  if (arg_count >= buckets)
    {
      fail_if (_yrealloc(names,        arg_count + 1, char**));
      fail_if (_yrealloc(callables,    arg_count + 1, size_t*));
      fail_if (_yrealloc(bucket_sizes, arg_count + 1, size_t));
      memset(names        + buckets, 0, (arg_count + 1 - buckets) * sizeof(char**));
      memset(callables    + buckets, 0, (arg_count + 1 - buckets) * sizeof(size_t*));
      memset(bucket_sizes + buckets, 0, (arg_count + 1 - buckets) * sizeof(size_t));
      buckets = arg_count + 1;
    }
  
  fail_if (xxrealloc(old_names,     names[arg_count],     bucket_sizes[arg_count] + 1, char*));
  fail_if (xxrealloc(old_callables, callables[arg_count], bucket_sizes[arg_count] + 1, size_t));
  
  names[arg_count][bucket_sizes[arg_count]] = dupname, dupname = NULL;
  callables[arg_count][bucket_sizes[arg_count]] = list_ptr;
  bucket_sizes[arg_count]++;
  
  fail_if (_yrealloc(callable_list, list_ptr + 1, mds_kbdc_tree_t*));
  fail_if (_yrealloc(callable_include_stack_list, list_ptr + 1, mds_kbdc_include_stack_t*));
  
  callable_list[list_ptr] = callable;
  callable_include_stack_list[list_ptr] = callable_include_stack;
  list_ptr++;
  
  return 0;
 fail:
  saved_errno = errno;
  free(dupname);
  if (old_names)
    names[arg_count] = old_names;
  if (old_callables)
    callables[arg_count] = old_callables;
  return errno = saved_errno, -1;
#undef _yrealloc
}


/**
 * Get a stored callable
 * 
 * @param  name                    The name of the callable
 * @param  arg_count               The number of arguments the callable takes
 *                                 if `name` is suffixless, otherwise zero
 * @param  callable                Output parameter for the callable, `NULL` if not found
 * @param  callable_include_stack  Output parameter for the include-stack for the callable
 */
void callables_get(const char* restrict name, size_t arg_count, mds_kbdc_tree_t** restrict callable,
		   mds_kbdc_include_stack_t** restrict callable_include_stack)
{
  char** restrict names_;
  size_t i, n;
  
  *callable = NULL;
  *callable_include_stack = NULL;
  
  if (arg_count >= buckets)
    return;
  
  names_ = names[arg_count];
  for (i = 0, n = bucket_sizes[arg_count]; i < n; i++)
    {
      if (strcmp(names_[i], name))
	continue;
      i = callables[arg_count][i];
      *callable = callable_list[i];
      *callable_include_stack = callable_include_stack_list[i];
      return;
    }
}

