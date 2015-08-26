/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
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
#include "address.h"

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include <libmdsserver/config.h>



/**
 * Check whether a string is a radix-10 non-negative integer
 * 
 * @param   str  The string
 * @return       Whether a string is a radix-10 non-negative integer
 */
__attribute__((nonnull))
static int is_pzinteger(const char* restrict str)
{
  for (; *str; str++)
    if (('0' > *str) || (*str > '9'))
      return 0;
  return 1;
}


/**
 * Set the socket adress, with the address family `AF_UNIX`
 * 
 * @param   out_address  Output paramter for the socket address
 * @param   pathlen      Pointer to a variable where the length of the pathname will be stored
 * @param   format       Formatting string for the pathname (should end with "%zn")
 * @param   ...          Formatting arguments for the pathname (should end with `pathlen`)
 * @return               Zero on success, -1 on error, `errno`
 *                       will have been set accordinly on error
 * 
 * @throws  ENOMEM        Out of memory. Possibly, the application hit the
 *                        RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 */
__attribute__((nonnull, format(gnu_printf, 3, 4)))
static int set_af_unix(struct sockaddr** restrict out_address, ssize_t* pathlen, const char* format, ...)
#define set_af_unix(a, f, ...)  ((set_af_unix)(a, &pathlen, f "%zn", __VA_ARGS__, &pathlen))
{
  struct sockaddr_un* address;
  size_t maxlen;
  va_list args;
  
  address = malloc(sizeof(struct sockaddr_un));
  *out_address = (struct sockaddr*)address;
  if (address == NULL)
    return -1;
  
  maxlen = sizeof(address->sun_path) / sizeof(address->sun_path[0]);
  
  va_start(args, format);
  
  address->sun_family = AF_UNIX;
  vsnprintf(address->sun_path, maxlen, format, args);
  
  va_end(args);
  
  if ((size_t)*pathlen > maxlen)
    return errno = ENAMETOOLONG, -1;
  
  return 0;
}


/**
 * Parse a display address string
 * 
 * @param   display  The address in MDS_DISPLAY-formatting, must not be `NULL`
 * @param   address  Output parameter for parsed address, must not be `NULL`
 * @return           Zero on success, even if parsing failed, -1 on error,
 *                   `errno` will have been set accordinly on error
 * 
 * @throws  ENOMEM        Out of memory. Possibly, the application hit the
 *                        RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 */
int libmds_parse_display_adress(const char* restrict display, libmds_display_address_t* restrict address)
{
  ssize_t pathlen;
  
  address->domain = -1;
  address->type = -1;
  address->protocol = -1;
  address->address = NULL;
  address->address_len = 0;
  
  if (strchr(display, ':') == NULL)
    return 0;
  
  if (*display == ':')
    {
      address->domain = PF_UNIX;
      address->type = SOCK_STREAM;
      address->protocol = 0;
      address->address_len = sizeof(struct sockaddr_un);
      
      if (strstr(display, ":file:") == display)
	return set_af_unix(&(address->address), "%s", display + 6);
      else if (display[1] && is_pzinteger(display + 1))
	return set_af_unix(&(address->address), "%s/%s.socket",
			   MDS_RUNTIME_ROOT_DIRECTORY, display + 1);
      return 0;
    }
  
  return 0;
}

