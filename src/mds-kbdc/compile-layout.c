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
 * Compile a subtree
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_subtree(mds_kbdc_tree_t* restrict tree);



static char32_t* parse_string(mds_kbdc_tree_t* restrict tree, const char* restrict raw, size_t lineoff)
{
  (void) tree;
  (void) raw;
  (void) lineoff;
  return NULL; /* TODO */
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
  size_t lineoff = tree->loc_end;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char** old = NULL;
  int saved_errno;
  
  while (code[lineoff] == ' ')
    lineoff++;
  
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
  size_t lineoff = tree->loc_end;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char** old = NULL;
  int saved_errno;
  
  while (code[lineoff] == ' ')
    lineoff++;
  
  if (result->countries_ptr == result->countries_size)
    {
      result->countries_size = result->countries_size ? (result->countries_size << 1) : 1;
      fail_if (xxrealloc(old, result->countries, result->countries_size, char*));
    }
  
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->data, lineoff), data == NULL));
  fail_if ((code = string_encode(data), code == NULL));
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
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a have-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_have(mds_kbdc_tree_assumption_have_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a have_chars-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_have_chars(mds_kbdc_tree_assumption_have_chars_t* restrict tree)
{
  size_t lineoff = tree->loc_end;
  char* restrict code = result->source_code->real_lines[tree->loc_line];
  char32_t* restrict data = NULL;
  char** old = NULL;
  char32_t** old32 = NULL;
  char32_t* restrict character;
  size_t n;
  int saved_errno;
  
  while (code[lineoff] == ' ')
    lineoff++;
  
  fail_if ((data = parse_string((mds_kbdc_tree_t*)tree, tree->chars, lineoff), data == NULL));
  for (n = 0; data[n] != -1; n++);
  
  if (result->assumed_strings_ptr + n >= result->assumed_strings_size)
    {
      result->assumed_strings_size += n;
      fail_if (xxrealloc(old32, result->assumed_strings, result->assumed_strings_size, char*));
    }
  
  while (n--)
    {
      fail_if (xmalloc(character, 2, char32_t));
      character[0] = data[n];
      character[1] = -1;
      result->assumed_strings[result->assumed_strings_ptr++] = character;
    }
  
  return 0;
  FAIL_BEGIN;
  free(data);
  free(old);
  free(old32);
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
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a function
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_function(mds_kbdc_tree_function_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a macro
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_macro(mds_kbdc_tree_macro_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a for-loop
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_for(mds_kbdc_tree_for_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile an if-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_if(mds_kbdc_tree_if_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a let-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_let(mds_kbdc_tree_let_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a mapping- or value-statement
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_map(mds_kbdc_tree_map_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
}


/**
 * Compile a macro call
 * 
 * @param   tree  The tree to compile
 * @return        Zero on success, -1 on error
 */
static int compile_macro_call(mds_kbdc_tree_macro_call_t* restrict tree)
{
  (void) tree;
  return 0; /* TODO */
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
    case C(MAP):                    c (map);          break;
    case C(MACRO_CALL):             c (macro_call);   break;
    case C(RETURN):                 break_level = 3;  break;
    case C(BREAK):                  break_level = 2;  break;
    case C(CONTINUE):               break_level = 1;  break;
    default:
      break;
    }
  
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

