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
#include "util.h"
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>


/**
 * Read an environment variable, but handle it as undefined if empty
 * 
 * @param   var  The environment variable's name
 * @return       The environment variable's value, `NULL` if empty or not defined
 */
char* getenv_nonempty(const char* var)
{
  char* rc = getenv(var);
  if ((rc == NULL) || (*rc == '\0'))
    return NULL;
  return rc;
}


/**
 * Re-exec the server.
 * This function only returns on failure.
 * 
 * @param  argc      The number of elements in `argv`
 * @param  argv      The command line arguments
 * @param  reexeced  Whether the server has previously been re-exec:ed
 */
void reexec_server(int argc, char** argv, int reexeced)
{
  char readlink_buf[PATH_MAX];
  ssize_t readlink_ptr;
  char** reexec_args;
  char** reexec_args_;
  int i;
  
  /* Re-exec the server. */
  readlink_ptr = readlink(SELF_EXE, readlink_buf, (sizeof(readlink_buf) / sizeof(char)) - 1);
  if (readlink_ptr < 0)
    return;
  /* ‘readlink() does not append a null byte to buf.’ */
  readlink_buf[readlink_ptr] = '\0';
  reexec_args = alloca(((size_t)argc + 2) * sizeof(char*));
  reexec_args_ = reexec_args;
  if (reexeced == 0)
    {
      *reexec_args_++ = *argv;
      *reexec_args_ = strdup("--re-exec");
      if (*reexec_args_)
	return;
      for (i = 1; i < argc; i++)
	reexec_args_[i] = argv[i];
    }
  else /* Don't let the --re-exec:s accumulate. */
    *reexec_args_ = *argv;
  for (i = 1; i < argc; i++)
    reexec_args_[i] = argv[i];
  reexec_args_[argc] = NULL;
  execv(readlink_buf, reexec_args);
}

