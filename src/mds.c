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
#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>


/**
 * Entry point of the program
 * 
 * @param   argc  Number of elements in `argv`
 * @param   argv  Command line arguments
 * @return        Non-zero on error
 */
int main(int argc, const char** argv)
{
  struct stat attr;
  
  (void) argv;
  
  /* Sanity check the number of command line arguments. */
  if (argc > 50)
    {
      fprintf(stderr,
	      "%s: that number of arguments is ridiculous, I will not allow it.\n",
	      *argv);
      return 1;
    }
  
  /* Stymied if the effective user is not root. */
  if (geteuid() != ROOT_USER_UID)
    {
      fprintf(stderr,
	      "%s: the effective user is not root, cannot continue.\n",
	      *argv);
      return 1;
    }
  
  /* Create directory for socket files, PID files and such. */
  if (stat(MDS_RUNTIME_ROOT_DIRECTORY, &attr) == 0)
    {
      /* Cannot create the directory, its pathname refers to an existing. */
      if (S_ISDIR(attr.st_mode) == 0)
	{
	  /* But it is not a directory so we cannot continue. */
	  fprintf(stderr,
		  "%s: %s already exists but is not a directory.\n",
		  MDS_RUNTIME_ROOT_DIRECTORY, *argv);
	  return 1;
	}
    }
  else
    /* Directory is missing, create it. */
    if (mkdir(MDS_RUNTIME_ROOT_DIRECTORY, 0755) < 0)
      if (errno != EEXIST) /* Unlikely race condition. */
	{
	  perror(*argv);
	  return 1;
	}
  
  /* Drop privileges. They most not be propagated non-authorised components. */
  if (seteuid(getuid()) < 0)
    {
      perror(*argv);
      return 1;
    }
  
  return 0;
}

