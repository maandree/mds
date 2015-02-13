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
#include "parse-error.h"

#include "paths.h"

#include <stdlib.h>
#include <alloca.h>
#include <string.h>



/**
 * Print information about a parsing error
 * 
 * @param  this    The error structure
 * @param  output  The output file
 * @param  desc    The description of the error
 */
static void print(const mds_kbdc_parse_error_t* restrict this, FILE* restrict output, const char* restrict desc)
{
  size_t i, n, start = 0, end = 0;
  const char* restrict code = this->code;
  char* restrict path = relpath(this->pathname, NULL);
  
  /* Convert bytes count to character count for the code position. */
  for (i = 0, n = this->start; i < n; i++)
    if ((code[i] & 0xC0) != 0x80)
      start++;
  for (n = this->end; i < n; i++)
    if ((code[i] & 0xC0) != 0x80)
      end++;
  end += start;
  
  /* Print error information. */
  fprintf(output, "\033[01m%s\033[21m:", path ? path : this->pathname);
  free(path);
  if (this->error_is_in_file)
    fprintf(output, "%zu:%zu–%zu:", this->line + 1, start, end);
  switch (this->severity)
    {
    case MDS_KBDC_PARSE_ERROR_NOTE:            fprintf(output, " \033[01;36mnote:\033[00m ");            break;
    case MDS_KBDC_PARSE_ERROR_WARNING:         fprintf(output, " \033[01;35mwarning:\033[00m ");         break;
    case MDS_KBDC_PARSE_ERROR_ERROR:           fprintf(output, " \033[01;31merror:\033[00m ");           break;
    case MDS_KBDC_PARSE_ERROR_INTERNAL_ERROR:  fprintf(output, " \033[01;31minternal error:\033[00m ");  break;
    default:
      abort();
      break;
    }
  if (this->error_is_in_file)
    {
      fprintf(output, "%s\n %s\n \033[01;32m", desc, code);
      i = 0;
      for (n = start; i < n; i++)  fputc(' ', output);
      for (n = end;   i < n; i++)  fputc('^', output);
    }
  else
    fprintf(output, "%s\n", desc);
  fprintf(output, "\033[00m\n");
}


/**
 * Print information about a parsing error
 * 
 * @param  this    The error structure
 * @param  output  The output file
 */
void mds_kbdc_parse_error_print(const mds_kbdc_parse_error_t* restrict this, FILE* restrict output)
{
  ssize_t m;
  size_t n;
  char* desc;
  char* dend = this->description + strlen(this->description);
  char* dstart;
  char* dptr;
  char* p;
  char* q;
  
  /* Count the number points in the description we should modify to format it. */
  for (p = this->description, n = 0;;)
    {
      if (q = strstr(p, "‘"), q == NULL)  q = dend;
      if (p = strstr(p, "’"), p == NULL)  p = dend;
      if (q < p)  p = q;
      if (*p++)   n++;
      else        break;
    }
  
  /* Allocate string for the formatted description. */
  n = 1 + strlen(this->description) + strlen("\033[xxm’") * n;
  dptr = desc = alloca(n * sizeof(char));
  
  /* Format description. */
  for (p = this->description;;)
    {
      dstart = p;
      if (q = strstr(p, "‘"), q == NULL)  q = dend;
      if (p = strstr(p, "’"), p == NULL)  p = dend;
      if (q < p)  p = q;
      if ((n = (size_t)(p - dstart)))
	memcpy(dptr, dstart, n), dptr += n;
      if (p == dend)
	break;
      if (strstr(p, "‘") == p)  sprintf(dptr, "\033[01m‘%zn", &m),  dptr += (size_t)m,  p += strlen("‘");
      else                      sprintf(dptr, "’\033[21m%zn", &m),  dptr += (size_t)m,  p += strlen("’");
    }
  *dptr = '\0';
  
  /* Print the error. */
  print(this, output, desc);
}



/**
 * Release all resources allocated in a `mds_kbdc_parse_error_t*`
 * 
 * @param  this  The error structure
 */
void mds_kbdc_parse_error_destroy(mds_kbdc_parse_error_t* restrict this)
{
  if (this == NULL)
    return;
  free(this->pathname),    this->pathname    = NULL;
  free(this->code),        this->code        = NULL;
  free(this->description), this->description = NULL;
}


/**
 * Release all resources allocated in a `mds_kbdc_parse_error_t*`
 * and release the allocation of the structure itself
 * 
 * @param  this  The error structure
 */
void mds_kbdc_parse_error_free(mds_kbdc_parse_error_t* restrict this)
{
  mds_kbdc_parse_error_destroy(this);
  free(this);
}


/**
 * Release all resources allocated in a `NULL`-terminated group
 * of `mds_kbdc_parse_error_t*`:s
 * 
 * @param  this  The group of error structures
 */
void mds_kbdc_parse_error_destroy_all(mds_kbdc_parse_error_t** restrict these)
{
  mds_kbdc_parse_error_t* restrict that;
  if (these == NULL)
    return;
  while (that = *these, that != NULL)
    mds_kbdc_parse_error_free(that), *these++ = NULL;
}


/**
 * Release all resources allocated in a `NULL`-terminated group of
 * `mds_kbdc_parse_error_t*`:s and release the allocation of the group itself
 * 
 * @param  this  The group of error structures
 */
void mds_kbdc_parse_error_free_all(mds_kbdc_parse_error_t** restrict these)
{
  mds_kbdc_parse_error_destroy_all(these);
  free(these);
}

