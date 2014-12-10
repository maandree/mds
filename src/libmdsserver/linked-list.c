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
#include "linked-list.h"

#include "macros.h"

#include <string.h>
#include <errno.h>


/**
 * The default initial capacity
 */
#ifndef LINKED_LIST_DEFAULT_INITIAL_CAPACITY
#define LINKED_LIST_DEFAULT_INITIAL_CAPACITY  128
#endif


/**
 * Computes the nearest, but higher, power of two,
 * but only if the current value is not a power of two
 * 
 * @param   value  The value to be rounded up to a power of two
 * @return         The nearest, but not smaller, power of two
 */
static size_t to_power_of_two(size_t value)
{
  value -= 1;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
#if __WORDSIZE == 64
  value |= value >> 32;
#endif
  return value + 1;
}


/**
 * Create a linked list
 * 
 * @param   this      Memory slot in which to store the new linked list
 * @param   capacity  The minimum initial capacity of the linked list, 0 for default
 * @return            Non-zero on error, `errno` will have been set accordingly
 */
int linked_list_create(linked_list_t* restrict this, size_t capacity)
{
  /* Use default capacity of zero is specified. */
  if (capacity == 0)
    capacity = LINKED_LIST_DEFAULT_INITIAL_CAPACITY;
  
  /* Initialise the linked list. */
  this->capacity   = capacity = to_power_of_two(capacity);
  this->edge       = 0;
  this->end        = 1;
  this->reuse_head = 0;
  this->reusable   = NULL;
  this->values     = NULL;
  this->next       = NULL;
  this->previous   = NULL;
  fail_if (xmalloc(this->reusable, capacity, ssize_t));
  fail_if (xmalloc(this->values,   capacity,  size_t));
  fail_if (xmalloc(this->next,     capacity, ssize_t));
  fail_if (xmalloc(this->previous, capacity, ssize_t));
  this->values[this->edge]   = 0;
  this->next[this->edge]     = this->edge;
  this->previous[this->edge] = this->edge;
  
  return 0;
 fail:
  return -1;
}


/**
 * Release all resources in a linked list, should
 * be done even if `linked_list_create` fails
 * 
 * @param  this  The linked list
 */
void linked_list_destroy(linked_list_t* restrict this)
{
  free(this->reusable),  this->reusable = NULL;
  free(this->values),    this->values   = NULL;
  free(this->next),      this->next     = NULL;
  free(this->previous),  this->previous = NULL;
}


/**
 * Clone a linked list
 * 
 * @param   this  The linked list to clone
 * @param   out   Memory slot in which to store the new linked list
 * @return        Non-zero on error, `errno` will have been set accordingly
 */
int linked_list_clone(const linked_list_t* restrict this, linked_list_t* restrict out)
{
  size_t n = this->capacity * sizeof(ssize_t);
  size_t*  restrict new_values = NULL;
  ssize_t* restrict new_next = NULL;
  ssize_t* restrict new_previous = NULL;
  ssize_t* restrict new_reusable;
  int saved_errno;
  
  out->values   = NULL;
  out->next     = NULL;
  out->previous = NULL;
  out->reusable = NULL;
  
  fail_if (xbmalloc(new_values,   n));
  fail_if (xbmalloc(new_next,     n));
  fail_if (xbmalloc(new_previous, n));
  fail_if (xbmalloc(new_reusable, n));
  
  out->values   = new_values;
  out->next     = new_next;
  out->previous = new_previous;
  out->reusable = new_reusable;
  
  out->capacity   = this->capacity;
  out->end        = this->end;
  out->reuse_head = this->reuse_head;
  out->edge       = this->edge;
  
  memcpy(out->values,   this->values,   n);
  memcpy(out->next,     this->next,     n);
  memcpy(out->previous, this->previous, n);
  memcpy(out->reusable, this->reusable, n);
  
  return 0;

 fail:
  saved_errno = errno;
  free(new_values);
  free(new_next);
  free(new_previous);
  return errno = saved_errno, -1;
}


/**
 * Pack the list so that there are no reusable
 * positions, and reduce the capacity to the
 * smallest capacity that can be used. Note that
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
int linked_list_pack(linked_list_t* restrict this)
{
  ssize_t* restrict new_next = NULL;
  ssize_t* restrict new_previous = NULL;
  ssize_t* restrict new_reusable = NULL;
  size_t size = this->end - this->reuse_head;
  size_t cap = to_power_of_two(size);
  ssize_t head = 0;
  size_t i = 0;
  ssize_t node;
  size_t* restrict vals;
  int saved_errno;
  
  fail_if (xmalloc(vals, cap, size_t));
  while (((size_t)head != this->end) && (this->next[head] == LINKED_LIST_UNUSED))
    head++;
  if ((size_t)head != this->end)
    for (node = head; (node != head) || (i == 0); i++)
      {
	vals[i] = this->values[node];
	node = this->next[node];
      }
  
  if (cap != this->capacity)
    {
      fail_if (xmalloc(new_next,     cap, ssize_t));
      fail_if (xmalloc(new_previous, cap, ssize_t));
      fail_if (xmalloc(new_reusable, cap, ssize_t));
      
      free(this->next);
      free(this->previous);
      free(this->reusable);
      
      this->next     = new_next;
      this->previous = new_previous;
      this->reusable = new_reusable;
    }
  
  for (i = 0; i < size; i++)
    this->next[i] = (ssize_t)(i + 1);
  this->next[size - 1] = 0;
  
  for (i = 1; i < size; i++)
    this->previous[i] = (ssize_t)(i - 1);
  this->previous[0] = (ssize_t)(size - 1);
  
  this->values = vals;
  this->end = size;
  this->reuse_head = 0;
  
  return 0;

 fail:
  saved_errno = errno;
  free(vals);
  free(new_next);
  free(new_previous);
  return errno = saved_errno, -1;
}


/**
 * Gets the next free position, and grow the
 * arrays if necessary. This methods has constant
 * amortised time complexity.
 * 
 * @param   this  The list
 * @return        The next free position,
 *                `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
static ssize_t linked_list_get_next(linked_list_t* restrict this)
{
  size_t* tmp_values;
  ssize_t* tmp;
  
  if (this->reuse_head > 0)
    return this->reusable[--(this->reuse_head)];
  if (this->end == this->capacity)
    {
      if ((ssize_t)(this->end) < 0)
	fail_if ((errno = ENOMEM));
      
      this->capacity <<= 1;
      
      fail_if (yrealloc(tmp_values, this->values,   this->capacity, size_t));
      fail_if (yrealloc(tmp,        this->next,     this->capacity, ssize_t));
      fail_if (yrealloc(tmp,        this->previous, this->capacity, ssize_t));
      fail_if (yrealloc(tmp,        this->reusable, this->capacity, ssize_t));
    }
  return (ssize_t)(this->end++);
 fail:
  return LINKED_LIST_UNUSED;
}


/**
 * Mark a position as unused
 * 
 * @param   this  The list
 * @param   node  The position
 * @return        The position
 */
static ssize_t linked_list_unuse(linked_list_t* restrict this, ssize_t node)
{
  if (node < 0)
    return node;
  this->reusable[this->reuse_head++] = node;
  this->next[node] = LINKED_LIST_UNUSED;
  this->previous[node] = LINKED_LIST_UNUSED;
  return node;
}


/**
 * Insert a value after a specified, reference, node
 * 
 * @param   this         The list
 * @param   value        The value to insert
 * @param   predecessor  The reference node
 * @return               The node that has been created and inserted,
 *                       `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
ssize_t linked_list_insert_after(linked_list_t* this, size_t value, ssize_t predecessor)
{
  ssize_t node = linked_list_get_next(this);
  fail_if (node == LINKED_LIST_UNUSED);
  this->values[node] = value;
  this->next[node] = this->next[predecessor];
  this->next[predecessor] = node;
  this->previous[node] = predecessor;
  this->previous[this->next[node]] = node;
  return node;
 fail:
  return LINKED_LIST_UNUSED;
}


/**
 * Remove the node after a specified, reference, node
 * 
 * @param   this         The list
 * @param   predecessor  The reference node
 * @return               The node that has been removed
 */
ssize_t linked_list_remove_after(linked_list_t* restrict this, ssize_t predecessor)
{
  ssize_t node = this->next[predecessor];
  this->next[predecessor] = this->next[node];
  this->previous[this->next[node]] = predecessor;
  return linked_list_unuse(this, node);
}


/**
 * Insert a value before a specified, reference, node
 * 
 * @param   this       The list
 * @param   value      The value to insert
 * @param   successor  The reference node
 * @return             The node that has been created and inserted,
 *                     `LINKED_LIST_UNUSED` on error, `errno` will be set accordingly
 */
ssize_t linked_list_insert_before(linked_list_t* restrict this, size_t value, ssize_t successor)
{
  ssize_t node = linked_list_get_next(this);
  fail_if (node == LINKED_LIST_UNUSED);
  this->values[node] = value;
  this->previous[node] = this->previous[successor];
  this->previous[successor] = node;
  this->next[node] = successor;
  this->next[this->previous[node]] = node;
  return node;
 fail:
  return LINKED_LIST_UNUSED;
}


/**
 * Remove the node before a specified, reference, node
 * 
 * @param   this       The list
 * @param   successor  The reference node
 * @return             The node that has been removed
 */
ssize_t linked_list_remove_before(linked_list_t* restrict this, ssize_t successor)
{
  ssize_t node = this->previous[successor];
  this->previous[successor] = this->previous[node];
  this->next[this->previous[node]] = successor;
  return linked_list_unuse(this, node);
}


/**
 * Remove the node from the list
 * 
 * @param  this  The list
 * @param  node  The node to remove
 */
void linked_list_remove(linked_list_t* restrict this, ssize_t node)
{
  this->next[this->previous[node]] = this->next[node];
  this->previous[this->next[node]] = this->previous[node];
  linked_list_unuse(this, node);
}


/**
 * Calculate the buffer size need to marshal a linked list
 * 
 * @param   this  The list
 * @return        The number of bytes to allocate to the output buffer
 */
size_t linked_list_marshal_size(const linked_list_t* restrict this)
{
  return sizeof(size_t) * (4 + this->reuse_head + 3 * this->end) + sizeof(int);
}


/**
 * Marshals a linked list
 * 
 * @param  this  The list
 * @param  data  Output buffer for the marshalled data
 */
void linked_list_marshal(const linked_list_t* restrict this, char* restrict data)
{
  buf_set(data, int, 0, LINKED_LIST_T_VERSION);
  buf_next(data, int, 1);
  
  buf_set(data, size_t, 0, this->capacity);
  buf_set(data, size_t, 1, this->end);
  buf_set(data, size_t, 2, this->reuse_head);
  buf_set(data, ssize_t, 3, this->edge);
  buf_next(data, size_t, 4);
  
  memcpy(data, this->reusable, this->reuse_head * sizeof(ssize_t));
  buf_next(data, ssize_t, this->reuse_head);
  
  memcpy(data, this->values, this->end * sizeof(size_t));
  buf_next(data, size_t, this->end);
  
  memcpy(data, this->next, this->end * sizeof(ssize_t));
  buf_next(data, ssize_t, this->end);
  
  memcpy(data, this->previous, this->end * sizeof(ssize_t));
}


/**
 * Unmarshals a linked list
 * 
 * @param   this  Memory slot in which to store the new linked list
 * @param   data  In buffer with the marshalled data
 * @return        Non-zero on error, `errno` will be set accordingly.
 *                Destroy the list on error.
 */
int linked_list_unmarshal(linked_list_t* restrict this, char* restrict data)
{
  /* buf_get(data, int, 0, LINKED_LIST_T_VERSION); */
  buf_next(data, int, 1);
  
  this->reusable = NULL;
  this->values   = NULL;
  this->next     = NULL;
  this->previous = NULL;
  
  buf_get(data, size_t, 0, this->capacity);
  buf_get(data, size_t, 1, this->end);
  buf_get(data, size_t, 2, this->reuse_head);
  buf_get(data, ssize_t, 3, this->edge);
  buf_next(data, size_t, 4);
  
  fail_if (xmalloc(this->reusable, this->capacity, size_t));
  fail_if (xmalloc(this->values,   this->capacity, size_t));
  fail_if (xmalloc(this->next,     this->capacity, size_t));
  fail_if (xmalloc(this->previous, this->capacity, size_t));
  
  memcpy(this->reusable, data, this->reuse_head * sizeof(ssize_t));
  buf_next(data, ssize_t, this->reuse_head);
  
  memcpy(this->values, data, this->end * sizeof(size_t));
  buf_next(data, size_t, this->end);
  
  memcpy(this->next, data, this->end * sizeof(ssize_t));
  buf_next(data, ssize_t, this->end);
  
  memcpy(this->previous, data, this->end * sizeof(ssize_t));
  
  return 0;
 fail:
  return -1;
}


/**
 * Print the content of the list
 * 
 * @param  this    The list
 * @param  output  Output file
 */
void linked_list_dump(linked_list_t* restrict this, FILE* restrict output)
{
  ssize_t i;
  size_t j;
  fprintf(output, "======= LINKED LIST DUMP =======\n");
  fprintf(output, "Capacity:    %zu\n", this->capacity);
  fprintf(output, "End:         %zu\n", this->end);
  fprintf(output, "Reuse head:  %zu\n", this->reuse_head);
  fprintf(output, "Edge:        %zi\n", this->edge);
  fprintf(output, "--------------------------------\n");
  fprintf(output, "Node table (Next, Prev, Value):\n");
  i = this->edge;
  fprintf(output, "    %zi: %zi, %zi, %zu\n", i, this->next[i], this->previous[i], this->values[i]);
  foreach_linked_list_node((*this), i)
    fprintf(output, "    %zi: %zi, %zi, %zu\n", i, this->next[i], this->previous[i], this->values[i]);
  i = this->edge;
  fprintf(output, "    %zi: %zi, %zi, %zu\n", i, this->next[i], this->previous[i], this->values[i]);
  fprintf(output, "--------------------------------\n");
  fprintf(output, "Raw node table:\n");
  for (j = 0; j < this->end; j++)
    fprintf(output, "    %zu: %zi, %zi, %zu\n", i, this->next[i], this->previous[i], this->values[i]);
  fprintf(output, "--------------------------------\n");
  fprintf(output, "Reuse stack:\n");
  for (j = 0; j < this->reuse_head; j++)
    fprintf(output, "    %zu: %zi\n", j, this->reusable[j]);
  fprintf(output, "================================\n");
}

