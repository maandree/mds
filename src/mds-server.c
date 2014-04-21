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
#include "mds-server.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>


/**
 * Number of elements in `argv`
 */
static int argc;

/**
 * Command line arguments
 */
static char** argv;


/**
 * Entry point of the server
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  char* unparsed_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  int i;
  int is_respawn = -1;
  int socket_fd = -1;
  int unparsed_args_ptr = 0;
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Drop privileges like it's hot. */
  if ((geteuid() == getuid() ? 0 : seteuid(getuid())) ||
      (getegid() == getgid() ? 0 : setegid(getgid())))
    {
      perror(*argv);
      return 1;
    }
  
  
  /* Sanity check the number of command line arguments. */
  if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT)
    {
      fprintf(stderr,
	      "%s: that number of arguments is ridiculous, I will not allow it.\n",
	      *argv);
      return 1;
    }
  
  
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      if (!strcmp(arg, "--initial-spawn")) /* Initial spawn? */
	if (is_respawn == 1)
	  {
	    fprintf(stderr,
		    "%s: conflicting arguments %s and %s cannot be combined.\n",
		    *argv, "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 0;
      else if (!strcmp(arg, "--respawn")) /* Respawning after crash? */
	if (is_respawn == 0)
	  {
	    fprintf(stderr,
		    "%s: conflicting arguments %s and %s cannot be combined.\n",
		    *argv, "--initial-spawn", "--respawn");
	    return 1;
	  }
	else
	  is_respawn = 1;
      else if (strstr(arg, "--socket-fd=") == arg) /* Socket file descriptor. */
	{
	  long int r;
	  char* endptr;
	  if (socket_fd != -1)
	    {
	      fprintf(stderr, "%s: duplicate declaration of %s.\n", *argv, "--socket-fd");
	      return -1;
	    }
	  arg += strlen("--socket-fd=");
	  r = strtol(arg, &endptr, 10);
	  if ((*argv == '\0') || isspace(*argv) ||
	      (endptr - arg != (ssize_t)strlen(arg))
	      || (r < 0) || (r > INT_MAX))
	    {
	      fprintf(stderr, "%s: invalid value for %s: %s.\n", *argv, "--socket-fd", arg);
	      return 1;
	    }
	  socket_fd = (int)r;
	}
      else
	/* Not recognised, it is probably for another server. */
	unparsed_args[unparsed_args_ptr++] = arg;
    }
  unparsed_args[unparsed_args_ptr] = NULL;
  
  
  /* Check that manditory arguments have been specified. */
  if (is_respawn < 0)
    {
      fprintf(stderr,
	      "%s: missing state argument, require either %s or %s.\n",
	      *argv, "--initial-spawn", "--respawn");
      return 1;
    }
  if (socket_fd < 0)
    {
      fprintf(stderr, "%s: missing socket file descriptor argument.\n", *argv);
      return 1;
    }
  
  
  return 0;
}

