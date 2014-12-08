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
#include "builtin-functions.h"

#include "variables.h"

#include <libmdsserver/macros.h>

#include <stdlib.h>
#include <string.h>



/**
 * Define useful data for built-in function with 2 parameters
 */
#define define_2						\
  const char32_t* restrict a = *args++;				\
  const char32_t* restrict b = *args++;				\
  size_t an = string_length(a);					\
  size_t bn = string_length(b);					\
  size_t i, n = an > bn ? an : bn;				\
  char32_t* restrict rc = malloc((n + 1) * sizeof(char32_t));	\
  fail_if (rc == NULL);						\
  rc[n] = -1

/**
 * Define useful data for built-in function with 1 parameter
 */
#define define_1						\
  const char32_t* restrict a = *args++;				\
  size_t i, n = string_length(a);				\
  char32_t* restrict rc = malloc((n + 1) * sizeof(char32_t));	\
  fail_if (rc == NULL);						\
  rc[n] = -1

/**
 * Return a value, or if there was a failure somewhere, return `NULL`
 * 
 * @param  v:void*  The value to return on success
 */
#define return(v)				\
  return v;					\
 fail:						\
  return NULL;


/**
 * Definition of the built-in function add/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_add_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] + b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function sub/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_sub_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] - b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function mul/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_mul_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] * b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function div/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_div_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] / b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function mod/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_mod_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] % b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function rsh/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_rsh_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] << b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function lsh/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_lsh_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] >> b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function or/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_or_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] | b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function and/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_and_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] & b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function xor/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_xor_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] ^ b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function not/1
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_not_1(const char32_t** restrict args)
{
  define_1;
  for (i = 0; i < n; i++)
    rc[i] = !a[i];
  return(rc);
}


/**
 * Definition of the built-in function equals/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_equals_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] == b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function greater/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_greater_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] > b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function less/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_less_2(const char32_t** restrict args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] < b[i % bn];
  return(rc);
}


/**
 * Definition of the built-in function get/2
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_get_2(const char32_t** restrict args)
{
  const char32_t* restrict a = *args++;
  const char32_t* restrict b = *args++;
  char32_t* restrict rc;
  size_t n = (size_t)*b;
  mds_kbdc_tree_t* value = variables_get((size_t)*a);
  while (n--)
    value = value->next;
  fail_if (rc = string_dup(value->compiled_string.string), rc == NULL);
  return(rc);
}


/**
 * Definition of the built-in function set/3
 * 
 * @param   args  The arguments passed to the function
 * @return        The return value of the function, `NULL` on error
 */
static char32_t* builtin_function_set_3(const char32_t** restrict args)
{
  const char32_t* restrict a = *args++;
  const char32_t* restrict b = *args++;
  const char32_t* restrict c = *args++;
  char32_t* restrict rc;
  size_t n = (size_t)*b;
  mds_kbdc_tree_t* value = variables_get((size_t)*a);
  while (n--)
    value = value->next;
  free(value->compiled_string.string);
  value->compiled_string.string = string_dup(c);
  fail_if (value->compiled_string.string == NULL);
  fail_if (rc = string_dup(c), rc == NULL);
  return(rc);
}


#undef define_1
#undef define_2
#undef return


/**
 * Check whether a function is a builtin function
 * 
 * @param   name       The name of the function
 * @param   arg_count  The number of arguments to pass to the function
 * @return             Whether the described function is a builtin function
 */
int builtin_function_defined(const char* restrict name, size_t arg_count)
{
  size_t i;
  static const char* const BUILTIN_FUNCTIONS_2[] =
    {
      "add", "sub", "mul", "div", "mod", "rsh", "lsh", "or",
      "and", "xor", "equals", "greater", "less", "get", NULL
    };
  
  if (arg_count == 3)
    return !strcmp(name, "set");
  else if (arg_count == 1)
    return !strcmp(name, "not");
  else if (arg_count == 2)
    for (i = 0; BUILTIN_FUNCTIONS_2[i]; i++)
      if (!strcmp(name, BUILTIN_FUNCTIONS_2[i]))
	return 1;
  
  return 0;
}


/**
 * Invoke a builtin function
 * 
 * The function will abort if an non-builtin function is addressed
 * 
 * Before invoking set/3 or get/2, please check the arguments,
 * please also check that either all or none of the arguments
 * are empty string for any of the other builtin functions
 * 
 * @param   name       The name of the function
 * @param   arg_count  The number of arguments to pass to the function
 * @param   args       The arguments to pass
 * @return             The return value of the function, `NULL` on error
 */
char32_t* builtin_function_invoke(const char* restrict name, size_t arg_count, const char32_t** restrict args)
{
#define t(f)  do { fail_if (rc = builtin_function_##f(args), rc == NULL); return rc; } while (0)
  char32_t* rc;
  
  if ((arg_count == 3) && !strcmp(name, "set"))  t (set_3);
  if ((arg_count == 1) && !strcmp(name, "not"))  t (not_1);
  
  if (arg_count != 2)
    abort();
  
  if (!strcmp(name, "add"))      t (add_2);
  if (!strcmp(name, "sub"))      t (sub_2);
  if (!strcmp(name, "mul"))      t (mul_2);
  if (!strcmp(name, "div"))      t (div_2);
  if (!strcmp(name, "mod"))      t (mod_2);
  if (!strcmp(name, "rsh"))      t (rsh_2);
  if (!strcmp(name, "lsh"))      t (lsh_2);
  if (!strcmp(name, "or"))       t (or_2);
  if (!strcmp(name, "and"))      t (and_2);
  if (!strcmp(name, "xor"))      t (xor_2);
  if (!strcmp(name, "equals"))   t (equals_2);
  if (!strcmp(name, "greater"))  t (greater_2);
  if (!strcmp(name, "less"))     t (less_2);
  if (!strcmp(name, "get"))      t (get_2);
  
  abort();
 fail:
  return NULL;
#undef t
}

