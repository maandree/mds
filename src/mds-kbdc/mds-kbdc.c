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
    
    if \and(\1 128) = 128
      let \2 : \or(\2 64)
    end if
    
    \add(a b)     # a + b
    \sub(a b)     # a - b
    \mul(a b)     # a ⋅ b
    \div(a b)     # floor[a / b]
    \mod(a b)     # a mod b
    \rsh(a b)     # a ⋅ 2 ↑ b
    \lsh(a b)     # floor[a / 2 ↑ b]
    \or(a b)      # bitwise
    \and(a b)     # bitwise
    \xor(a b)     # bitwise
    \not(a)       # logical
    \equals(a b)  # a = b
    \greater(a b) # a > b
    \less(a b)    # a < b
    \set(variable index value)
    \get(variable index)
    
   */
  
  source_code_destroy(&source_code);
  return 0;
  
 pfail:
  xperror(*argv);
  source_code_destroy(&source_code);
  return 1;
}

