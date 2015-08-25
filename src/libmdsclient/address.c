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

#include <sys/un.h>
#include <limits.h>



/**
 * Parse a display address string
 * 
 * @param   display  The address in MDS_DISPLAY-formatting, must not be `NULL`
 * @param   address  Output parameter for parsed address, must not be `NULL`
 * @return           Zero on success, even if parsing failed, -1 on error,
 *                   `errno` will have been set accordinly on error
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the application hit the
 *                  RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 */
int libmds_parse_display_adress(const char* restrict display, libmds_display_address_t* restrict address)
{
  address->domain = -1;
  address->type = -1;
  address->protocol = -1;
  address->address = NULL;
  address->address_len = 0;
  
  return (void) display, 0; /* TODO */
}

