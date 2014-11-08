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
#ifndef MDS_MDS_KBDC_TREE_H
#define MDS_MDS_KBDC_TREE_H


#include <stddef.h>



/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_information_t`
 */
#define MDS_KBDC_TREE_TYPE_INFORMATION  0

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_information_language_t`
 */
#define MDS_KBDC_TREE_TYPE_INFORMATION_LANGUAGE  1

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_information_country_t`
 */
#define MDS_KBDC_TREE_TYPE_INFORMATION_COUNTRY  2

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_information_variant_t`
 */
#define MDS_KBDC_TREE_TYPE_INFORMATION_VARIANT  3

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_include_t`
 */
#define MDS_KBDC_TREE_TYPE_INCLUDE  4

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_function_t`
 */
#define MDS_KBDC_TREE_TYPE_FUNCTION  5

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_macro_t`
 */
#define MDS_KBDC_TREE_TYPE_MACRO  6

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_assumption`
 */
#define MDS_KBDC_TREE_TYPE_ASSUMPTION  7

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_assumption_have_t`
 */
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE  8

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_assumption_have_chars_t`
 */
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_CHARS  9

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_assumption_have_range_t`
 */
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_RANGE  10

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_for_t`
 */
#define MDS_KBDC_TREE_TYPE_FOR  11

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_if_t`
 */
#define MDS_KBDC_TREE_TYPE_IF  12

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_let_t`
 */
#define MDS_KBDC_TREE_TYPE_LET  13

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_map_t`
 */
#define MDS_KBDC_TREE_TYPE_MAP  15

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_array_t`
 */
#define MDS_KBDC_TREE_TYPE_ARRAY  16

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_keys_t`
 */
#define MDS_KBDC_TREE_TYPE_KEYS  17

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_string_t`
 */
#define MDS_KBDC_TREE_TYPE_STRING  18

/**
 * Value of `mds_kbdc_tree_t.type` for `mds_kbdc_tree_type_macro_call_t`
 */
#define MDS_KBDC_TREE_TYPE_MACRO_CALL  19



/**
 * Keyboard layout syntax tree
 */
union mds_kbdc_tree __attribute__((transparent));

/**
 * Keyboard layout syntax tree
 */
typedef union mds_kbdc_tree mds_kbdc_tree_t;



/**
 * This macro is used in this header file, and is then
 * undefined. It defined the common members for these
 * tree structures.
 * 
 * It defines:
 * -  int type;            -- Integer that specifies which structure is used
 * -  mds_kbdc_tree_t*;    -- The next node in the tree, at the same level; a sibling
 */
#define MDS_KBDC_TREE_COMMON       int type; mds_kbdc_tree_t* next

/**
 * This macro is used in this header file, and is then
 * undefined. It defined padding of a tree structure.
 * 
 * @param  S:size_t  The size of the data structure excluding this padding and
 *                   the members defined by the macro `MDS_KBDC_TREE_COMMON`
 */
#define MDS_KBDC_TREE_PADDING_(S)  char _padding[(5 * sizeof(void*) - (S)) / sizeof(char)]

/**
 * This macro is used in this header file, and is then
 * undefined. It defined padding of a tree structure.
 * 
 * @param  N  The number of sizeof(void*) sized memebers defined data
 *            structure excluding this padding and the members defined
 *            by the macro `MDS_KBDC_TREE_COMMON`
 */
#define MDS_KBDC_TREE_PADDING(N)   MDS_KBDC_TREE_PADDING((N) * sizeof(void*))



/**
 * This datastructure is not meant to be
 * used directly, it is a common definition
 * for tree structurs that only have, beside
 * the common members, a pointer to the first
 * node on next level in the tree
 */
struct mds_kbdc_tree_nesting
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The first child node, `.inner.next`
   * is used to access the second child node.
   */
  mds_kbdc_tree_t* inner;
  
  MDS_KBDC_TREE_PADDING(1);
};


/**
 * Tree structure for the "information"-keyword
 */
typedef struct mds_kbdc_tree_nesting mds_kbdc_tree_information_t;


/**
 * This datastructure is not meant to be
 * used directly, it is a common definition
 * for the tree structurs for the information
 * entries: the children of `mds_kbdc_tree_information_t`
 */
struct mds_kbdc_tree_information_data
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The value of the information entry
   */
  char* data;
  
  MDS_KBDC_TREE_PADDING(1);
};


/**
 * Declaration of one of the languages for which the layout is designed
 */
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_language_t;

/**
 * Declaration of one of the country for which the layout is designed
 */
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_country_t;

/**
 * Declaration of which variant of the language–country the layout is
 */
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_variant_t;


/**
 * Leaf structure for inclusion of a file
 */
typedef struct mds_kbdc_tree_include
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The filename of the file to include
   */
  char* filename;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_tree_include_t;


/**
 * This datastructure is not meant to be
 * used directly, it is a common definition
 * for tree structurs that define a callable
 * element
 */
struct mds_kbdc_tree_callable
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The name of the callable
   */
  char* name;
  
  /**
   * The first child node, `.inner.next`
   * is used to access the second child node
   */
  mds_kbdc_tree_t* inner;
  
  MDS_KBDC_TREE_PADDING(2);
};


/**
 * Tree structure for a function
 */
typedef struct mds_kbdc_tree_callable mds_kbdc_tree_function_t;

/**
 * Tree structure for a macro
 */
typedef struct mds_kbdc_tree_callable mds_kbdc_tree_macro_t;


/**
 * Tree structure for the "assumption"-keyword
 */
typedef struct mds_kbdc_tree_nesting mds_kbdc_tree_assumption_t;


/**
 * Treeu structure for making the assumption
 * that there is a mapping to a key or string
 */
typedef struct mds_kbdc_tree_assumption_have
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The key or string
   */
  mds_kbdc_tree_t* data;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_tree_assumption_have_t;


/**
 * Leaf structure for making the assumption
 * that there are mappings to a set of characters
 */
typedef struct mds_kbdc_tree_assumption_have_chars
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The characters
   */
  char* chars;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_tree_assumption_have_chars_t;


/**
 * Leaf structure for making the assumption
 * that there are mappings to a range of characters
 */
typedef struct mds_kbdc_tree_assumption_have_range
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The first character, inclusive
   */
  char* first;
  
  /**
   * The last character, inclusive
   */
  char* last;
  
  MDS_KBDC_TREE_PADDING(2);
  
} mds_kbdc_tree_assumption_have_range_t;


/**
 * Tree structure for a "for"-loop
 */
typedef struct mds_kbdc_tree_for
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The first value to variable should take, inclusive
   */
  char* first;
  
  /**
   * The last value the variable should take, inclusive
   */
  char* last;
  
  /**
   * The variable
   */
  char* variable;
  
  /**
   * The first child node, `.inner.next` is
   * used to access the second child node.
   * This is what should be done inside the loop.
   */
  mds_kbdc_tree_t* inner;
  
  MDS_KBDC_TREE_PADDING(4);
  
} mds_kbdc_tree_for_t;


/**
 * Tree structure for a "if"-statement
 */
typedef struct mds_kbdc_tree_if
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The condition
   */
  char* condition;
  
  /**
   * This is what should be done inside
   * if the condition is satisfied
   */
  mds_kbdc_tree_t* inner;
  
  /**
   * This is what should be done inside
   * if the condition is not satisfied
   */
  mds_kbdc_tree_t* otherwise;
  
  MDS_KBDC_TREE_PADDING(3);
  
} mds_kbdc_tree_if_t;


/**
 * Tree structure for assigning a value to a variable,
 * possibly declaring the variable in the process
 */
typedef struct mds_kbdc_tree_let
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The variable
   */
  char* variable;
  
  /**
   * The value to assign to the variable
   */
  mds_kbdc_tree_t* value;
  
  MDS_KBDC_TREE_PADDING(2);
  
} mds_kbdc_tree_let_t;


/**
 * Tree structure for mapping a (possible single element)
 * sequence of key combinations or strings to another
 * combination or string or sequence there of
 */
typedef struct mds_kbdc_tree_map
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The input sequence
   */
  mds_kbdc_tree_t* sequence;
  
  /**
   * The output sequence
   */
  mds_kbdc_tree_t* result;
  
  MDS_KBDC_TREE_PADDING(2);
  
} mds_kbdc_tree_map_t;


/**
 * Tree structure for an array of values
 */
typedef struct mds_kbdc_tree_array
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The first value, `.elements.next`
   * is used to access the second value.
   */
  mds_kbdc_tree_t* elements;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_tree_array_t;


/**
 * Leaf structure for a key combination
 */
typedef struct mds_kbdc_tree_keys
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The key combination
   */
  char* keys;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_trees_key_t;


/**
 * Leaf structure for a string
 */
typedef struct mds_kbdc_tree_string
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The string
   */
  char* string;
  
  MDS_KBDC_TREE_PADDING(1);
  
} mds_kbdc_tree_string_t;


/**
 * Tree structure for a macro call
 */
typedef struct mds_kbdc_tree_macro_call
{
  MDS_KBDC_TREE_COMMON;
  
  /**
   * The name of the macro
   */
  char* name;
  
  /**
   * The first input argument for the
   * macro call, the second is accessed
   * using `.arguments.next`
   */
  mds_kbdc_tree_t* arguments;
  
  MDS_KBDC_TREE_PADDING(2);
  
} mds_kbdc_tree_macro_call_t;



/**
 * Keyboard layout syntax tree
 */
union mds_kbdc_tree
{
  struct
  {
    MDS_KBDC_TREE_COMMON;
    MDS_KBDC_TREE_PADDING(0);
  };
  
  mds_kbdc_tree_information_t information;
  mds_kbdc_tree_information_language_t language;
  mds_kbdc_tree_information_country_t country;
  mds_kbdc_tree_information_variant_t variant;
  mds_kbdc_tree_information_include_t include;
  mds_kbdc_tree_function_t function;
  mds_kbdc_tree_macro_t macro;
  mds_kbdc_tree_assumption_t assumption;
  mds_kbdc_tree_assumption_have have;
  mds_kbdc_tree_assumption_have_chars have_chars;
  mds_kbdc_tree_assumption_have_range have_range;
  mds_kbdc_tree_for_t for_;
  mds_kbdc_tree_if_t if_;
  mds_kbdc_tree_let_t let;
  mds_kbdc_tree_map_t map;
  mds_kbdc_tree_array_t array;
  mds_kbdc_tree_keys_t keys;
  mds_kbdc_tree_string_t string;
  mds_kbdc_tree_macro_call_t macro_call;
};



#undef MDS_KBDC_TREE_PADDING
#undef MDS_KBDC_TREE_PADDING_
#undef MDS_KBDC_TREE_COMMON

#endif

