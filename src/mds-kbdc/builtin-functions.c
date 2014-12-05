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
  if (rc == NULL)						\
    return NULL;						\
  rc[n] = -1

/**
 * Define useful data for built-in function with 1 parameter
 */
#define define_1						\
  const char32_t* restrict a = *args++;				\
  size_t i, n = string_length(a);				\
  char32_t* restrict rc = malloc((n + 1) * sizeof(char32_t));	\
  if (rc == NULL)						\
    return NULL;						\
  rc[n] = -1


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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
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
  return rc;
}


/* static char32_t* builtin_function_set_3(const char32_t** restrict args);  (variable index value) */
/* static char32_t* builtin_function_get_2(const char32_t** restrict args);  (variable index) */


#undef define_1
#undef define_2


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
 * @param   name       The name of the function
 * @param   arg_count  The number of arguments to pass to the function
 * @param   args       The arguments to pass
 * @return             The return value of the function, `NULL` on error
 */
char32_t* builtin_function_invoke(const char* restrict name, size_t arg_count, const char32_t** restrict args)
{
  if (arg_count == 3)
    if (!strcmp(name, "set"))
      return NULL; /* TODO builtin_function_set_3(args) */
  
  if (arg_count == 1)
    if (!strcmp(name, "not"))
      return builtin_function_not_1(args);
  
  if (arg_count != 2)
    return NULL;
  
  if (!strcmp(name, "add"))      return builtin_function_add_2(args);
  if (!strcmp(name, "sub"))      return builtin_function_sub_2(args);
  if (!strcmp(name, "mul"))      return builtin_function_mul_2(args);
  if (!strcmp(name, "div"))      return builtin_function_div_2(args);
  if (!strcmp(name, "mod"))      return builtin_function_mod_2(args);
  if (!strcmp(name, "rsh"))      return builtin_function_rsh_2(args);
  if (!strcmp(name, "lsh"))      return builtin_function_lsh_2(args);
  if (!strcmp(name, "or"))       return builtin_function_or_2(args);
  if (!strcmp(name, "and"))      return builtin_function_and_2(args);
  if (!strcmp(name, "xor"))      return builtin_function_xor_2(args);
  if (!strcmp(name, "equals"))   return builtin_function_equals_2(args);
  if (!strcmp(name, "greater"))  return builtin_function_greater_2(args);
  if (!strcmp(name, "less"))     return builtin_function_less_2(args);
  if (!strcmp(name, "get"))      return NULL; /* TODO builtin_function_get_2(args) */
  
  return NULL;
}

