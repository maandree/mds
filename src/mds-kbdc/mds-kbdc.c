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
#include "mds-kbdc.h"

#include "globals.h"
#include "raw-data.h"

#include <libmdsserver/macros.h>

#include <stdio.h>
#include <errno.h>



/**
 * Compile a keyboard layout file
 * 
 * @param   argc_  The number of elements in `argv_`
 * @param   argv_  The command line arguments
 * @return         Zero on and only one success
 */
int main(int argc_, char** argv_)
{
  const char* pathname = argv_[1];
  source_code_t source_code;
  
  argc = argc_;
  argv = argv_;
  
  source_code_initialise(&source_code);
  fail_if (read_source_lines(pathname, &source_code) < 0);
  
  return 0;
  
 pfail:
  xperror(*argv);
  source_code_destroy(&source_code);
  return 1;
}

