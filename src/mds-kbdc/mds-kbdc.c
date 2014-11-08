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
  const char* pathname = argv_[1];
  source_code_t source_code;
  size_t i, n;
  
  argc = argc_;
  argv = argv_;
  
  source_code_initialise(&source_code);
  fail_if (read_source_lines(pathname, &source_code) < 0);
  /* TODO '\t':s should be expanded into ' ':s. */
  
  for (i = 0, n = source_code.line_count; i < n; i++)
    {
      char* line = source_code.lines[i];
      char* end;
      char prev_end_char;
      
      while (*line && (*line == ' '))
	line++;
      end = strchrnul(line, ' ');
      if (end == line)
	continue;
      prev_end_char = *end;
      *end = '\0';
      
      if (!strcmp(line, "information"))
	;
      else if (!strcmp(line, "language"))
	;
      else if (!strcmp(line, "country"))
	;
      else if (!strcmp(line, "variant"))
	;
      else if (!strcmp(line, "include"))
	;
      else if (!strcmp(line, "function"))
	;
      else if (!strcmp(line, "macro"))
	;
      else if (!strcmp(line, "assumption"))
	;
      else if (!strcmp(line, "if"))
	;
      else if (!strcmp(line, "else"))
	;
      else if (!strcmp(line, "for"))
	;
      else if (!strcmp(line, "end"))
	;
      else if (!strcmp(line, "return"))
	;
      else if (!strcmp(line, "continue"))
	;
      else if (!strcmp(line, "break"))
	;
      else if (!strcmp(line, "let"))
	;
      else if (!strcmp(line, "have"))
	;
      else if (!strcmp(line, "have_chars"))
	;
      else if (!strcmp(line, "have_range"))
	;
      else if ((*line != '"') && (*line != '<') && (strchr(line, '(') == NULL))
	{
	  size_t j, m1, m2;
	  m1 = (size_t)(line - source_code.lines[i]);
	  m2 = (size_t)(end - source_code.lines[i]);
	  fprintf(stderr, "\033[1m%s:%zu:%zu-%zu: \033[31merror:\033[0m invalid keyword \033[1m‘%s’\033[0m\n",
		  pathname, i + 1, m1 + 1, m2 + 1 /* TODO measure chars, not bytes */, line);
	  fprintf(stderr, " %s\n \033[1;32m", source_code.real_lines[i]);
	  for (j = 0; j < m1; j++)
	    fputc(' ', stderr);
	  for (; j < m2; j++)
	    if ((source_code.lines[i][j] & 0xC0) != 0x80)
	      fputc('^', stderr);
	  fprintf(stderr, "\033[0m\n");
	}
      
      *end = prev_end_char;
    }
  /*
    
    information
      language "LANGUAGE" # multiple is allowed
      country "COUNTRY" # multiple is allowed
      variant "VARIANT"
    end information
    
    include "some file"
    
    function add/3
      \add(\add(\1 \2) \3)
    end function
    function add/4
      \add(\add(\1 \2 \3) \4)
    end function
    
    macro caps_affected/2
      <letter \1> : "\1"
      <shift letter \1> : "\2"
      <caps letter \1> : "\2"
      <caps shift letter \1> : "\1"
    end macro
    
    <altgr letter \\> : <dead letter ´>
    <altgr shift letter \\> : "¬"
    <altgr shift letter j> : <dead letter \u031B> # horn
    
    assumption
      have_range "a" "z"
      have_range "A" "Z"
      have_range "0" "9"
      have_chars " !\"#$%&'()*+,-./:;<=>?@[\]^_`{|}~"
      have <dead compose>
      have <shift>
      have <altgr>
      have <alt>
      have <ctrl>
    end assumption
    
    for "A" to "Z" as \1
      <dead compose> "(" "\1" ")" : "\add(\u24B6 \sub(\1 "A"))"
    end for
    
    let \6 : { \and(\4 16) \and(\4 32) \and(\4 64) \and(\4 128) }
    
    for 0 to 3 as \8
      \set(\6 \8 \add(\rsh(\get(\6 \8) \8) \mul(2 \rsh(\get(\7 \8)))))
    end for
    
    if \or(\equals(\1 0) \equals(\1 "0"))
      "0"
    else if \or(\equals(\1 1) \equals(\1 "1"))
      "1"
    else
      "9"
    end if
    
    if \and(\1 128)  ##  a number is true iff it is not zero
      let \2 : \or(\2 64)
    end if
    
    <altgr menu> : <-altgr ultra>
    
   */
  
  source_code_destroy(&source_code);
  return 0;
  
 pfail:
  xperror(*argv);
  source_code_destroy(&source_code);
  return 1;
}

