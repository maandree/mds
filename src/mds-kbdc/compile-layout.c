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
#include "compile-layout.h"

#include "include-stack.h"
#include "string.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>



/**
 * This process's value for `mds_kbdc_tree_t.processed`
 */
#define PROCESS_LEVEL  6

/**
 * Tree type constant shortener
 */
#define C(TYPE)  MDS_KBDC_TREE_TYPE_##TYPE

/**
 * Add an error with “included from here”-notes to the error list
 * 
 * @param  NODE:const mds_kbdc_tree_t*    The node the triggered the error
 * @param  PTR:size_t                     The number of “included from here”-notes
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(NODE, PTR, SEVERITY, ...)			\
  NEW_ERROR_WITH_INCLUDES(NODE, PTR, SEVERITY, __VA_ARGS__)

/**
 * Beginning of failure clause
 */
#define FAIL_BEGIN  pfail: saved_errno = errno

/**
 * End of failure clause
 */
#define FAIL_END  return errno = saved_errno, -1



/**
 * Variable whether the latest created error is stored
 */
static mds_kbdc_parse_error_t* error;

/**
 * The parameter of `compile_layout`
 */
static mds_kbdc_parsed_t* restrict result;

/**
 * 3:   `return` is being processed
 * 2:    `break` is being processed
 * 1: `continue` is being processed
 * 0:    Neither is being processed
 */
static int break_level = 0;

/**
 * Whether a second variant has already been encountered
 */
static int multiple_variants = 0;



/**
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree);



static int check_function_calls_in_literal(const mds_kbdc_tree_t* restrict tree,
					   const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return 0; /* TODO */
}

static char32_t* parse_string(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
}

static char32_t* parse_keys(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
}

static size_t parse_variable(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return 0; /* TODO */
}

static int let(size_t variable, const char32_t* restrict string, const mds_kbdc_tree_t* restrict value,
	       mds_kbdc_tree_t* restrict statement, size_t lineoff, int possibile_shadow_attempt)
{
  (void) variable;
  (void) string;
  (void) value;
  (void) statement;
  (void) lineoff;
  (void) possibile_shadow_attempt;
  return 0; /* TODO */
}

static int push_stack(void)
{
  return 0; /* TODO */
}

static int pop_stack(void)
{
  return 0; /* TODO */
}

static int get_macro(const mds_kbdc_tree_macro_call_t* restrict macro_call,
		     const mds_kbdc_tree_macro_t** restrict macro)
{
  NEW_ERROR(macro_call, includes_ptr, ERROR, "macro ‘%s’ as not been defined yet", macro_call->name);
  /* return set `*macro = NULL` if `(*macro)->processed == PROCESS_LEVEL` */
  (void) macro;
  return 0; /* TODO */
 pfail:
  return -1;
}


/**
 * Compile an include-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_include(mds_kbdc_tree_include_t* restrict tree)
{
  void* data;
  int r;
  if (mds_kbdc_include_stack_push(tree, &data))
    return -1;
  r = compile_subtree(tree->inner);
  mds_kbdc_include_stack_pop(data);
  return r;
}


/**
 * Compile a language-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_language(mds_kbdc_tree_information_language_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char** old = NULL;
  int saved_errno;
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  
  if (result->languages_ptr == result->languages_size)
    {
      result->languages_size = result->languages_size ? (result->languages_size << 1) : 1;
      fail_if (xxrealloc(old, result->languages, result->languages_size, char*));
    }
  
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  fail_if ((code = string_encode(data), code == NULL));
  result->languages[result->languages_ptr++] = code;
  
  return 0;
  FAIL_BEGIN;
  free(old);
  free(data);
  FAIL_END;
}


/**
 * Compile a country-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_country(mds_kbdc_tree_information_country_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char** old = NULL;
  int saved_errno;
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  
  if (result->countries_ptr == result->countries_size)
    {
      result->countries_size = result->countries_size ? (result->countries_size << 1) : 1;
      fail_if (xxrealloc(old, result->countries, result->countries_size, char*));
    }
  
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  fail_if ((code = string_encode(data), code == NULL));
  free(data);
  result->countries[result->countries_ptr++] = code;
  
  return 0;
  FAIL_BEGIN;
  free(old);
  free(data);
  FAIL_END;
}


/**
 * Compile a variant-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_variant(mds_kbdc_tree_information_variant_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  int saved_errno;
  
  if (result->variant)
    {
      if (multiple_variants == 0)
	NEW_ERROR(tree, includes_ptr, ERROR, "only one ‘variant’ is allowed");
      multiple_variants = 1;
      return 0;
    }
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  fail_if ((code = string_encode(data), code == NULL));
  free(data);
  result->variant = code;
  
  return 0;
  FAIL_BEGIN;
  free(data);
  FAIL_END;
}


/**
 * Compile a have-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_have(mds_kbdc_tree_assumption_have_t* restrict tree)
{
  mds_kbdc_tree_t* node = tree->data;
  char32_t* data = NULL;
  char32_t** old = NULL;
  size_t new_size = (node->type == C(STRING)) ? result->assumed_strings_size : result->assumed_keys_size;
  int saved_errno;
  
  new_size = new_size ? (new_size << 1) : 1;
  
  if (node->type == C(STRING))
    {
      fail_if ((data = parse_string(node, node->string.string, node->loc_start), data == NULL));
      if (node->processed == PROCESS_LEVEL)
	return free(data), 0;
      if (result->assumed_strings_ptr == result->assumed_strings_size)
	fail_if (xxrealloc(old, result->assumed_strings, new_size, char*));
      result->assumed_strings_size = new_size;
      result->assumed_strings[result->assumed_strings_ptr++] = data;
    }
  else
    {
      fail_if ((data = parse_keys(node, node->keys.keys, node->loc_start), data == NULL));
      if (node->processed == PROCESS_LEVEL)
	return free(data), 0;
      if (result->assumed_keys_ptr == result->assumed_keys_size)
	fail_if (xxrealloc(old, result->assumed_keys, new_size, char*));
      result->assumed_keys_size = new_size;
      result->assumed_keys[result->assumed_keys_ptr++] = data;
    }
  
  return 0;
  FAIL_BEGIN;
  free(old);
  free(data);
  FAIL_END;
}


/**
 * Compile a have_chars-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_have_chars(mds_kbdc_tree_assumption_have_chars_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char32_t** old = NULL;
  char32_t* restrict character;
  size_t n;
  int saved_errno;
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->chars, lineoff), data == NULL));
  for (n = 0; data[n] >= 0; n++);
  
  if (result->assumed_strings_ptr + n > result->assumed_strings_size)
    {
      result->assumed_strings_size += n;
      fail_if (xxrealloc(old, result->assumed_strings, result->assumed_strings_size, char*));
    }
  
  while (n--)
    {
      fail_if (xmalloc(character, 2, char32_t));
      character[0] = data[n];
      character[1] = -1;
      result->assumed_strings[result->assumed_strings_ptr++] = character;
    }
  
  free(data);
  return 0;
  FAIL_BEGIN;
  free(data);
  free(old);
  FAIL_END;
}


/**
 * Compile a have_range-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_have_range(mds_kbdc_tree_assumption_have_range_t* restrict tree)
{
  size_t lineoff_first;
  size_t lineoff_last;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict first = NULL;
  char32_t* restrict last = NULL;
  char32_t** old = NULL;
  char32_t* restrict character;
  size_t n;
  int saved_errno;
  
  for (lineoff_first = tree->loc_end; code[lineoff_first] == ' '; lineoff_first++);
  for (lineoff_last = lineoff_first + strlen(tree->first); code[lineoff_last] == ' '; lineoff_last++);
  
  fail_if ((first = parse_string((mds_kbdc_tree_t*)tree, tree->first, lineoff_first), first == NULL));
  fail_if ((last = parse_string((mds_kbdc_tree_t*)tree, tree->last, lineoff_last), last == NULL));
  
  if (tree->processed == PROCESS_LEVEL)
    goto done;
  
  if ((first[0] < 0) || (first[1] >= 0))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_first, lineoff_first = 0;
      error->end = error->start + strlen(tree->first);
    }
  if ((last[0] < 0) || (last[1] >= 0))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_last, lineoff_last = 0;
      error->end = error->start + strlen(tree->last);
    }
  
  if ((lineoff_first == 0) || (lineoff_last == 0))
    goto done;
  
  if (*first > *last)
    *first ^= *last, *last ^= *first, *first ^= *last;
  
  n = (size_t)(*last - *first) + 1;
  
  if (result->assumed_strings_ptr + n > result->assumed_strings_size)
    {
      result->assumed_strings_size += n;
      fail_if (xxrealloc(old, result->assumed_strings, result->assumed_strings_size, char*));
    }
  
  while (*first != *last)
    {
      fail_if (xmalloc(character, 2, char32_t));
      character[0] = (*first)++;
      character[1] = -1;
      result->assumed_strings[result->assumed_strings_ptr++] = character;
    }
  
 done:
  free(first);
  free(last);
  return 0;
  FAIL_BEGIN;
  free(first);
  free(last);
  free(old);
  FAIL_END;
}


/**
 * Check that all called macros are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined macro is used
 */
static int check_marco_calls(const mds_kbdc_tree_t* restrict tree)
{
#define t(...)   if (rc |= r = (__VA_ARGS__), r < 0)  return r
  const mds_kbdc_tree_macro_t* macro;
  void* data;
  int r, rc = 0;
 again:
  if (tree == NULL)
    return rc;
  
  switch (tree->type)
    {
    case C(INCLUDE):
      t (mds_kbdc_include_stack_push(&(tree->include), &data));
      t (r = check_marco_calls(tree->include.inner), mds_kbdc_include_stack_pop(data), r);
      break;
      
    case C(FOR):
      t (check_marco_calls(tree->for_.inner));
      break;
      
    case C(IF):
      t (check_marco_calls(tree->if_.inner));
      t (check_marco_calls(tree->if_.otherwise));
      break;
      
    case C(MACRO_CALL):
      t (get_macro(&(tree->macro_call), &macro));
      break;
      
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
  (void) macro;
#undef t
}


/**
 * Check that all called functions in a for-statement are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined function is used
 */
static int check_function_calls_in_for(const mds_kbdc_tree_for_t* restrict tree)
{
#define t(...)  if (rc |= r = check_function_calls_in_literal(__VA_ARGS__), r < 0)  return r
  size_t lineoff_first;
  size_t lineoff_last;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  int r, rc = 0;
  
  for (lineoff_first = tree->loc_end; code[lineoff_first] == ' '; lineoff_first++);
  for (lineoff_last = lineoff_first + strlen(tree->first); code[lineoff_last] == ' '; lineoff_last++);
  
  t ((const mds_kbdc_tree_t*)tree, tree->first, lineoff_first);
  t ((const mds_kbdc_tree_t*)tree, tree->last, lineoff_last);
  
  return rc;
#undef t
}


/**
 * Check that all called functions in an if-statement are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined function is used
 */
static int check_function_calls_in_if(const mds_kbdc_tree_if_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  return check_function_calls_in_literal((const mds_kbdc_tree_t*)tree, tree->condition, lineoff);
}


/**
 * Check that all called functions in a key-combination are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined function is used
 */
static int check_function_calls_in_keys(const mds_kbdc_tree_keys_t* restrict tree)
{
  return check_function_calls_in_literal((const mds_kbdc_tree_t*)tree, tree->keys, tree->loc_end);
}


/**
 * Check that all called functions in a string are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined function is used
 */
static int check_function_calls_in_string(const mds_kbdc_tree_string_t* restrict tree)
{
  return check_function_calls_in_literal((const mds_kbdc_tree_t*)tree, tree->string, tree->loc_end);
}


/**
 * Check that all called functions are already defined
 * 
 * @param   tree  The tree to evaluate
 * @return        Zero on success, -1 on error, 1 if an undefined function is used
 */
static int check_function_calls(const mds_kbdc_tree_t* restrict tree)
{
#define t(...)   if (rc |= r = (__VA_ARGS__), r < 0)  return r
  void* data;
  int r, rc = 0;
 again:
  if (tree == NULL)
    return rc;
  
  switch (tree->type)
    {
    case C(INCLUDE):
      t (mds_kbdc_include_stack_push(&(tree->include), &data));
      t (r = check_function_calls(tree->include.inner), mds_kbdc_include_stack_pop(data), r);
      break;
      
    case C(FOR):
      t (check_function_calls_in_for(&(tree->for_)));
      t (check_function_calls(tree->for_.inner));
      break;
      
    case C(IF):
      t (check_function_calls_in_if(&(tree->if_)));
      t (check_function_calls(tree->if_.inner));
      t (check_function_calls(tree->if_.otherwise));
      break;
      
    case C(LET):
      t (check_function_calls(tree->let.value));
      break;
      
    case C(ARRAY):
      t (check_function_calls(tree->array.elements));
      break;
      
    case C(KEYS):
      t (check_function_calls_in_keys(&(tree->keys)));
      break;
      
    case C(STRING):
      t (check_function_calls_in_string(&(tree->string)));
      break;
      
    case C(MAP):
      t (check_function_calls(tree->map.sequence));
      break;
      
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
#undef t
}


/**
 * Check that a callable's name-suffix is correct
 * 
 * @param   tree  The tree to inspect
 * @return        Zero on sucess, -1 on error, 1 if the name-suffix in invalid
 */
static int check_name_suffix(struct mds_kbdc_tree_callable* restrict tree)
{
  const char* restrict name = strchr(tree->name, '/');
  const char* restrict code = result->source_code->real_lines[tree->loc_line];
  
  if (name == NULL)
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "name-suffix is missing");
      goto name_error;
    }
  if (*++name == '\0')
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "empty name-suffix");
      goto name_error;
    }
  if (!strcmp(name, "0"))
    return 0;
  if (*name == '\0')
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "leading zero in name-suffix");
      goto name_error;
    }
  for (; *name; name++)
    if ((*name < '0') || ('0' < *name))
      {
	NEW_ERROR(tree, includes_ptr, ERROR, "name-suffix may only contain digits");
	goto name_error;
      }
  
  return 0;
 pfail:
  return -1;
 name_error:
  error->start = tree->loc_end;
  while (code[error->start] == ' ')
    error->start++;
  error->end = error->start + strlen(tree->name);
  tree->processed = PROCESS_LEVEL;
  return 1;
}


/**
 * Compile a function
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_function(mds_kbdc_tree_function_t* restrict tree)
{
#define t(expr)  fail_if ((r = (expr), r < 0));  if (r)  tree->processed = PROCESS_LEVEL
  int r;
  
  t (check_name_suffix((struct mds_kbdc_tree_callable*)tree));
  
  /* TODO check for redefinition */
  
  t (check_marco_calls(tree->inner));
  t (check_function_calls(tree->inner));
  
  /* TODO add definition */
  
  return 0;
 pfail:
  return -1;
#undef t
}


/**
 * Compile a macro
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_macro(mds_kbdc_tree_macro_t* restrict tree)
{
#define t(expr)  fail_if ((r = (expr), r < 0));  if (r)  tree->processed = PROCESS_LEVEL
   int r;
  
  t (check_name_suffix((struct mds_kbdc_tree_callable*)tree));
  
  /* TODO check for redefinition */
  
  t (check_marco_calls(tree->inner));
  t (check_function_calls(tree->inner));
  
  /* TODO add definition */
  
  return 0;
 pfail:
  return -1;
#undef t
}


/**
 * Compile a for-loop
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_for(mds_kbdc_tree_for_t* restrict tree)
{
  size_t lineoff_first;
  size_t lineoff_last;
  size_t lineoff_var;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict first = NULL;
  char32_t* restrict last = NULL;
  char32_t diff;
  char32_t character[2];
  size_t variable;
  int saved_errno;
  
  for (lineoff_first = tree->loc_end; code[lineoff_first] == ' '; lineoff_first++);
  for (lineoff_last = lineoff_first + strlen(tree->first); code[lineoff_last] == ' '; lineoff_last++);
  for (lineoff_last += strlen("to"); code[lineoff_last] == ' '; lineoff_last++);
  for (lineoff_var = lineoff_last + strlen(tree->variable); code[lineoff_var] == ' '; lineoff_var++);
  for (lineoff_var += strlen("as"); code[lineoff_var] == ' '; lineoff_var++);
  
  fail_if ((first = parse_string((mds_kbdc_tree_t*)tree, tree->first, lineoff_first), first == NULL));
  fail_if ((last = parse_string((mds_kbdc_tree_t*)tree, tree->last, lineoff_last), last == NULL));
  fail_if ((variable = parse_variable((mds_kbdc_tree_t*)tree, tree->variable, lineoff_var), variable == 0));
  
  if (tree->processed == PROCESS_LEVEL)
    goto done;
  
  if ((first[0] < 0) || (first[1] >= 0))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_first, lineoff_first = 0;
      error->end = error->start + strlen(tree->first);
    }
  if ((last[0] < 0) || (last[1] >= 0))
    {
      NEW_ERROR(tree, includes_ptr, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_last, lineoff_last = 0;
      error->end = error->start + strlen(tree->last);
    }
  
  if ((lineoff_first == 0) || (lineoff_last == 0))
    goto done;
  
  character[1] = -1;
  for (diff = (*first > *last) ? -1 : +1; (*first != *last) && (break_level < 2); *first += diff)
    {
      break_level = 0;
      character[0] = *first;
      fail_if (let(variable, character, NULL, (mds_kbdc_tree_t*)tree, lineoff_var, 1));
      fail_if (compile_subtree(tree->inner));
    }
  
  if (break_level < 3)
    break_level = 0;
  
 done:
  free(first);
  free(last);
  return 0;
  FAIL_BEGIN;
  free(first);
  free(last);
  FAIL_END;
}


/**
 * Compile an if-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_if(mds_kbdc_tree_if_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  int ok, saved_errno;
  size_t i;
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->condition, lineoff), data == NULL));
  fail_if (tree->processed == PROCESS_LEVEL);
  
  for (ok = 1, i = 0; data[i] >= 0; i++)
    ok &= !!(data[i]);
  free(data);
  
  return compile_subtree(ok ? tree->inner : tree->otherwise);
  FAIL_BEGIN;
  free(data);
  FAIL_END;
}


/**
 * Compile a let-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_let(mds_kbdc_tree_let_t* restrict tree)
{
  size_t lineoff;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  mds_kbdc_tree_t* value = NULL;
  size_t variable;
  int saved_errno;
  
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  fail_if ((variable = parse_variable((mds_kbdc_tree_t*)tree, tree->variable, lineoff), variable == 0));
  if (tree->processed == PROCESS_LEVEL)
    return 0;
  
  fail_if ((value = mds_kbdc_tree_dup(tree->value), value == NULL));
  fail_if (compile_subtree(value));
  if ((tree->processed = value->processed) == PROCESS_LEVEL)
    return 0;
  
  fail_if (let(variable, NULL, value, NULL, 0, 0));
  
  free(value);
  return 0;
  FAIL_BEGIN;
  free(value);
  FAIL_END;
}


/*
 * `compile_keys`, `compile_string`, `compile_array` and `evaluate_element`
 * are do only compilation subprocedures that may alter the compiled nodes.
 * This is because (1) `compile_let`, `compile_map` and `compile_macro_call`
 * needs the compiled values, and (2) only duplicates of nodes of types
 * `C(KEYS)`, `C(STRING)` and `C(ARRAY)` are compiled, as they can only be
 * found with `C(LET)`-, `C(MAP)`- and `C(MACRO_CALL)`-nodes.
 */


/**
 * Evaluate an element or argument in a mapping-, value-, let-statement or macro call
 * 
 * @param   node  The element to evaluate
 * @return        Zero on success, -1 on error, 1 if the element is invalid
 */
static int evaluate_element(mds_kbdc_tree_t* restrict node)
{
  char32_t* restrict data = NULL;
  int bad = 0;
  
  for (; node; node = node->next)
    {
      if (node->type == C(STRING))
	fail_if ((data = parse_string(node, node->string.string, node->loc_start), data == NULL));
      if (node->type == C(KEYS))
	fail_if ((data = parse_keys(node, node->keys.keys, node->loc_start), data == NULL));
      free(node->string.string);
      node->string.string = string_encode(data);
      free(data);
      fail_if (node->string.string == NULL);
      bad |= (node->processed == PROCESS_LEVEL);
    }
  
  return bad;
 pfail:
  return -1;
}


/**
 * Compile a key-combination
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_keys(mds_kbdc_tree_keys_t* restrict tree)
{
  return evaluate_element((mds_kbdc_tree_t*)tree) < 0 ? -1 : 0;
}


/**
 * Compile a string
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_string(mds_kbdc_tree_string_t* restrict tree)
{
  return evaluate_element((mds_kbdc_tree_t*)tree) < 0 ? -1 : 0;
}


/**
 * Compile an array
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_array(mds_kbdc_tree_array_t* restrict tree)
{
  int r = evaluate_element(tree->elements);
  if (r < 0)
    return -1;
  if (r)
    tree->processed = PROCESS_LEVEL;
  return 0;
}


/**
 * Compile a mapping- or value-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_map(mds_kbdc_tree_map_t* restrict tree)
{
  int bad = 0;
  mds_kbdc_tree_t* seq = NULL;
  mds_kbdc_tree_t* res = NULL;
  int saved_errno;
  
  fail_if ((seq = mds_kbdc_tree_dup(tree->sequence), seq = NULL));
  fail_if ((res = mds_kbdc_tree_dup(tree->result),   res = NULL));
  
  fail_if ((bad |= evaluate_element(seq), bad < 0));
  fail_if ((bad |= evaluate_element(res), bad < 0));
  if (bad)
    return 0;
  
  /* TODO */
  
  /* NUL-characters (0xC0 0x80) is only allowed in value-statements */
  
  mds_kbdc_tree_free(seq);
  mds_kbdc_tree_free(res);
  return 0;
  FAIL_BEGIN;
  mds_kbdc_tree_free(seq);
  mds_kbdc_tree_free(res);
  FAIL_END;
}


/**
 * Compile a macro call
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  mds_kbdc_tree_t* arg = NULL;
  mds_kbdc_tree_t* arg_;
  const mds_kbdc_tree_macro_t* macro = NULL;
  size_t variable;
  int bad, saved_errno;
  
  fail_if ((arg = mds_kbdc_tree_dup(tree->arguments), arg = NULL));
  fail_if ((bad = evaluate_element(arg), bad < 0));
  if (bad)
    return 0;
  
  fail_if (get_macro(tree, &macro));
  if (macro == NULL)
    goto done;
  
  fail_if (push_stack());
  for (arg_ = arg; arg_; arg_ = arg_->next)
    fail_if (let(variable, NULL, arg_, NULL, 0, 0));
  fail_if (compile_subtree(macro->inner));
  fail_if (pop_stack());
  
 done:
  break_level = 0;
  mds_kbdc_tree_free(arg);
  return 0;
  FAIL_BEGIN;
  mds_kbdc_tree_free(arg);
  FAIL_END;
}


/**
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree)
{
#define t(expr)   if (r = (expr), r < 0)  return r
#define c(type)   t (compile_##type(&(tree->type)))
#define c_(type)  t (compile_##type(&(tree->type##_)))
  int r;
 again:
  if (tree == NULL)
    return 0;
  
  if (tree->processed == PROCESS_LEVEL)
    goto next;
  
  switch (tree->type)
    {
    case C(INFORMATION):
      t (compile_subtree(tree->information.inner));
      break;
    case C(INFORMATION_LANGUAGE):   c (language);     break;
    case C(INFORMATION_COUNTRY):    c (country);      break;
    case C(INFORMATION_VARIANT):    c (variant);      break;
    case C(INCLUDE):                c (include);      break;
    case C(FUNCTION):               c (function);     break;
    case C(MACRO):                  c (macro);        break;
    case C(ASSUMPTION):
      t ((includes_ptr == 0) && compile_subtree(tree->assumption.inner));
      break;
    case C(ASSUMPTION_HAVE):        c (have);         break;
    case C(ASSUMPTION_HAVE_CHARS):  c (have_chars);   break;
    case C(ASSUMPTION_HAVE_RANGE):  c (have_range);   break;
    case C(FOR):                    c_ (for);         break;
    case C(IF):                     c_ (if);          break;
    case C(LET):                    c (let);          break;
    case C(KEYS):                   c (keys);         break;
    case C(STRING):                 c (string);       break;
    case C(ARRAY):                  c (array);        break;
    case C(MAP):                    c (map);          break;
    case C(MACRO_CALL):             c (macro_call);   break;
    case C(RETURN):                 break_level = 3;  break;
    case C(BREAK):                  break_level = 2;  break;
    case C(CONTINUE):               break_level = 1;  break;
    default:
      break;
    }
  
 next:
  if (break_level)
    return 0;
  
  tree = tree->next;
  goto again;
#undef c_
#undef c
#undef t
}


/**
 * Compile the layout code
 * 
 * @param   result_  `result` from `eliminate_dead_code`, will be updated
 * @return           -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int compile_layout(mds_kbdc_parsed_t* restrict result_)
{
  int r;
  mds_kbdc_include_stack_begin(result = result_);
  r = compile_subtree(result_->tree);
  return mds_kbdc_include_stack_end(), r;
}



#undef FAIL_END
#undef FAIL_BEGIN
#undef NEW_ERROR
#undef C
#undef PROCESS_LEVEL

