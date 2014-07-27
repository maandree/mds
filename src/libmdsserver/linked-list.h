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
#ifndef MDS_LIBMDSSERVER_LINKED_LIST_H
#define MDS_LIBMDSSERVER_LINKED_LIST_H


/**
 * Linear array sentinel doubly linked list class.
 * An array linked list is a linked list constructed
 * by parallel arrays which gives it nice memory
 * properties. A linear sentinel linked list is a
 * linear linked listed constructed as a circular
 * linked listed with a sentinel (dummy) node between
 * the first node and the last node. In this
 * implementation, when a node is removed the value
 * stored that that position is not removed before
 * that position is reused. Insertion methods have
 * constant amortised time complexity, and constant
 * amortised memory complexity, removal methods have
 * constant time complexity and constant memory
 * complexity.
 */


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>



/**
 * Sentinel value indicating that the position is unused
 */
#if __WORDSIZE == 64
# define LINKED_LIST_UNUSED  (-__INT64_C(9223372036854775807) - 1)
#else
# define LINKED_LIST_UNUSED  (-2147483647 - 1)
#endif



#define LINKED_LIST_T_VERSION  0

/**
 * Linear array sentinel doubly linked list class
 */
typedef struct linked_list
{
  /**
   * The size of the arrays
   */
  size_t capacity;
  
  /**
   * The index after the last used index in
   * `values` and `next`
   */
  size_t end;
  
  /**
   * Head of the stack of reusable positions
   */
  size_t reuse_head;
  
  /**
   * Stack of indices than are no longer in use
   */
  ssize_t* reusable;
  
  /**
   * The value stored in each node
   */
  size_t* values;
  
  /**
   * The next node for each node, `edge` if the current
   * node is the last node, and `LINKED_LIST_UNUSED`
   * if there is no node on this position
   */
  ssize_t* next;
  
  /**
   * The previous node for each node, `edge` if
   * the current node is the first node, and
   * `LINKED_LIST_UNUSED` if there is no node
   * on this position
   */
  ssize_t* previous;
  
  /**
   * The sentinel node in the list
   */
  ssize_t edge;
  
} linked_list_t;



/**
 * Create a linked list
 * 
 * @param   this      Memory slot in which to store the new linked list
 * @param   capacity  The minimum initial capacity of the linked list, 0 for default
 * @return            Non-zero on error, `errno` will have been set accordingly
 */
int linked_list_create(linked_list_t* restrict this, size_t capacity);

/**
 * Release all resources in a linked list, should
 * be done even if `linked_list_create` fails
 * 
 * @param  this  The linked list
 */
void linked_list_destroy(linked_list_t* restrict this);

/**
 * Clone a linked list
 * 
 * @param   this  The linked list to clone
 * @param   out   Memory slot in which to store the new linked list
 * @return        Non-zero on error, `errno` will have been set accordingly
 */
int linked_list_clone(const linked_list_t* restrict this, linked_list_t* restrict out);

/**
 * Pack the list so that there are no reusable
 * positions, and reduce the capacity to the
 * smallest capacity that can be used. Not that
 * values (nodes) returned by the list's methods
 * will become invalid. Additionally (to reduce
 * the complexity) the list will be defragment
 * so that the nodes' indices are continuous.
 * This method has linear time complexity and
 * linear memory complexity.
 * 
 * @param   this  The list
 * @return        Non-zero on error, `errno` will have been set accordingly
 */
int linked_list_pack(linked_list_t* restrict this);
    
/**
 * Insert a value in the beginning of the list
 * 
 * @param   this:linked_list_t*  The list
 * @param   value:size_t         The value to insert
 * @return  :ssize_t             The node that has been created and inserted,
 *                               `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
#define linked_list_insert_beginning(this, value)  \
  (linked_list_insert_after(this, value, this->edge))

/**
 * Remove the node at the beginning of the list
 * 
 * @param   this:linked_list_t*  The list
 * @return  :ssize_t             The node that has been removed
 */
#define linked_list_remove_beginning(this)  \
  (linked_list_remove_after(this, this->edge))

/**
 * Insert a value after a specified, reference, node
 * 
 * @param   this         The list
 * @param   value        The value to insert
 * @param   predecessor  The reference node
 * @return               The node that has been created and inserted,
 *                       `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
ssize_t linked_list_insert_after(linked_list_t* restrict this, size_t value, ssize_t predecessor);

/**
 * Remove the node after a specified, reference, node
 * 
 * @param   this         The list
 * @param   predecessor  The reference node
 * @return               The node that has been removed
 */
ssize_t linked_list_remove_after(linked_list_t* restrict this, ssize_t predecessor);    

/**
 * Insert a value before a specified, reference, node
 * 
 * @param   this       The list
 * @param   value      The value to insert
 * @param   successor  The reference node
 * @return             The node that has been created and inserted,
 *                     `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
ssize_t linked_list_insert_before(linked_list_t* restrict this, size_t value, ssize_t successor);

/**
 * Remove the node before a specified, reference, node
 * 
 * @param   this       The list
 * @param   successor  The reference node
 * @return             The node that has been removed
 */
ssize_t linked_list_remove_before(linked_list_t* restrict this, ssize_t successor);    

/**
 * Remove the node from the list
 * 
 * @param  this  The list
 * @param  node  The node to remove
 */
void linked_list_remove(linked_list_t* restrict this, ssize_t node);

/**
 * Insert a value in the end of the list
 * 
 * @param   this:linked_list_t*  The list
 * @param   value:size_t         The value to insert
 * @return  :ssize_t             The node that has been created and inserted,
 *                               `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
#define linked_list_insert_end(this, value)  \
  (linked_list_insert_before((this), (value), (this)->edge))

/**
 * Remove the node at the end of the list
 * 
 * @param   this:linked_list_t*  The list
 * @return  :ssize_t             The node that has been removed
 */
#define linked_list_remove_end(this)  \
  (linked_list_remove_before((this), (this)->edge))

/**
 * Calculate the buffer size need to marshal a linked list
 * 
 * @param   this  The list
 * @return        The number of bytes to allocate to the output buffer
 */
size_t linked_list_marshal_size(const linked_list_t* restrict this) __attribute__((pure));

/**
 * Marshals a linked list
 * 
 * @param  this  The list
 * @param  data  Output buffer for the marshalled data
 */
void linked_list_marshal(const linked_list_t* restrict this, char* restrict data);

/**
 * Unmarshals a linked list
 * 
 * @param   this  Memory slot in which to store the new linked list
 * @param   data  In buffer with the marshalled data
 * @return        Non-zero on error, errno will be set accordingly.
 *                Destroy the list on error.
 */
int linked_list_unmarshal(linked_list_t* restrict this, char* restrict data);

/**
 * Wrapper for `for` keyword that iterates over each element in a linked list
 * 
 * @param  list:linked_list_t  The linked list
 * @param  node:ssize_t        The variable to store the node in at each iteration
 */
#define foreach_linked_list_node(list, node)  \
  for (node = list.edge; node = list.next[node], node != list.edge;)


/**
 * Print the content of the list
 * 
 * @param  this    The list
 * @param  output  Output file
 */
void linked_list_dump(linked_list_t* restrict this, FILE* output);


#endif

