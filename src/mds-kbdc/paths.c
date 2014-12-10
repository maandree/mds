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
#include "paths.h"

#include <libmdsserver/macros.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



/**
 * Get the current working directory
 * 
 * @return  The current working directory
 */
char* curpath(void)
{
  static size_t cwd_size = 4096;
  char* cwd = NULL;
  char* old = NULL;
  int saved_errno;
  
  /* glibc offers ways to do this in just one function call,
   * but we will not assume that glibc is used here. */
  for (;;)
    {
      fail_if (xxrealloc(old, cwd, cwd_size + 1, char));
      if (getcwd(cwd, cwd_size))
	break;
      else
	fail_if (errno != ERANGE);
      cwd_size <<= 1;
    }
  
  return cwd;
 fail:
  saved_errno = errno;
  free(old);
  free(cwd);
  errno = saved_errno;
  return NULL;
}


/**
 * Get the absolute path of a file
 * 
 * @param   path  The filename of the file
 * @return        The file's absolute path, `NULL` on error
 */
char* abspath(const char* path)
{
  char* cwd = NULL;
  char* buf = NULL;
  int saved_errno, slash = 1;
  size_t size, p;
  
  if (*path == '/')
    {
      fail_if (xstrdup(buf, path));
      return buf;
    }
  
  fail_if (cwd = curpath(), cwd == NULL);
  size = (p = strlen(cwd)) + strlen(path) + 2;
  fail_if (xmalloc(buf, size + 1, char));
  memcpy(buf, cwd, (p + 1) * sizeof(char));
  if (buf[p - 1] != '/')
    buf[p++] = '/';
  
  while (*path)
    if (slash && (path[0] == '/'))
      path += 1;
    else if (slash && (path[0] == '.') && (path[1] == '/'))
      path += 2;
    else if (slash && (path[0] == '.') && (path[1] == '.') && (path[2] == '/'))
      {
	path += 3;
	p--;
	while (p && (buf[--p] != '/'));
	p++;
      }
    else
      {
	buf[p++] = *path;
	slash = (*path++ == '/');
      }
  
  buf[p] = '\0';
  
  free(cwd);
  return buf;
 fail:
  saved_errno = errno;
  free(cwd);
  errno = saved_errno;
  return NULL;
}


/**
 * Get a relative path of a file
 * 
 * @param   path  The filename of the file
 * @param   base  The pathname of the base directory,
 *                `NULL` for the current working directroy
 * @return        The file's relative path, `NULL` on error
 */
char* relpath(const char* path, const char* base)
{
  char* abs = abspath(path);
  char* absbase = NULL;
  char* buf = NULL;
  char* old = NULL;
  int saved_errno;
  size_t p, slash = 1, back = 0;
  
  fail_if (abs == NULL);
  absbase = base ? abspath(base) : curpath();
  fail_if (absbase == NULL);
  
  if (absbase[strlen(absbase) - 1] != '/')
    /* Both `abspath` and `curpath` (and `relpath`) allocates one extra slot. */
    absbase[strlen(absbase) + 1] = '\0', absbase[strlen(absbase)] = '/';
  
  for (p = 1; abs[p] && absbase[p] && (abs[p] == absbase[p]); p++)
    if (abs[p] == '/')
      slash = p + 1;
  
  for (p = slash; absbase[p]; p++)
    if (absbase[p] == '/')
      back++;
  
  fail_if (xmalloc(buf, back * 3 + strlen(abs + slash) + 2, char));
  
  for (p = 0; back--;)
    buf[p++] = '.', buf[p++] = '.', buf[p++] = '/';
  memcpy(buf + p, abs + slash, (strlen(abs + slash) + 1) * sizeof(char));
  
  free(abs);
  free(absbase);
  return buf;
 fail:
  saved_errno = errno;
  free(abs);
  free(absbase);
  free(old);
  errno = saved_errno;
  return NULL;
}

