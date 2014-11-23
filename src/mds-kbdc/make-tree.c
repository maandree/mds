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
#include "make-tree.h"

#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


#ifndef DEBUG
# define DEBUG_PROC(statements)
#else
# define DEBUG_PROC(statements)  statements
#endif


/**
 * Print the current keyword stack, this is intended to
 * as a compiler debugging feature and should be used from
 * inside `DEBUG_PROC`
 */
#define PRINT_STACK					\
  do							\
    {							\
      size_t i = stack_ptr;				\
      fprintf(stderr, "stack {\n");			\
      while (i--)					\
	fprintf(stderr, "  %s\n", keyword_stack[i]);	\
      fprintf(stderr, "}\n");				\
    }							\
  while (0)


/**
 * Check whether a value is inside a closed range
 * 
 * @param   LOWER:¿T?  The lower bound, inclusive
 * @param   VALUE:¿T?  The value to test
 * @param   UPPER:¿T?  The upper bound, inclusive
 * @return  :int       1 if `LOWER` ≤ `VALUE` ≤ `UPPER`, otherwise 0
 */
#define in_range(LOWER, VALUE, UPPER)			\
  (((LOWER) <= (VALUE)) && ((VALUE) <= (UPPER)))


/**
 * Check whether a character is a valid callable name character, forward slash is accepted
 * 
 * @param   C:char  The character
 * @return  :int    Zero if `C` is a valid callable name character or a forward slash, otherwise 0
 */
#define is_name_char(C)									\
  (in_range('a', C, 'z') || in_range('A', C, 'Z') || strchr("0123456789_/", C))


/**
 * Pointer to the beginning of the current line
 */
#define LINE			\
  (result->source_code->lines[line_i])


/**
 * Update the tip of the stack to point to the address
 * of the current stack's tip's `next`-member
 */
#define NEXT							\
  tree_stack[stack_ptr] = &(tree_stack[stack_ptr][0]->next)


/**
 * Add an error the to error list
 * 
 * @param  ERROR_IS_IN_FILE:int           Whether the error is in the layout code
 * @param  SEVERITY:identifier            * in `MDS_KBDC_PARSE_ERROR_*` to indicate severity
 * @param  ...:const char*, ...           Error description format string and arguments
 * @scope  error:mds_kbdc_parse_error_t*  Variable where the new error will be stored
 */
#define NEW_ERROR(ERROR_IS_IN_FILE, SEVERITY, ...)				\
  NEW_ERROR_(result, SEVERITY, ERROR_IS_IN_FILE, line_i,			\
	     (size_t)(line - LINE), (size_t)(end - LINE), 1, __VA_ARGS__)


/**
 * Create a new node
 * 
 * @param  LOWERCASE:identifier  The keyword, for the node type, in lower case
 * @param  UPPERCASE:identifier  The keyword, for the node type, in upper case
 */
#define NEW_NODE(LOWERCASE, UPPERCASE)				\
  mds_kbdc_tree_##LOWERCASE##_t* node;				\
  fail_if (xcalloc(node, 1, mds_kbdc_tree_##LOWERCASE##_t));	\
  node->type = MDS_KBDC_TREE_TYPE_##UPPERCASE;			\
  node->loc_line  = line_i;					\
  node->loc_start = (size_t)(line - LINE);			\
  node->loc_end   = (size_t)(end  - LINE)


/**
 * Create a new node named `subnode`
 * 
 * @param  LOWERCASE:identifier  The keyword, for the node type, in lower case
 * @param  UPPERCASE:identifier  The keyword, for the node type, in upper case
 */
#define NEW_SUBNODE(LOWERCASE, UPPERCASE)				\
  mds_kbdc_tree_##LOWERCASE##_t* subnode;				\
  fail_if (xcalloc(subnode, 1, mds_kbdc_tree_##LOWERCASE##_t));		\
  subnode->type = MDS_KBDC_TREE_TYPE_##UPPERCASE;			\
  subnode->loc_line  = line_i;						\
  subnode->loc_start = (size_t)(line - LINE);				\
  subnode->loc_end   = (size_t)(end  - LINE)


/**
 * Update the tip of the tree stack with the current node
 * and change the pointer at the tip to the pointer to the
 * current node's down pointer
 * 
 * This is what should be done when a branch node has
 * been created and should be added to the result tree
 * 
 * @param  KEYWORD:const char*  The keyword for the current node's type
 */
#define BRANCH(KEYWORD)					\
  (*(tree_stack[stack_ptr]) = (mds_kbdc_tree_t*)node,	\
   tree_stack[stack_ptr + 1] = &(node->inner),		\
   keyword_stack[stack_ptr++] = KEYWORD)


/**
 * Update the tip of the tree stack with the current node
 * and change the pointer at the tip to the pointer to the
 * current node's next pointer
 * 
 * This is what should be done when a leaf node has been
 * created and should be added to the result tree
 */
#define LEAF						\
  (*(tree_stack[stack_ptr]) = (mds_kbdc_tree_t*)node,	\
   NEXT)


/**
 * Skip all blank spaces
 * 
 * @param  var:const char*  The variable
 */
#define SKIP_SPACES(var)		\
  while (*var && (*var == ' '))		\
    var++


/**
 * Check that there are no tokens after a keyword
 * 
 * @param  KEYWORD:const char*  The keyword,
 */
#define NO_PARAMETERS(KEYWORD)						\
  do									\
    {									\
      line += strlen(line);						\
      *end = prev_end_char, prev_end_char = '\0';			\
      SKIP_SPACES(line);						\
      if (*line)							\
	{								\
	  end = line + strlen(line);					\
	  NEW_ERROR(1, ERROR, "extra token after ‘%s’", KEYWORD);	\
	}								\
    }									\
  while (0)


/**
 * Take next parameter, which should be a name of a callable,
 * and store it in the current node
 * 
 * @param  var:identifier  The name of the member variable, for the current
 *                         node, where the parameter should be stored
 */
#define NAMES_1(var)							\
  do									\
    {									\
      line += strlen(line);						\
      *end = prev_end_char, prev_end_char = '\0';			\
      SKIP_SPACES(line);						\
      if (*line == '\0')						\
	{								\
	  line = original, end = line + strlen(line);			\
	  NEW_ERROR(1, ERROR, "a name is expected");			\
	}								\
      else								\
	{								\
	  char* name_end = line;					\
	  char* test;							\
	  int stray_char = 0;						\
	  while (*name_end && is_name_char(*name_end))			\
	    name_end++;							\
	  if (*name_end && (*name_end != ' '))				\
	    {								\
	      char* end_end = name_end + 1;				\
	      while ((*end_end & 0xC0) == 0x80)				\
		end_end++;						\
	      prev_end_char = *end_end, *end_end = '\0';		\
	    NEW_ERROR(1, ERROR, "stray ‘%s’ character", name_end);	\
	    error->start = (size_t)(name_end - LINE);			\
	    error->end   = (size_t)(end_end  - LINE);			\
	    *end_end = prev_end_char;					\
	    stray_char = 1;						\
	    }								\
	  test = name_end;						\
	  SKIP_SPACES(test);						\
	  if (*test && !stray_char)					\
	    {								\
	      NEW_ERROR(1, ERROR, "too many parameters");		\
	      error->start = (size_t)(test - LINE);			\
	      error->end   = strlen(LINE);				\
	    }								\
	  end = name_end;						\
	  prev_end_char = *end;						\
	  *end = '\0';							\
	  fail_if ((node->var = strdup(line)) == NULL);			\
	}								\
    }									\
  while (0)


/**
 * Suppress the next `line += strlen(line)`
 */
#define NO_JUMP			\
  (*end = prev_end_char,	\
   end = line,			\
   prev_end_char = *end,	\
   *end = '\0')


/**
 * Check whether a character ends a strings, whilst not being being a quote
 * 
 * @param  c:char  The character
 */
#define IS_END(c)	\
  strchr(" >}])", c)


/**
 * Take next parameter, which should be a string or numeral,
 * and store it in the current node
 * 
 * @param  var:identifier  The name of the member variable, for the current
 *                         node, where the parameter should be stored
 */
#define CHARS(var)									\
  do											\
    {											\
      if (too_few)									\
	break;										\
      line += strlen(line);								\
      *end = prev_end_char, prev_end_char = '\0';					\
      SKIP_SPACES(line);								\
      if (*line == '\0')								\
	{										\
	  line = original, end = line + strlen(line);					\
	  NEW_ERROR(1, ERROR, "too few parameters");					\
	  line = end, too_few = 1;							\
	}										\
      else										\
	{										\
	  char* arg_end = line;								\
	  char* call_end = arg_end;							\
	  int escape = 0, quote = 0;							\
	  while (*arg_end)								\
	    {										\
	      char c = *arg_end++;							\
	      if      (escape)               escape = 0;				\
	      else if (arg_end <= call_end)  ;						\
	      else if (c == '\\')							\
		{									\
		  escape = 1;								\
		  call_end = arg_end + get_end_of_call(arg_end, 0, strlen(arg_end)); 	\
		}									\
	      else if (quote)                quote = (c != '"');			\
	      else if (IS_END(c))            { arg_end--; break; }			\
	      else                           quote = (c == '"');			\
	    }										\
	  prev_end_char = *arg_end, *arg_end = '\0', end = arg_end;			\
	  fail_if ((node->var = strdup(line)) == NULL);					\
	  line = end;									\
	}										\
    }											\
  while (0)


/**
 * Test that there are no more parameters
 */
#define END						\
  do							\
    {							\
      SKIP_SPACES(line);				\
      if (*line)					\
	{						\
	  NEW_ERROR(1, ERROR, "too many parameters");	\
	  error->end = strlen(LINE);			\
	}						\
    }							\
  while (0)


/**
 * Test that the next parameter is in quotes
 */
#define QUOTES							\
  do								\
    {								\
      char* line_ = line;					\
      line += strlen(line);					\
      *end = prev_end_char;					\
      SKIP_SPACES(line);					\
      if (*line && (*line != '"'))				\
	{							\
	  char* arg_end = line;					\
	  SKIP_SPACES(arg_end);					\
	  NEW_ERROR(1, ERROR, "parameter must be in quotes");	\
	  error->end = (size_t)(arg_end - LINE);		\
	}							\
      *end = '\0';						\
      line = line_;						\
    }								\
  while (0)


/**
 * Check that there is exactly one parameter, that it is in
 * quotes, and add it to the current node
 * 
 * @param  var:identifier  The name of the member variable, for the current
 *                         node, where the parameter should be stored
 */
#define QUOTES_1(var)	\
  do			\
    {			\
      QUOTES;		\
      CHARS(var);	\
      END;		\
    }			\
  while (0)


/**
 * Check that the next word is a specific keyword
 * 
 * @parma  KEYWORD:const char*  The keyword
 */
#define TEST_FOR_KEYWORD(KEYWORD)					\
  do									\
    {									\
      if (too_few)							\
	break;								\
      line += strlen(line);						\
      *end = prev_end_char, prev_end_char = '\0';			\
      SKIP_SPACES(line);						\
      if (*line == '\0')						\
	{								\
	  line = original, end = line + strlen(line);			\
	  NEW_ERROR(1, ERROR, "too few parameters");			\
	  line = end, too_few = 1;					\
	}								\
      else								\
	{								\
	  int ok = (strstr(line, KEYWORD) == line);			\
	  line += strlen(KEYWORD);					\
	  ok = ok && ((*line == '\0') || (*line == ' '));		\
	  if (ok)							\
	    {								\
	      end = line;						\
	      prev_end_char = *end, *end = '\0';			\
	      break;							\
	    }								\
	  line -= strlen(KEYWORD);					\
	  end = line;							\
	  SKIP_SPACES(end);						\
	  prev_end_char = *end, *end = '\0';				\
	  NEW_ERROR(1, ERROR, "expecting keyword ‘%s’", KEYWORD);	\
	}								\
    }									\
  while (0)


/**
 * Take next parameter, which should be a key combination or strings,
 * and store it in the current node
 * 
 * @param  var:identifier  The name of the member variable, for the current
 *                         node, where the parameter should be stored
 */
#define KEYS(var)									\
  do											\
    {											\
      if (too_few)									\
	break;										\
      line += strlen(line);								\
      *end = prev_end_char, prev_end_char = '\0';					\
      SKIP_SPACES(line);								\
      if (*line == '\0')								\
	{										\
	  line = original, end = line + strlen(line);					\
	  NEW_ERROR(1, ERROR, "too few parameters");					\
	  line = end, too_few = 1;							\
	}										\
      else										\
	{										\
	  char* arg_end = line;								\
	  char* call_end = arg_end;							\
	  int escape = 0, quote = 0, triangle = (*arg_end == '<');			\
	  while (*arg_end)								\
	    {										\
	      char c = *arg_end++                ;					\
	      if      (escape)                   escape = 0;				\
	      else if (arg_end <= call_end)      ;					\
	      else if (c == '\\')							\
		{									\
		  escape = 1;								\
		  call_end = arg_end + get_end_of_call(arg_end, 0, strlen(arg_end));	\
		}									\
	      else if (quote)                    quote = (c != '"');			\
	      else if (c == '\"')                quote = 1;				\
	      else if (c == '>')                 triangle = 0;				\
	      else if (IS_END(c) && !triangle)   { arg_end--; break; }			\
	    }										\
	  prev_end_char = *arg_end, *arg_end = '\0', end = arg_end;			\
	  if (*line == '<')								\
	    {										\
	      NEW_SUBNODE(keys, KEYS);							\
	      node->var = (mds_kbdc_tree_t*)subnode;					\
	      fail_if ((subnode->keys = strdup(line)) == NULL);				\
	    }										\
	  else										\
	    {										\
	      NEW_SUBNODE(string, STRING);						\
	      node->var = (mds_kbdc_tree_t*)subnode;					\
	      fail_if ((subnode->string = strdup(line)) == NULL);			\
	    }										\
	  line = end;									\
	}										\
    }											\
  while (0)


/**
 * Take next parameter, which should be a key combination,
 * and store it in the current node
 * 
 * @param  var:identifier  The name of the member variable, for the current
 *                         node, where the parameter should be stored
 */
#define PURE_KEYS(var)									\
  do											\
    {											\
      if (too_few)									\
	break;										\
      line += strlen(line);								\
      *end = prev_end_char, prev_end_char = '\0';					\
      SKIP_SPACES(line);								\
      if (*line == '\0')								\
	{										\
	  line = original, end = line + strlen(line);					\
	  NEW_ERROR(1, ERROR, "too few parameters");					\
	  line = end, too_few = 1;							\
	}										\
      else										\
	{										\
	  char* arg_end = line;								\
	  char* call_end = arg_end;							\
	  int escape = 0, quote = 0, triangle = (*arg_end == '<');			\
	  while (*arg_end)								\
	    {										\
	      char c = *arg_end++                ;					\
	      if      (escape)                   escape = 0;				\
	      else if (arg_end <= call_end)      ;					\
	      else if (c == '\\')							\
		{									\
		  escape = 1;								\
		  call_end = arg_end + get_end_of_call(arg_end, 0, strlen(arg_end));	\
		}									\
	      else if (quote)                    quote = (c != '"');			\
	      else if (c == '\"')                quote = 1;				\
	      else if (c == '>')                 triangle = 0;				\
	      else if (IS_END(c) && !triangle)  { arg_end--; break; }			\
	    }										\
	  prev_end_char = *arg_end, *arg_end = '\0';					\
	  fail_if ((node->var = strdup(line)) == NULL);					\
	  end = arg_end, line = end;							\
	}										\
    }											\
  while (0)


/**
 * Parse a sequence in a mapping
 * 
 * @param  mapseq:int  Whether this is a mapping sequence, otherwise
 *                     it is treated as macro call arguments
 */
#define SEQUENCE(mapseq)										\
  do /* for(;;) */											\
    {													\
      *end = prev_end_char;										\
      SKIP_SPACES(line);										\
      if ((*line == '\0') || (*line == (mapseq ? ':' : ')')))						\
	break;												\
      if (mapseq && (*line == '('))									\
	{												\
	  NEW_NODE(unordered, UNORDERED);								\
	  node->loc_end = node->loc_start + 1;								\
	  BRANCH(")");											\
	  line++;											\
	}												\
      else if (*line == '[')										\
	{												\
	  NEW_NODE(alternation, ALTERNATION);								\
	  node->loc_end = node->loc_start + 1;								\
	  BRANCH("]");											\
	  line++;											\
	}												\
      else if (*line == '.')										\
	{												\
	  NEW_NODE(nothing, NOTHING);									\
	  node->loc_end = node->loc_start + 1;								\
	  LEAF;												\
	  line++;											\
	}												\
      else if (strchr("])", *line))									\
	{												\
	  end = line + 1;										\
	  prev_end_char = *end, *end = '\0';								\
	  if (stack_ptr == stack_orig)									\
	    {												\
	      NEW_ERROR(1, ERROR, "runaway ‘%s’", line);						\
	    }												\
	  else												\
	    {												\
	      stack_ptr--;										\
	      if (strcmp(line, keyword_stack[stack_ptr]))						\
		{											\
		  NEW_ERROR(1, ERROR, "expected ‘%s’ but got ‘%s’", keyword_stack[stack_ptr], line);	\
		}											\
	      NEXT;											\
	    }												\
	  *end = prev_end_char;										\
	  line++;											\
	}												\
      else if (*line == '<')										\
	{												\
	  NEW_NODE(keys, KEYS);										\
	  NO_JUMP;											\
	  PURE_KEYS(keys);										\
	  LEAF;												\
	  node->loc_end = (size_t)(line - LINE);							\
	}												\
      else												\
	{												\
	  NEW_NODE(string, STRING);									\
	  NO_JUMP;											\
	  CHARS(string);										\
	  LEAF;												\
	  node->loc_end = (size_t)(line - LINE);							\
	}												\
    }													\
  while (1)


/**
 * Change the scopes created in `SEQUENCE` has all been popped
 * 
 * @param  stack_orig:size_t  The size of the stack when `SEQUENCE` was called
 */
#define SEQUENCE_FULLY_POPPED(stack_orig)						\
  do											\
    {											\
      if (stack_ptr == stack_orig)							\
	break;										\
      end = line + 1;									\
      NEW_ERROR(1, ERROR, "premature end of sequence");					\
      while (stack_ptr > stack_orig)							\
	{										\
	  stack_ptr--;									\
	  NEW_ERROR(1, NOTE, "missing associated ‘%s’", keyword_stack[stack_ptr]);	\
	  error->start = tree_stack[stack_ptr][0]->loc_start;				\
	  error->end   = tree_stack[stack_ptr][0]->loc_end;				\
	}										\
    }											\
  while (0)


/**
 * Create new leaf and update the stack accordingly
 * 
 * @param  LOWERCASE:identifier  The keyword, for the node type, in lower case
 * @param  UPPERCASE:identifier  The keyword, for the node type, in upper case
 * @param  PARSE:expression      Statement, without final semicolon, to retrieve members
 */
#define MAKE_LEAF(LOWERCASE, UPPERCASE, PARSE)		\
  do							\
    {							\
      NEW_NODE(LOWERCASE, UPPERCASE);			\
      PARSE;						\
      LEAF;						\
    }							\
  while (0)


/**
 * Create new branch and update the stack accordingly
 * 
 * @param  LOWERCASE:identifier  The keyword, for the node type, in lower case
 * @param  UPPERCASE:identifier  The keyword, for the node type, in upper case
 * @param  PARSE:expression      Statement, without final semicolon, to retrieve members
 */
#define MAKE_BRANCH(LOWERCASE, UPPERCASE, PARSE)	\
  do							\
    {							\
      NEW_NODE(LOWERCASE, UPPERCASE);			\
      PARSE;						\
      BRANCH(#LOWERCASE);				\
    }							\
  while (0)



/**
 * Parse a file into a syntax tree
 * 
 * @param   filename  The filename of the file to parse
 * @param   result    Output parameter for the parsing result
 * @return            -1 if an error occursed that cannot be stored in `result`, zero otherwise
 */
int parse_to_tree(const char* restrict filename, mds_kbdc_parsed_t* restrict result)
{
  mds_kbdc_parse_error_t* error;
  size_t line_i, line_n;
  const char** keyword_stack = NULL;
  mds_kbdc_tree_t*** tree_stack = NULL;
  size_t stack_ptr = 0;
  int saved_errno, in_array = 0;
  
  fail_if (xmalloc(result->source_code, 1, mds_kbdc_source_code_t));
  mds_kbdc_source_code_initialise(result->source_code);
  
  /* Get a non-relative pathname for the file, relative filenames
   * can be misleading as the program can have changed working
   * directory to be able to resolve filenames. */
  result->pathname = realpath(filename, NULL);
  fail_if (result->pathname == NULL);
  
  /* Check that the file exists and can be read. */
  if (access(result->pathname, R_OK) < 0)
    {
      saved_errno = errno;
      NEW_ERROR_(result, ERROR, 0, 0, 0, 0, 0, NULL);
      error->description = strdup(strerror(saved_errno));
      fail_if (error->description == NULL);
      return 0;
    }
  
  /* Read the file and simplify it a bit. */
  fail_if (read_source_lines(result->pathname, result->source_code) < 0);
  /* TODO '\t':s should be expanded into ' ':s. */
  
  /* Allocate stacks needed to parse the tree. */
  {
    /* The maxium line-length is needed because lines can have there own stacking,
     * like sequence mapping lines, additionally, let statements can have one array. */
    size_t max_line_length = 0, cur_line_length;
    for (line_i = 0, line_n = result->source_code->line_count; line_i < line_n; line_i++)
      {
	cur_line_length = strlen(LINE);
	if (max_line_length < cur_line_length)
	  max_line_length = cur_line_length;
      }
    
    fail_if (xmalloc(keyword_stack, result->source_code->line_count + max_line_length, const char*));
    fail_if (xmalloc(tree_stack, result->source_code->line_count + max_line_length + 1, mds_kbdc_tree_t**));
  }
  /* Create a node-slot for the tree root. */
  *tree_stack = &(result->tree);
  
  for (line_i = 0, line_n = result->source_code->line_count; line_i < line_n; line_i++)
    {
      char* line = LINE;
      char* end;
      char prev_end_char;
      char* original;
      int too_few = 0;
      
      SKIP_SPACES(line);
      if (end = strchrnul(line, ' '), end == line)
	continue;
      prev_end_char = *end, *end = '\0';
      original = line;
      
    redo:
      if (in_array)
	{
	  for (;;)
	    {
	      SKIP_SPACES(line);
	      if (*line == '\0')
		break;
	      else if (*line == '}')
		{
		  line++;
		  end = line + strlen(line);
		  END;
		  line = end, prev_end_char = '\0';
		  in_array = 0;
		  stack_ptr -= 2;
		  NEXT;
		  break;
		}
	      else
		{
#define node subnode
		  NEW_NODE(string, STRING);
		  NO_JUMP;
		  CHARS(string);
		  LEAF;
		  node->loc_end = (size_t)(end - LINE);
		  *end = prev_end_char;
		  line = end;
#undef node
		}
	    }
	  continue;
	}
      else if (!strcmp(line, "have_chars"))
	MAKE_LEAF(assumption_have_chars, ASSUMPTION_HAVE_CHARS, QUOTES_1(chars));
      else if (!strcmp(line, "have_range"))
	MAKE_LEAF(assumption_have_range, ASSUMPTION_HAVE_RANGE, CHARS(first); CHARS(last); END);
      else if (!strcmp(line, "have")) MAKE_LEAF(assumption_have, ASSUMPTION_HAVE, KEYS(data); END);
      else if (!strcmp(line, "information")) MAKE_BRANCH(information, INFORMATION, NO_PARAMETERS("information"));
      else if (!strcmp(line, "assumption")) MAKE_BRANCH(assumption, ASSUMPTION, NO_PARAMETERS("assumption"));
      else if (!strcmp(line, "return")) MAKE_LEAF(return, RETURN, NO_PARAMETERS("return"));
      else if (!strcmp(line, "continue")) MAKE_LEAF(continue, CONTINUE, NO_PARAMETERS("continue"));
      else if (!strcmp(line, "break")) MAKE_LEAF(break, BREAK, NO_PARAMETERS("break"));
      else if (!strcmp(line, "language")) MAKE_LEAF(information_language, INFORMATION_LANGUAGE, QUOTES_1(data));
      else if (!strcmp(line, "country")) MAKE_LEAF(information_country, INFORMATION_COUNTRY, QUOTES_1(data));
      else if (!strcmp(line, "variant")) MAKE_LEAF(information_variant, INFORMATION_VARIANT, QUOTES_1(data));
      else if (!strcmp(line, "include")) MAKE_LEAF(include, INCLUDE, QUOTES_1(filename));
      else if (!strcmp(line, "function")) MAKE_BRANCH(function, FUNCTION, NAMES_1(name));
      else if (!strcmp(line, "macro")) MAKE_BRANCH(macro, MACRO, NAMES_1(name));
      else if (!strcmp(line, "if")) MAKE_BRANCH(if, IF, CHARS(condition); END);
      else if (!strcmp(line, "else"))
	{
	  size_t i;
	  if (stack_ptr == 0)
	    {
	      NEW_ERROR(1, ERROR, "runaway ‘else’ statement");
	      goto next;
	    }
	  line += strlen(line);
	  *end = prev_end_char, prev_end_char = '\0';
	  end = line + strlen(line);
	  SKIP_SPACES(line);
	  i = stack_ptr - 1;
	  while (keyword_stack[i] == NULL)
	    i--;
	  if (strcmp(keyword_stack[i], "if"))
	    {
	      stack_ptr--;
	      line = original, end = line + strlen(line);
	      NEW_ERROR(1, ERROR, "runaway ‘else’ statement");
	    }
	  else if (*line == '\0')
	    {
	      /* else */
	      mds_kbdc_tree_if_t* supernode = &(tree_stack[stack_ptr - 1][0]->if_);
	      if (supernode->otherwise)
		{
		  line = strstr(LINE, "else");
		  end = line + 4, prev_end_char = *end;
		  NEW_ERROR(1, ERROR, "multiple ‘else’ statements");
		  mds_kbdc_tree_free(supernode->otherwise);
		  supernode->otherwise = NULL;
		}
	      tree_stack[stack_ptr] = &(supernode->otherwise);
	    }
	  else if ((strstr(line, "if") == line) && ((line[2] == ' ') || (line[2] == '\0')))
	    {
	      /* else if */
	      mds_kbdc_tree_if_t* supernode = &(tree_stack[stack_ptr - 1][0]->if_);
	      NEW_NODE(if, IF);
	      node->loc_end = node->loc_start + 2;
	      end = line += 2, prev_end_char = *end, *end = '\0';
	      CHARS(condition);
	      END;
	      tree_stack[stack_ptr] = &(supernode->otherwise);
	      BRANCH(NULL);
	    }
	  else
	    {
	      NEW_ERROR(1, ERROR, "expecting nothing or ‘if’");
	      stack_ptr--;
	    }
	}
      else if (!strcmp(line, "for"))
	{
	  NEW_NODE(for, FOR);
	  CHARS(first);
	  TEST_FOR_KEYWORD("to");
	  CHARS(last);
	  TEST_FOR_KEYWORD("as");
	  CHARS(variable);
	  END;
	  BRANCH("for");
	}
      else if (!strcmp(line, "let"))
	{
	  NEW_NODE(let, LET);
	  CHARS(variable);
	  TEST_FOR_KEYWORD(":");
	  *end = prev_end_char;
	  SKIP_SPACES(line);
	  if (*line == '{')
#define inner value
	    BRANCH(NULL);
#undef inner
	  else
	    LEAF;
	  if (*line == '\0')
	    {
	      line = original, end = line + strlen(line), prev_end_char = '\0';
	      NEW_ERROR(1, ERROR, "too few parameters");
	    }
	  else if (*line != '{')
	    {
#define node subnode
	      NEW_NODE(string, STRING);
	      NO_JUMP;
	      CHARS(string);
	      node->loc_end = (size_t)(end - LINE);
#undef node
	      node->value = (mds_kbdc_tree_t*)subnode;
	      END;
	    }
	  else
	    {
#define node subnode
#define inner elements
	      NEW_NODE(array, ARRAY);
	      BRANCH("}");
	      node->loc_end = node->loc_start + 1;
#undef inner
#undef node
	      in_array = 1;
	      line++;
	      goto redo;
	    }
	}
      else if (!strcmp(line, "end"))
	{
	  if (stack_ptr == 0)
	    {
	      NEW_ERROR(1, ERROR, "runaway ‘end’ statement");
	      goto next;
	    }
	  line += strlen(line);
	  *end = prev_end_char, prev_end_char = '\0';
	  SKIP_SPACES(line);
	  while (keyword_stack[--stack_ptr] == NULL);
	  if (*line == '\0')
	    {
	      line = original, end = line + strlen(line);
	      NEW_ERROR(1, ERROR, "expecting a keyword after ‘end’");
	    }
	  else if (strcmp(line, keyword_stack[stack_ptr]))
	    NEW_ERROR(1, ERROR, "expected ‘%s’ but got ‘%s’", keyword_stack[stack_ptr], line);
	  NEXT;
	}
      else if (strchr("\"<([0123456789", *line))
	{
	  size_t stack_orig = stack_ptr + 1;
#define node supernode
#define inner sequence
	  NEW_NODE(map, MAP);
	  node->loc_end = node->loc_start;
	  BRANCH(":");
#undef inner
#undef node
	  SEQUENCE(1);
	  SEQUENCE_FULLY_POPPED(stack_orig);
#define node supernode
#define inner result
	  stack_ptr--;
	  *end = prev_end_char;
	  SKIP_SPACES(line);
	  if (*line++ != ':')
	    {
	      LEAF;
	      continue; /* Not an error in macros. */
	    }
	  BRANCH(":");
#undef inner
#undef node
	  SEQUENCE(1);
	  SEQUENCE_FULLY_POPPED(stack_orig);
	  stack_ptr--;
	  *end = prev_end_char;
	  SKIP_SPACES(line);
#define node supernode
	  LEAF;
#undef node
	  if (*line == '\0')
	    continue;
	  end = line + strlen(line), prev_end_char = *end;
	  NEW_ERROR(1, ERROR, "too many parameters");
	}
      else
	{
	  char* old_end = end;
	  char old_prev_end_char = prev_end_char;
	  size_t stack_orig = stack_ptr + 1;
	  *end = prev_end_char;
	  end = strchrnul(line, '(');
	  prev_end_char = *end, *end = '\0';
	  if (prev_end_char)
	    {
#define node supernode
#define inner arguments
	      NEW_NODE(macro_call, MACRO_CALL);
	      old_end = end, old_prev_end_char = prev_end_char;
	      NO_JUMP;
	      *old_end = '\0';
	      CHARS(name);
	      BRANCH(NULL);
	      end = old_end, prev_end_char = old_prev_end_char;
	      line++;
#undef inner
#undef node
	      SEQUENCE(0);
	      SEQUENCE_FULLY_POPPED(stack_orig);
#define node supernode
	      if (*line == ')')
		{
		  line++;
		  SKIP_SPACES(line);
		  if (*line)
		    {
		      NEW_ERROR(1, ERROR, "extra token after macro call");
		      error->end = strlen(LINE);
		    }
		}
	      else
		{
		  NEW_ERROR(1, ERROR, "missing ‘)’");
		  error->start = (size_t)(strchr(LINE, '(') - LINE);
		  error->end   = error->start + 1;
		}
	      stack_ptr--;
	      NEXT;
	      goto next;
#undef node
	    }
	  *old_end = '\0';
	  end = old_end;
	  prev_end_char = old_prev_end_char;
	  if (strchr("}", *line))
	    NEW_ERROR(1, ERROR, "runaway ‘%c’", *line);
	  else
	    NEW_ERROR(1, ERROR, "invalid syntax ‘%s’", line);
	}
      
    next:
      *end = prev_end_char;
    }
  
  /* Check that all scopes have been popped. */
  if (stack_ptr)
    {
      char* line = NULL;
      char* end = NULL;
      while (stack_ptr && keyword_stack[stack_ptr - 1] == NULL)
	stack_ptr--;
      if (stack_ptr)
	{
	  NEW_ERROR(0, ERROR, "premature end of file");
	  while (stack_ptr--)
	    {
	      if (keyword_stack[stack_ptr] == NULL)
		continue;
	      line_i = tree_stack[stack_ptr][0]->loc_line;
	      line = LINE + tree_stack[stack_ptr][0]->loc_start;
	      end  = LINE + tree_stack[stack_ptr][0]->loc_end;
	      if (!strcmp(keyword_stack[stack_ptr], "}"))
		NEW_ERROR(1, NOTE, "missing associated ‘%s’", keyword_stack[stack_ptr]);
	      else
		NEW_ERROR(1, NOTE, "missing associated ‘end %s’", keyword_stack[stack_ptr]);
	    }
	}
    }
  
  free(keyword_stack);
  free(tree_stack);
  return 0;
  
 pfail:
  saved_errno = errno;
  free(keyword_stack);
  free(tree_stack);
  return errno = saved_errno, -1;
}



#undef MAKE_BRANCH
#undef MAKE_LEAF
#undef SEQUENCE_FULLY_POPPED
#undef SEQUENCE
#undef PURE_KEYS
#undef KEYS
#undef TEST_FOR_KEYWORD
#undef QUOTES_1
#undef QUOTES
#undef END
#undef CHARS
#undef IS_END
#undef NO_JUMP
#undef NAMES_1
#undef NO_PARAMETERS
#undef SKIP_SPACES
#undef LEAF
#undef BRANCH
#undef NEW_NODE
#undef NEW_ERROR
#undef NEXT
#undef LINE
#undef is_name_char
#undef in_range
#undef PRINT_STACK
#undef DEBUG_PROC

