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
#ifndef MDS_LIBMDSCLIENT_ADDRESS_H
#define MDS_LIBMDSCLIENT_ADDRESS_H


#include <sys/socket.h>


/**
 * The address of the display, parsed into arguments
 */
typedef struct libmds_display_address
{
  /**
   * The domain (protocol family), that is
   * the first argument for socket(2), a
   * value whose constant is prefixed PF_;
   * -1 if not detected
   */
  int domain;
  
  /**
   * The socket type, that is the second
   * argument for socket(2), a value whose
   * constant is prefixed SOCK_; -1 if not
   * detected
   */
  int type;
  
  /**
   * The protocol, that is the third
   * argument for socket(2), a value whose
   * constant is prefixed IPPROTO_ (zero
   * for the default); -1 if not detected
   */
  int protocol;
  
  /**
   * The address, `NULL` if not detected,
   * you are responsible for freeing this
   */
  struct sockaddr* address;
  
  /**
   * The size of `address`, may be set
   * even if `address` is `NULL`
   */
  socklen_t address_len;
  
  /**
   * Code for an error that has occured
   * when parsing the address, whose
   * description can be retrieved using
   * `gia_strerror`, zero if none
   */
  int gai_error;
  
} libmds_display_address_t;


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
__attribute__((nonnull))
int libmds_parse_display_adress(const char* restrict display, libmds_display_address_t* restrict address);


#endif

