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
#ifndef MDS_LIBMDSSERVER_MACRO_BITS_H
#define MDS_LIBMDSSERVER_MACRO_BITS_H


#include <limits.h>
#include <stdint.h>
#include <stddef.h>



/**
 * Convert the beginning of a `const char*` to a `size_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atoz(str) ((size_t)atol(str))

/**
 * Convert the beginning of a `const char*` to an `ssize_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atosz(str) ((ssize_t)atol(str))

/**
 * Convert the beginning of a `const char*` to a `short int`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atoh(str) ((short)atol(str))

/**
 * Convert the beginning of a `const char*` to an `unsigned short int`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atouh(str) ((unsigned short)atol(str))

/**
 * Convert the beginning of a `const char*` to an `unsigned int`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atou(str) ((unsigned int)atoi(str))

/**
 * Convert the beginning of a `const char*` to an `unsigned long int`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atoul(str) ((unsigned long)atol(str))

/**
 * Convert the beginning of a `const char*` to an `unsigned long long int`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atoull(str) ((unsigned long long)atoll(str))

/**
 * Convert the beginning of a `const char*` to an `int8_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define ato8(str) ((int8_t)atoi(str))

/**
 * Convert the beginning of a `const char*` to an `uint8_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atou8(str) ((uint8_t)atou(str))

/**
 * Convert the beginning of a `const char*` to an `int16_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define ato16(str) ((int16_t)atoi(str))

/**
 * Convert the beginning of a `const char*` to an `uint16_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atou16(str) ((uint16_t)atou(str))

#if UINT_MAX == UINT32_MAX
/**
 * Convert the beginning of a `const char*` to an `int32_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define ato32(str) ((int32_t)atoi(str))

/**
 * Convert the beginning of a `const char*` to an `uint32_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define atou32(str) ((uint32_t)atou(str))
#else
/**
 * Convert the beginning of a `const char*` to an `int32_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define ato32(str) ((int32_t)atol(str))

/**
 * Convert the beginning of a `const char*` to an `uint32_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define atou32(str) ((uint32_t)atoul(str))
#endif

#if ULONG_MAX == UINT64_MAX
/**
 * Convert the beginning of a `const char*` to an `int64_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define ato64(str) ((int64_t)atol(str))

/**
 * Convert the beginning of a `const char*` to an `uint64_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define atou64(str) ((uint64_t)atoul(str))
#else
/**
 * Convert the beginning of a `const char*` to an `int64_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define ato64(str) ((int64_t)atoll(str))

/**
 * Convert the beginning of a `const char*` to an `uint64_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
# define atou64(str) ((uint64_t)atoull(str))
#endif

/**
 * Convert the beginning of a `const char*` to an `intmax_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atoj(str) ((intmax_t)atou64(str))

/**
 * Convert the beginning of a `const char*` to an `uintmax_t`
 * 
 * @param   str:const char*  The string that begins with an integer
 * @return                   The integer at the beginning of the string
 */
#define atouj(str) ((uintmax_t)atou64(str))


#endif
