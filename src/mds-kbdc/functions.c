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
#include "functions.h"

#include <stdlib.h>



/**
 * Define useful data for built-in function with 2 parameters.
 */
#define define_2						\
  const char32_t* restrict a = va_arg(args, const char32_t*);	\
  const char32_t* restrict b = va_arg(args, const char32_t*);	\
  size_t an = string_length(a);					\
  size_t bn = string_length(b);					\
  size_t i, n = an > bn ? an : bn;				\
  char32_t* restrict rc = malloc((n + 1) * sizeof(char32_t));	\
  if (rc == NULL)						\
    return NULL;						\
  rc[n] = -1

/**
 * Define useful data for built-in function with 1 parameter.
 */
#define define_1						\
  const char32_t* restrict a = va_arg(args, const char32_t*);	\
  size_t i, n = string_length(a);				\
  char32_t* restrict rc = malloc((n + 1) * sizeof(char32_t));	\
  if (rc == NULL)						\
    return NULL;						\
  rc[n] = -1


/**
 * Definition of the built-in function add/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_add_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] + b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function sub/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_sub_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] - b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function mul/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_mul_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] * b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function div/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_div_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] / b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function mod/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_mod_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] % b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function rsh/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_rsh_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] << b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function lsh/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_lsh_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] >> b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function or/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_or_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] | b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function and/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_and_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] & b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function xor/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_xor_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] ^ b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function not/1.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_not_1(va_list args)
{
  define_1;
  for (i = 0; i < n; i++)
    rc[i] = !a[i];
  return rc;
}


/**
 * Definition of the built-in function equals/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_equals_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] == b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function greater/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_greater_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] > b[i % bn];
  return rc;
}


/**
 * Definition of the built-in function less/2.
 * 
 * @param   args  The arguments passed to the function.
 * @return        The return value of the function, `NULL` on error.
 */
static char32_t* function_builtin_less_2(va_list args)
{
  define_2;
  for (i = 0; i < n; i++)
    rc[i] = a[i % an] < b[i % bn];
  return rc;
}


/* static char32_t* function_builtin_set_2(va_list args);  (variable index value) */
/* static char32_t* function_builtin_get_2(va_list args);  (variable index) */


#undef define_1
#undef define_2


/**
 * Check whether a function is defined.
 * 
 * @param   name       The name of the function.
 * @param   arg_count  The number of arguments to pass to the function.
 * @return             Whether the function is defined for the selected number of arguments.
 */
int function_check_defined(const char32_t* restrict name, size_t arg_count)
{
  return 0; /* TODO */
}


/**
 * Invoke a function defined in the keyboard layout source code, or that is builtin.
 * 
 * @param   name           The name of the function.
 * @param   arg_count      The number of arguments to pass to the function.
 * @param   ...:char32_t*  The arguments to pass, do not end with a sentinel.
 * @return                 The return value of the function, `NULL` on error.
 */
char32_t* function_invoke(const char32_t* restrict name, size_t arg_count, ...)
{
  return NULL; /* TODO */
}

