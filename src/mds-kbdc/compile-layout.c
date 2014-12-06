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
/* TODO add call stack */

#include "include-stack.h"
#include "builtin-functions.h"
#include "string.h"
#include "variables.h"
#include "callables.h"

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
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(NODE, SEVERITY, ...)					\
  NEW_ERROR_WITH_INCLUDES(NODE, includes_ptr, SEVERITY, __VA_ARGS__)

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
 * The previous value-statement, which has no effect
 * if we can find another value statemnt that is
 * sure to be evaluated
 * 
 * (We will not look too hard.)
 */
static mds_kbdc_tree_t* last_value_statement = NULL;



/**
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree);



/*** Macro-, function- and variable-support, string-parsing and value- and mapping-compilation. ***/
/*                           (Basically everything except tree-walking.)                          */


static int check_function_calls_in_literal(const mds_kbdc_tree_t* restrict tree,
					   const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return 0; /* TODO */
}


/**
 * Parse a quoted string
 * 
 * @param   tree     The statement where the string is located
 * @param   raw      The string to parse
 * @param   lineoff  The offset on the line where the string beings
 * @return           The string as pure text, `NULL` on error
 */
static char32_t* parse_quoted_string(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
}


/**
 * Parse am unquoted string
 * 
 * @param   tree     The statement where the string is located
 * @param   raw      The string to parse
 * @param   lineoff  The offset on the line where the string beings
 * @return           The string as pure text, `NULL` on error
 */
static char32_t* parse_unquoted_string(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
}


/**
 * Parse a string
 * 
 * @param   tree     The statement where the string is located
 * @param   raw      The string to parse
 * @param   lineoff  The offset on the line where the string beings
 * @return           The string as pure text, `NULL` on error
 */
static char32_t* parse_string(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  mds_kbdc_tree_t* old_last_value_statement = last_value_statement;
  char32_t* rc = ((*raw == '"') ? parse_quoted_string : parse_unquoted_string)(tree, raw, lineoff);
  last_value_statement = old_last_value_statement;
  return rc;
}


static char32_t* parse_keys(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
  
  /* Do not forget to store and then restore `last_value_statement` */
}


/**
 * Parse a variable string
 * 
 * @param   tree     The statement where the variable is selected
 * @param   raw      The variable string
 * @param   lineoff  The offset on the line where the variable string begins
 * @return           The index of the variable, zero on error
 */
static size_t parse_variable(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  int bad = 0;
  size_t var;
  const char* restrict raw_ = raw;
  
  /* The variable must begin with \. */
  if (*raw++ != '\\')  bad = 1;
  /* Zero is not a valid varible, nor may there be leading zeroes.  */
  if (*raw++ == '0')  bad = 1;
  for (; *raw; raw++)
    /* Check that the variable consists only of digits. */
    if (('0' <= *raw) && (*raw <= '9'));
    /* However, it may end with a dot. */
    else if ((raw[0] == '.') && (raw[1] == '\0'));
    else
      bad = 1;
  
  /* Report an error but return a variable index if the variable-string is invalid. */
  if (bad)
    {
      NEW_ERROR(tree, ERROR, "not a variable");
      error->start = lineoff;
      error->end = lineoff + strlen(raw_);
      tree->processed = PROCESS_LEVEL;
      return 1;
    }
  
  /* Parse the variable string and check that it did not overflow. */
  var = (size_t)atoll(raw_ + 1);
  if (strlen(raw_ + 1) != (size_t)snprintf(NULL, 0, "%zu", var))
    return errno = ERANGE, (size_t)0;
  return var;
 pfail:
  return 0;
}


/**
 * Assign a value to a variable, and define or shadow it in the process
 * 
 * @param   variable                  The variable index
 * @param   string                    The variable's new value, must be `NULL` iff `value != NULL`
 * @param   value                     The variable's new value, must be `NULL` iff `string != NULL`
 * @param   statement                 The statement where the variable is assigned, may be `NULL`
 * @param   lineoff                   The offset of the line for where the string selecting the variable begins
 * @param   possibile_shadow_attempt  Whether `statement` is of a type that does not shadow variables,
 *                                    but could easily be mistaked for one that does
 * @return                            Zero on success, -1 on error
 */
static int let(size_t variable, const char32_t* restrict string, const mds_kbdc_tree_t* restrict value,
	       mds_kbdc_tree_t* restrict statement, size_t lineoff, int possibile_shadow_attempt)
{
  mds_kbdc_tree_t* tree = NULL;
  int saved_errno;
  
  /* Warn if this is a possible shadow attempt. */
  if (possibile_shadow_attempt && variables_let_will_override(variable) &&
      statement && (statement->processed != PROCESS_LEVEL))
    {
      statement->processed = PROCESS_LEVEL;
      NEW_ERROR(statement, WARNING, "does not shadow existing definition");
      error->start = lineoff;
      error->end = lineoff + (size_t)snprintf(NULL, 0, "\\%zu", variable);
    }
  
  /* Duplicate value. */
  if (value)
    fail_if ((tree = mds_kbdc_tree_dup(value), tree == NULL));
  if (value == NULL)
    {
      fail_if ((tree = mds_kbdc_tree_create(C(COMPILED_STRING)), tree == NULL));
      tree->compiled_string.string = string_dup(string);
      fail_if (tree->compiled_string.string == NULL);
    }
  
  /* Assign variable. */
  fail_if (variables_let(variable, tree));
  return 0;
  FAIL_BEGIN;
  mds_kbdc_tree_free(tree);
  FAIL_END;
}


/**
 * Store a macro
 * 
 * @param   macro                The macro
 * @param   macro_include_stack  The include-stack for the macro
 * @return                       Zero on success, -1 on error
 */
static int set_macro(mds_kbdc_tree_macro_t* restrict macro,
		     mds_kbdc_include_stack_t* macro_include_stack)
{
  return callables_set(macro->name, 0, (mds_kbdc_tree_t*)macro, macro_include_stack);
}


/**
 * Get a stored macro
 * 
 * @param  name                 The name of the macro, with suffix
 * @param  macro                Output parameter for the macro, `NULL` if not found
 * @param  macro_include_stack  Output parameter for the include-stack for the macro
 */
static void get_macro_lax(const char* restrict macro_name, mds_kbdc_tree_macro_t** restrict macro,
			  mds_kbdc_include_stack_t** restrict macro_include_stack)
{
  callables_get(macro_name, 0, (mds_kbdc_tree_t**)macro, macro_include_stack);
}


/**
 * Get a stored macro
 * 
 * The function is similar to `get_macro_lax`, however, this fucntion
 * will report an error if the macro has not yet been defined, and it
 * will pretend that it has not yet been defined if the macro contained
 * an error in an earlier called to it
 * 
 * @param   macro_call           The macro-call
 * @param   macro                Output parameter for the macro, `NULL` if not found or has an error
 * @param   macro_include_stack  Output parameter for the include-stack for the macro
 * @return                       Zero on success, -1 on error
 */
static int get_macro(mds_kbdc_tree_macro_call_t* restrict macro_call,
		     mds_kbdc_tree_macro_t** restrict macro,
		     mds_kbdc_include_stack_t** restrict macro_include_stack)
{
  get_macro_lax(macro_call->name, macro, macro_include_stack);
  if (*macro == NULL)
    {
      NEW_ERROR(macro_call, ERROR, "macro ‘%s’ as not been defined yet", macro_call->name);
      macro_call->processed = PROCESS_LEVEL;
      return 0;
    }
  if ((*macro)->processed == PROCESS_LEVEL)
    *macro = NULL;
  return 0;
 pfail:
  return -1;
}


/**
 * Store a function
 * 
 * @param   function                The function
 * @param   function_include_stack  The include-stack for the function
 * @return                          Zero on success, -1 on error
 */
static int set_function(mds_kbdc_tree_function_t* restrict function,
			mds_kbdc_include_stack_t* function_include_stack)
{
  char* suffixless = function->name;
  char* suffix_start = strchr(suffixless, '/');
  size_t arg_count = (size_t)atoll(suffix_start + 1);
  int r;
  
  *suffix_start = '\0';
  r = callables_set(suffixless, arg_count, (mds_kbdc_tree_t*)function, function_include_stack);
  return *suffix_start = '/', r;
}


/**
 * Get a stored function
 * 
 * @param  name                    The name of the function, suffixless
 * @param  arg_count               The number of arguments the function takes
 * @param  function                Output parameter for the function, `NULL` if not found
 * @param  function_include_stack  Output parameter for the include-stack for the function
 */
static void get_function_lax(const char* restrict function_name, size_t arg_count,
			     mds_kbdc_tree_function_t** restrict function,
			     mds_kbdc_include_stack_t** restrict function_include_stack)
{
  callables_get(function_name, arg_count, (mds_kbdc_tree_t**)function, function_include_stack);
}


static int set_return_value(char32_t* restrict value)
{
  free(value);
  last_value_statement = NULL; /* should only be done if we have side-effects */
  return 1; /* TODO */
}


static int add_mapping(mds_kbdc_tree_map_t* restrict mapping, mds_kbdc_include_stack_t* restrict include_stack)
{
  mds_kbdc_tree_free((mds_kbdc_tree_t*)mapping);
  mds_kbdc_include_stack_free(include_stack);
  return 0; /* TODO */
}



/*** Tree-walking. ***/


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
  
  /* For simplicity we set `last_value_statement` on includes,
   * so we are sure `last_value_statement` has the same
   * include-stack as its overriding statement. */
  last_value_statement = NULL;
  
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
  
  /* Make sure the language-list fits another entry. */
  if (result->languages_ptr == result->languages_size)
    {
      result->languages_size = result->languages_size ? (result->languages_size << 1) : 1;
      fail_if (xxrealloc(old, result->languages, result->languages_size, char*));
    }
  
  /* Locate the first character in the language-string. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  /* Evaluate function calls, variable dereferences and escapes in the language-string. */
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  if (tree->processed == PROCESS_LEVEL)
    return free(data), 0;
  /* We want the string in UTF-8, not UTF-16. */
  fail_if ((code = string_encode(data), code == NULL));
  free(data);
  
  /* Add the language to the language-list. */
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
  
  /* Make sure the country-list fits another entry. */
  if (result->countries_ptr == result->countries_size)
    {
      result->countries_size = result->countries_size ? (result->countries_size << 1) : 1;
      fail_if (xxrealloc(old, result->countries, result->countries_size, char*));
    }
  
  /* Locate the first character in the country-string. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  /* Evaluate function calls, variable dereferences and escapes in the country-string. */
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  if (tree->processed == PROCESS_LEVEL)
    return free(data), 0;
  /* We want the string in UTF-8, not UTF-16. */
  fail_if ((code = string_encode(data), code == NULL));
  free(data);
  
  /* Add the country to the country-list. */
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
  
  /* Make sure the variant has not already been set. */
  if (result->variant)
    {
      if (multiple_variants == 0)
	NEW_ERROR(tree, ERROR, "only one ‘variant’ is allowed");
      multiple_variants = 1;
      return 0;
    }
  
  /* Locate the first character in the variant-string. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  /* Evaluate function calls, variable dereferences and escapes in the variant-string. */
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  if (tree->processed == PROCESS_LEVEL)
    return free(data), 0;
  /* We want the string in UTF-8, not UTF-16. */
  fail_if ((code = string_encode(data), code == NULL));
  free(data);
  
  /* Store the variant. */
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
  size_t new_size = 0;
  int saved_errno;
  
  /* Make sure we can fit all assumption in the assumption list (part 1/2). */
  new_size = (node->type == C(STRING)) ? result->assumed_strings_size : result->assumed_keys_size;
  new_size = new_size ? (new_size << 1) : 1;
  
  if (node->type == C(STRING))
    {
      /* Evaluate function calls, variable dereferences and escapes in the string. */
      fail_if ((data = parse_string(node, node->string.string, node->loc_start), data == NULL));
      if (node->processed == PROCESS_LEVEL)
	return free(data), 0;
      /* Make sure we can fit all strings in the assumption list (part 2/2). */
      if (result->assumed_strings_ptr == result->assumed_strings_size)
	fail_if (xxrealloc(old, result->assumed_strings, new_size, char*));
      result->assumed_strings_size = new_size;
      /* Add the assumption to the list. */
      result->assumed_strings[result->assumed_strings_ptr++] = data;
    }
  else
    {
      /* Evaluate function calls, variable dereferences and escapes in the key-combination. */
      fail_if ((data = parse_keys(node, node->keys.keys, node->loc_start), data == NULL));
      if (node->processed == PROCESS_LEVEL)
	return free(data), 0;
      /* Make sure we can fit all key-combinations in the assumption list (part 2/2). */
      if (result->assumed_keys_ptr == result->assumed_keys_size)
	fail_if (xxrealloc(old, result->assumed_keys, new_size, char*));
      result->assumed_keys_size = new_size;
      /* Add the assumption to the list. */
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
  
  /* Locate the first character in the list. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  /* Evaluate function calls, variable dereferences
     and escapes in the charcter list. */
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->chars, lineoff), data == NULL));
  if (tree->processed == PROCESS_LEVEL)
    return free(data), 0;
  
  /* Make sure we can fit all characters in the assumption list. */
  for (n = 0; data[n] >= 0; n++);
  if (result->assumed_strings_ptr + n > result->assumed_strings_size)
    {
      result->assumed_strings_size += n;
      fail_if (xxrealloc(old, result->assumed_strings, result->assumed_strings_size, char*));
    }
  
  /* Add all characters to the assumption list. */
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
  
  
  /* Locate the first characters of both bound strings. */
  for (lineoff_first = tree->loc_end; code[lineoff_first] == ' '; lineoff_first++);
  for (lineoff_last = lineoff_first + strlen(tree->first); code[lineoff_last] == ' '; lineoff_last++);
  
  /* Duplicate bounds and evaluate function calls,
     variable dereferences and escapes in the bounds. */
  fail_if ((first = parse_string((mds_kbdc_tree_t*)tree, tree->first, lineoff_first), first == NULL));
  fail_if ((last = parse_string((mds_kbdc_tree_t*)tree, tree->last, lineoff_last), last == NULL));
  
  /* Did one of the bound not evaluate, then stop. */
  if (tree->processed == PROCESS_LEVEL)
    goto done;
  
  
  /* Check that the primary bound is single-character. */
  if ((first[0] < 0) || (first[1] >= 0))
    {
      NEW_ERROR(tree, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_first, lineoff_first = 0;
      error->end = error->start + strlen(tree->first);
    }
  /* Check that the secondary bound is single-character. */
  if ((last[0] < 0) || (last[1] >= 0))
    {
      NEW_ERROR(tree, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_last, lineoff_last = 0;
      error->end = error->start + strlen(tree->last);
    }
  
  /* Was one of the bounds not single-character, then stop. */
  if ((lineoff_first == 0) || (lineoff_last == 0))
    goto done;
  
  
  /* If the range is descending, swap the bounds so it is ascending.
     (This cannot be done in for-loops as that may cause side-effects
     to be created in the wrong order.) */
  if (*first > *last)
    *first ^= *last, *last ^= *first, *first ^= *last;
  
  /* Make sure we can fit all characters in the assumption list. */
  n = (size_t)(*last - *first) + 1;
  if (result->assumed_strings_ptr + n > result->assumed_strings_size)
    {
      result->assumed_strings_size += n;
      fail_if (xxrealloc(old, result->assumed_strings, result->assumed_strings_size, char*));
    }
  
  /* Add all characters to the assumption list. */
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
static int check_marco_calls(mds_kbdc_tree_t* restrict tree)
{
#define t(...)   if (rc |= r = (__VA_ARGS__), r < 0)  return r
  mds_kbdc_tree_macro_t* _macro;
  mds_kbdc_include_stack_t* _macro_include_stack;
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
      t (get_macro(&(tree->macro_call), &_macro, &_macro_include_stack));
      break;
      
    default:
      break;
    }
  
  tree = tree->next;
  goto again;
  (void) _macro;
  (void) _macro_include_stack;
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
      
    case C(LET):     t (check_function_calls(tree->let.value));            break;
    case C(ARRAY):   t (check_function_calls(tree->array.elements));       break;
    case C(KEYS):    t (check_function_calls_in_keys(&(tree->keys)));      break;
    case C(STRING):  t (check_function_calls_in_string(&(tree->string)));  break;
    case C(MAP):     t (check_function_calls(tree->map.sequence));         break;
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
  
  /* A "/" must exist in the name to tell us how many parameters there are. */
  if (name == NULL)
    {
      NEW_ERROR(tree, ERROR, "name-suffix is missing");
      goto name_error;
    }
  /* Do not let the suffix by just "/". */
  if (*++name == '\0')
    {
      NEW_ERROR(tree, ERROR, "empty name-suffix");
      goto name_error;
    }
  
  /* We are all good if the suffix is simply "/0" */
  if (!strcmp(name, "0"))
    return 0;
  
  /* The suffix may not have leading zeroes. */
  if (*name == '\0')
    {
      NEW_ERROR(tree, ERROR, "leading zero in name-suffix");
      goto name_error;
    }
  /* The suffix must be a decimal, non-negative number. */
  for (; *name; name++)
    if ((*name < '0') || ('0' < *name))
      {
	NEW_ERROR(tree, ERROR, "name-suffix may only contain digits");
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
  mds_kbdc_tree_function_t* function;
  mds_kbdc_include_stack_t* function_include_stack;
  mds_kbdc_include_stack_t* our_include_stack = NULL;
  char* suffixless;
  char* suffix_start = NULL;
  size_t arg_count;
  int r, saved_errno;
  
  /* Check that the suffix if properly formatted. */
  t (check_name_suffix((struct mds_kbdc_tree_callable*)tree));
  
  /* Get the function's name without suffix, and parse the suffix. */
  suffixless = tree->name;
  suffix_start = strchr(suffixless, '/');
  *suffix_start++ = '\0';
  arg_count = (size_t)atoll(suffix_start--);
  
  /* Check that the function is not already defined as a builtin function. */
  if (builtin_function_defined(suffixless, arg_count))
    {
      NEW_ERROR(tree, ERROR, "function ‘%s’ is already defined as a builtin function", tree->name);
      *suffix_start = '/';
      return 0;
    }
  /* Check that the function is not already defined,
     the include-stack is used in the error-clause as
     well as later when we list the function as defined. */
  get_function_lax(suffixless, arg_count, &function, &function_include_stack);
  fail_if ((our_include_stack = mds_kbdc_include_stack_save(), our_include_stack == NULL));
  if (function)
    {
      *suffix_start = '/';
      NEW_ERROR(tree, ERROR, "function ‘%s’ is already defined", tree->name);
      fail_if (mds_kbdc_include_stack_restore(function_include_stack));
      NEW_ERROR(function, NOTE, "previously defined here");
      fail_if (mds_kbdc_include_stack_restore(our_include_stack));
      mds_kbdc_include_stack_free(our_include_stack);
      return 0;
    }
  
  /* Check the the function does not call macros or functions
   * before they are defined, otherwise they may get defined
   * between the definition of the function and calls to it. */
  t (check_marco_calls(tree->inner));
  t (check_function_calls(tree->inner));
  
  /* List the function as defined. */
  *suffix_start = '/', suffix_start = NULL;
  t (set_function(tree, our_include_stack));
  
  return 0;
  FAIL_BEGIN;
  if (suffix_start)
    *suffix_start = '/';
  mds_kbdc_include_stack_free(our_include_stack);
  FAIL_END;
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
  mds_kbdc_tree_macro_t* macro;
  mds_kbdc_include_stack_t* macro_include_stack;
  mds_kbdc_include_stack_t* our_include_stack = NULL;
  int r, saved_errno;
  
  /* Check that the suffix if properly formatted. */
  t (check_name_suffix((struct mds_kbdc_tree_callable*)tree));
  
  /* Check that the macro is not already defined,
     the include-stack is used in the error-clause as
     well as later when we list the macro as defined. */
  fail_if ((our_include_stack = mds_kbdc_include_stack_save(), our_include_stack == NULL));
  get_macro_lax(tree->name, &macro, &macro_include_stack);
  if (macro)
    {
      NEW_ERROR(tree, ERROR, "macro ‘%s’ is already defined", tree->name);
      fail_if (mds_kbdc_include_stack_restore(macro_include_stack));
      NEW_ERROR(macro, NOTE, "previously defined here");
      fail_if (mds_kbdc_include_stack_restore(our_include_stack));
      mds_kbdc_include_stack_free(our_include_stack);
      return 0;
    }
  
  /* Check the the macro does not call macros or functions
   * before they are defined, otherwise they may get defined
   * between the definition of the macro and calls to it. */
  t (check_marco_calls(tree->inner));
  t (check_function_calls(tree->inner));
  
  /* List the macro as defined. */
  t (set_macro(tree, our_include_stack));
  
  return 0;
  FAIL_BEGIN;
  mds_kbdc_include_stack_free(our_include_stack);
  FAIL_END;
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
  
  
  last_value_statement = NULL;
  
  
  /* Locate the first character of the primary bound's string. */
  for (lineoff_first = tree->loc_end; code[lineoff_first] == ' '; lineoff_first++);
  /* Locate the first character of the secondary bound's string. */
  for (lineoff_last = lineoff_first + strlen(tree->first); code[lineoff_last] == ' '; lineoff_last++);
  for (lineoff_last += strlen("to"); code[lineoff_last] == ' '; lineoff_last++);
  /* Locate the first character of the select variable. */
  for (lineoff_var = lineoff_last + strlen(tree->variable); code[lineoff_var] == ' '; lineoff_var++);
  for (lineoff_var += strlen("as"); code[lineoff_var] == ' '; lineoff_var++);
  
  /* Duplicate bounds and evaluate function calls,
     variable dereferences and escapes in the bounds. */
  fail_if ((first = parse_string((mds_kbdc_tree_t*)tree, tree->first, lineoff_first), first == NULL));
  fail_if ((last = parse_string((mds_kbdc_tree_t*)tree, tree->last, lineoff_last), last == NULL));
  /* Get the index of the selected variable. */
  fail_if ((variable = parse_variable((mds_kbdc_tree_t*)tree, tree->variable, lineoff_var), variable == 0));
  
  /* Did one of the bound not evaluate, then stop. */
  if (tree->processed == PROCESS_LEVEL)
    goto done;
  
  
  /* Check that the primary bound is single-character. */
  if ((first[0] < 0) || (first[1] >= 0))
    {
      NEW_ERROR(tree, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_first, lineoff_first = 0;
      error->end = error->start + strlen(tree->first);
    }
  /* Check that the secondary bound is single-character. */
  if ((last[0] < 0) || (last[1] >= 0))
    {
      NEW_ERROR(tree, ERROR, "iteration boundary must be a single character string");
      error->start = lineoff_last, lineoff_last = 0;
      error->end = error->start + strlen(tree->last);
    }
  
  /* Was one of the bounds not single-character, then stop. */
  if ((lineoff_first == 0) || (lineoff_last == 0))
    goto done;
  
  
  /* Iterate over the loop for as long as a `return` or `break` has not
     been encountered (without being caught elsewhere). */
  character[1] = -1;
  for (diff = (*first > *last) ? -1 : +1; (*first != *last) && (break_level < 2); *first += diff)
    {
      break_level = 0;
      character[0] = *first;
      fail_if (let(variable, character, NULL, (mds_kbdc_tree_t*)tree, lineoff_var, 1));
      fail_if (compile_subtree(tree->inner));
    }
  
  /* Catch `break` and `continue`, they may not propagate further. */
  if (break_level < 3)
    break_level = 0;
  
  
 done:
  last_value_statement = NULL;
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
  
  last_value_statement = NULL;
  
  /* Locate the first character in the condition. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  /* Evaluate function calls, variable dereferences and escapes in the condition. */
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->condition, lineoff), data == NULL));
  if (tree->processed == PROCESS_LEVEL)
    return free(data), 0;
  
  /* Evaluate whether the evaluted value is true. */
  for (ok = 1, i = 0; data[i] >= 0; i++)
    ok &= !!(data[i]);
  free(data);
  
  /* Compile the appropriate clause. */
  ok = compile_subtree(ok ? tree->inner : tree->otherwise);
  last_value_statement = NULL;
  return ok;
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
  
  /* Get the index of the selected variable. */
  for (lineoff = tree->loc_end; code[lineoff] == ' '; lineoff++);
  fail_if ((variable = parse_variable((mds_kbdc_tree_t*)tree, tree->variable, lineoff), variable == 0));
  if (tree->processed == PROCESS_LEVEL)
    return 0;
  
  /* Duplicate arguments and evaluate function calls,
     variable dereferences and escapes in the value. */
  fail_if ((value = mds_kbdc_tree_dup(tree->value), value == NULL));
  fail_if (compile_subtree(value));
  if ((tree->processed = value->processed) == PROCESS_LEVEL)
    return 0;
  
  /* Set the value of the variable. */
  fail_if (let(variable, NULL, value, NULL, 0, 0));
  
  mds_kbdc_tree_free(value);
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
      node->type = (node->type == C(STRING)) ? C(COMPILED_STRING) : C(COMPILED_KEYS);
      node->compiled_string.string = data;
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
 * Check that a chain of strings and key-combinations
 * does not contain NULL characters
 * 
 * @param   tree  The tree to check
 * @return        Zero on success, -1 on error, 1 if any of
 *                the elements contain a NULL character
 */
static int check_nonnul(mds_kbdc_tree_t* restrict tree)
{
  const char32_t* restrict string;
  int rc = 0;
 again:
  if (tree == NULL)
    return rc;
  
  for (string = tree->compiled_string.string; *string != -1; string++)
    if (*string == 0)
      {
	NEW_ERROR(tree, ERROR, "NULL characters are not allowed in mappings");
	tree->processed = PROCESS_LEVEL;
	rc = 1;
	break;
      }
  
  tree = tree->next;
  goto again;
 pfail:
   return -1;
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
  mds_kbdc_include_stack_t* restrict include_stack = NULL;
  mds_kbdc_tree_t* seq = NULL;
  mds_kbdc_tree_t* res = NULL;
  mds_kbdc_tree_t* old_seq = tree->sequence;
  mds_kbdc_tree_t* old_res = tree->result;
  mds_kbdc_tree_map_t* dup_map = NULL;
  int r, saved_errno;
  mds_kbdc_tree_t* previous_last_value_statement = last_value_statement;
  
  /* Duplicate arguments and evaluate function calls,
     variable dereferences and escapes in the mapping
     input sequence. */
  fail_if ((seq = mds_kbdc_tree_dup(old_seq), seq = NULL));
  fail_if ((bad |= evaluate_element(seq), bad < 0));
  
  /* Duplicate arguments and evaluate function calls,
     variable dereferences and escapes in the mapping
     output sequence, unless this is a value-statement. */
  if (tree->result)
    {
      fail_if ((res = mds_kbdc_tree_dup(old_res), res = NULL));
      fail_if ((bad |= evaluate_element(res), bad < 0));
    }
  
  /* Stop if any of the mapping-arguments could not be evaluated. */
  if (bad)
    goto done;
  
  
  if (tree->result)
    {
      /* Mapping-statement. */
      
      /* Check for invalid characters in the mapping-arguments. */
      fail_if ((bad |= check_nonnul(seq), bad < 0));
      fail_if ((bad |= check_nonnul(res), bad < 0));
      if (bad)
	goto done;
      
      /* Duplicate the mapping-statement but give it the evaluated mapping-arguments. */
      tree->sequence = seq;
      tree->result = res;
      fail_if ((dup_map = &(mds_kbdc_tree_dup((mds_kbdc_tree_t*)tree)->map), dup_map = NULL));
      tree->sequence = old_seq, seq = NULL;
      tree->result = old_res, res = NULL;
      
      /* Enlist the mapping for assembling. */
      fail_if ((include_stack = mds_kbdc_include_stack_save(), include_stack == NULL));
      fail_if (add_mapping(dup_map, include_stack));
      
      goto done;
    }
  
  
  /* Value-statement */
  
  /* Save this statement so we can warn later if it is unnecessary,
   * `set_return_value` will set it to `NULL` if there are side-effects,
   * which would make this statement necessary (unless the overridding
   * statement has identical side-effect, but we will not check for that).
   * For simplicity, we do not store the include-stack, instead, we reset
   * `last_value_statement` to `NULL` when we visit an include-statement. */
  last_value_statement = (mds_kbdc_tree_t*)tree;
  
  /* Add the value statement */
  fail_if ((r = set_return_value(seq->compiled_string.string), r < 0));
  seq->compiled_string.string = NULL;
  
  /* Check that the value-statement is inside a function call, or has
     side-effects by directly or indirectly calling ‘\set/3’ on an
     array that is not shadowed by an inner function- or macro-call. */
  if (r)
    {
      NEW_ERROR(tree, ERROR, "value-statement outside function without side-effects");
      tree->processed = PROCESS_LEVEL;
    }
  
  /* Check whether we made a previous value-statement unnecessary. */
  if (previous_last_value_statement)
    {
      /* For simplicity we set `last_value_statement` on includes,
       * so we are sure `last_value_statement` has the same include-stack. */
      
      NEW_ERROR(previous_last_value_statement, WARNING, "value-statement has no effects");
      NEW_ERROR(tree, NOTE, "overridden here");
    }
  
  
 done:
  mds_kbdc_tree_free(seq);
  mds_kbdc_tree_free(res);
  return 0;
  FAIL_BEGIN;
  mds_kbdc_include_stack_free(include_stack);
  mds_kbdc_tree_free((mds_kbdc_tree_t*)dup_map);
  mds_kbdc_tree_free(seq);
  mds_kbdc_tree_free(res);
  tree->sequence = old_seq;
  tree->result = old_res;
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
  mds_kbdc_tree_macro_t* macro;
  mds_kbdc_include_stack_t* macro_include_stack;
  mds_kbdc_include_stack_t* our_include_stack = NULL;
  size_t variable;
  int bad, saved_errno;
  
  last_value_statement = NULL;
  
  /* Duplicate arguments and evaluate function calls,
     variable dereferences and escapes in the macro
     call arguments. */
  fail_if ((arg = mds_kbdc_tree_dup(tree->arguments), arg = NULL));
  fail_if ((bad = evaluate_element(arg), bad < 0));
  if (bad)
    return 0;
  
  /* Get the macro's subtree and include-stack, if it has
     not been defined `get_macro` will add an error message
     and return `NULL`. */
  fail_if (get_macro(tree, &macro, &macro_include_stack));
  if (macro == NULL)
    goto done;
  
  
  /* Push call stack and set parameters. */
  variables_stack_push();
  for (arg_ = arg; arg_; arg_ = arg_->next)
    fail_if (let(variable, NULL, arg_, NULL, 0, 0));
  
  /* Switch include-stack to the macro's. */
  fail_if ((our_include_stack = mds_kbdc_include_stack_save(), our_include_stack == NULL));
  fail_if (mds_kbdc_include_stack_restore(macro_include_stack));
  
  /* Call the macro. */
  fail_if (compile_subtree(macro->inner));
  
  /* Switch back the include-stack to ours. */
  fail_if (mds_kbdc_include_stack_restore(our_include_stack));
  mds_kbdc_include_stack_free(our_include_stack), our_include_stack = NULL;
  
  /* Pop call stack. */
  variables_stack_pop();
  
  
 done:
  last_value_statement = NULL;
  break_level = 0;
  mds_kbdc_tree_free(arg);
  return 0;
  FAIL_BEGIN;
  mds_kbdc_tree_free(arg);
  mds_kbdc_include_stack_free(our_include_stack);
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
    /* An error has occurred here before, lets skip it so
     * we do not deluge the user with errors. */
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
    /* If a `continue`, `break` or `return` has been encountered,
     * we are done here and should return to whence we came and
     * let the subcompiler of that construct deal with it. */
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
  int r, saved_errno;
  mds_kbdc_include_stack_begin(result = result_);
  r = compile_subtree(result_->tree);
  saved_errno = errno;
  mds_kbdc_include_stack_end();
  variables_terminate();
  callables_terminate();
  errno = saved_errno;
  return r;
}



#undef FAIL_END
#undef FAIL_BEGIN
#undef NEW_ERROR
#undef C
#undef PROCESS_LEVEL

