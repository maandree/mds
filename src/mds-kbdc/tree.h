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



#define MDS_KBDC_TREE_TYPE_INFORMATION  0
#define MDS_KBDC_TREE_TYPE_INFORMATION_LANGUAGE  1
#define MDS_KBDC_TREE_TYPE_INFORMATION_COUNTRY  2
#define MDS_KBDC_TREE_TYPE_INFORMATION_VARIANT  3
#define MDS_KBDC_TREE_TYPE_INCLUDE  4
#define MDS_KBDC_TREE_TYPE_FUNCTION  5
#define MDS_KBDC_TREE_TYPE_MACRO  6
#define MDS_KBDC_TREE_TYPE_ASSUMPTION  7
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE  8
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_CHARS  9
#define MDS_KBDC_TREE_TYPE_ASSUMPTION_HAVE_RANGE  10
#define MDS_KBDC_TREE_TYPE_FOR  11
#define MDS_KBDC_TREE_TYPE_IF  12
#define MDS_KBDC_TREE_TYPE_LET  13
#define MDS_KBDC_TREE_TYPE_MAP  15
#define MDS_KBDC_TREE_TYPE_ARRAY  16
#define MDS_KBDC_TREE_TYPE_KEYS  17
#define MDS_KBDC_TREE_TYPE_STRING  18
#define MDS_KBDC_TREE_TYPE_MACRO_CALL  19



/**
 * Keyboard layout syntax tree
 */
union mds_kbdc_tree __attribute__((transparent));
typedef union mds_kbdc_tree mds_kbdc_tree_t;



struct mds_kbdc_tree_simple
{
  int type;
  mds_kbdc_tree_t* next;
  mds_kbdc_tree_t* inner;
};


typedef struct mds_kbdc_tree_simple mds_kbdc_tree_information_t;


struct mds_kbdc_tree_information_data
{
  int type;
  mds_kbdc_tree_t* next;
  char* data;
};


typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_language_t;
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_country_t;
typedef struct mds_kbdc_tree_information_data mds_kbdc_tree_information_variant_t;


typedef struct mds_kbdc_tree_include
{
  int type;
  mds_kbdc_tree_t* next;
  char* filename;
  
} mds_kbdc_tree_include_t;


struct mds_kbdc_tree_callable
{
  int type;
  mds_kbdc_tree_t* next;
  char* data;
  mds_kbdc_tree_t* inner;
};


typedef struct mds_kbdc_tree_callable mds_kbdc_tree_function_t;
typedef struct mds_kbdc_tree_callable mds_kbdc_tree_macro_t;


typedef struct mds_kbdc_tree_simple mds_kbdc_tree_assumption_t;


typedef struct mds_kbdc_tree_assumption_have
{
  int type;
  mds_kbdc_tree_t* next;
  char* keys;
  
} mds_kbdc_tree_assumption_have_t;


typedef struct mds_kbdc_tree_assumption_have_chars
{
  int type;
  mds_kbdc_tree_t* next;
  char* chars;
  
} mds_kbdc_tree_assumption_have_chars_t;


typedef struct mds_kbdc_tree_assumption_have_range
{
  int type;
  mds_kbdc_tree_t* next;
  char* first;
  char* last;
  
} mds_kbdc_tree_assumption_have_range_t;


typedef struct mds_kbdc_tree_for
{
  int type;
  mds_kbdc_tree_t* next;
  char* first;
  char* last;
  char* variable;
  
} mds_kbdc_tree_for_t;


typedef struct mds_kbdc_tree_if
{
  int type;
  mds_kbdc_tree_t* next;
  char* condition;
  mds_kbdc_tree_t* inner;
  mds_kbdc_tree_t* otherwise;
  
} mds_kbdc_tree_if_t;


typedef struct mds_kbdc_tree_let
{
  int type;
  mds_kbdc_tree_t* next;
  char* variable;
  mds_kbdc_tree_t* value;
  
} mds_kbdc_tree_let_t;


typedef struct mds_kbdc_tree_map
{
  int type;
  mds_kbdc_tree_t* next;
  mds_kbdc_tree_t* sequence;
  mds_kbdc_tree_t* result;
  
} mds_kbdc_tree_map_t;


typedef struct mds_kbdc_tree_array
{
  int type;
  mds_kbdc_tree_t* next;
  mds_kbdc_tree_t* elements;
  
} mds_kbdc_tree_array_t;


typedef struct mds_kbdc_tree_keys
{
  int type;
  mds_kbdc_tree_t* next;
  char* keys;
  
} mds_kbdc_trees_key_t;


typedef struct mds_kbdc_tree_string
{
  int type;
  mds_kbdc_tree_t* next;
  char* string;
  
} mds_kbdc_tree_string_t;


typedef struct mds_kbdc_tree_macro_call
{
  int type;
  mds_kbdc_tree_t* next;
  char* name;
  mds_kbdc_tree_t* arguments;
  
} mds_kbdc_tree_macro_call_t;



/**
 * Keyboard layout syntax tree
 */
union mds_kbdc_tree
{
  struct
  {
    int type;
    mds_kbdc_tree_t* next;
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


#endif

