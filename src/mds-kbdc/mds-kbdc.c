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
#include "make-tree.h"

#include <libmdsserver/macros.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>



/**
 * Compile a keyboard layout file
 * 
 * @param   argc_  The number of elements in `argv_`
 * @param   argv_  The command line arguments
 * @return         Zero on and only one success
 */
int main(int argc_, char** argv_)
{
  mds_kbdc_parse_error_t** parse_errors;
  mds_kbdc_tree_t* tree;
  
  argc = argc_;
  argv = argv_;
  
  fail_if (parse_to_tree(argv[1], &tree, &parse_errors) < 0);
  mds_kbdc_tree_print(tree, stderr);
  if (parse_errors != NULL)
    {
      mds_kbdc_parse_error_t** errors = parse_errors;
      int fatal = 0;
      while (*errors)
	{
	  if ((*errors)->severity >= MDS_KBDC_PARSE_ERROR_ERROR)
	    fatal = 1;
	  mds_kbdc_parse_error_print(*errors++, stderr);
	}
      mds_kbdc_parse_error_free_all(parse_errors);
      if (fatal)
	return mds_kbdc_tree_free(tree), 1;
    }
  
  mds_kbdc_tree_free(tree);
  return 0;
  
 pfail:
  xperror(*argv);
  return 1;
}

