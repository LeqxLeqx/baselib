/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 *                                                                         *
 *  baselib: a library implementing several simple utilities for C         *
 *  Copyright (C) 2017  LeqxLeqx                                           *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <assert.h>
#include <stdlib.h>
#include <semaphore.h>

#include "any.h"
#include "list_struct.h"
#include "list_type.h"
#include "list.h"
#include "mtest.h"
#include "strings.h"
#include "string_builder.h"
#include "utilities.h"

#include "linked_list.h"



struct LinkedListNode
{
  struct LinkedListNode * next;
  Any value;
};


struct LinkedList
{
  List base;
  struct LinkedListNode
    * start,
    * end,
    * cache;
  unsigned int cache_index;
  unsigned int size;
};

struct LinkedListTraversal
{
  ListTraversal base;
  struct LinkedListNode * current;
};






/* INTERNAL METHODS */
static void linked_list_get_cached_current(
    LinkedList * linked_list,
    unsigned int index,
    struct LinkedListNode **  node_ptr,
    unsigned int * node_index_ptr
  )
{

  if (linked_list->cache && linked_list->cache_index <= index)
  {
    *node_ptr = linked_list->cache;
    *node_index_ptr = linked_list->cache_index;
  }
  else
  {
    *node_ptr = linked_list->start;
    *node_index_ptr = 0;
  }

}

static struct LinkedListNode * linked_list_node_new(Any value, struct LinkedListNode * next)
{
  struct LinkedListNode * ret = (struct LinkedListNode *) _malloc(sizeof(struct LinkedListNode));
  assert(ret);

  ret->value = value;
  ret->next = next;

  return ret;
}

static void linked_list_add_imp(LinkedList * linked_list, Any value)
{
  struct LinkedListNode
    * node = linked_list_node_new(value, NULL);

  if (linked_list->size == 0)
  {
    linked_list->start = node;
    linked_list->end = node;
  }
  else
  {
    linked_list->end->next = node;
    linked_list->end = node;
  }

  linked_list->size++;
}

static void linked_list_insert_imp(LinkedList * linked_list, unsigned int index, Any value)
{
  struct LinkedListNode * node, * previous_node, * new_node;
  unsigned int node_index;

  if (index == linked_list->size)
  {
    linked_list_add_imp(linked_list, value);
    return;
  }
  else if (index == 0)
  {
    new_node = linked_list_node_new(value, linked_list->start);
    linked_list->start = new_node;
  }
  else
  {
    linked_list_get_cached_current(linked_list, index - 1, &node, &node_index);

    while (node_index != index)
    {
      previous_node = node;
      node = node->next;
      node_index++;
    }

    new_node = linked_list_node_new(value, node);
    previous_node->next = new_node;
  }

  linked_list->size++;

  linked_list->cache = new_node;
  linked_list->cache_index = index;
}

static unsigned int linked_list_remove_imp(LinkedList * linked_list, Any any, bool free_on_remove)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  unsigned int ret = 0;

  struct LinkedListNode * current = linked_list->start;
  struct LinkedListNode * last = NULL;
  struct LinkedListNode ** last_ptr = &linked_list->start;

  while (current)
  {

    if (any_equals(current->value, any))
    {

      if (free_on_remove)
        _free(any_to_ptr(current->value));

      ret++;
      *last_ptr = current->next;
      struct LinkedListNode * hold = current;
      current = current->next;

      _free(hold);
    }
    else
    {
      last = current;
      last_ptr = &last->next;
      current = current->next;
    }

  }

  linked_list->end = last;
  linked_list->size -= ret;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);
  return ret;
}

static Any linked_list_remove_at_imp(LinkedList * linked_list, unsigned int index)
{
  Any ret;

  if (linked_list->size == 1)
  {
    ret = linked_list->start->value;
    _free(linked_list->start);
    linked_list->start = NULL;
    linked_list->end = NULL;
  }
  else
  {
    struct LinkedListNode
      * last = NULL,
      * current = linked_list->start;

    for (unsigned int k = 0; k < index; k++)
    {
      last = current;
      current = current->next;
    }

    if (last == NULL) /* ergo, is first */
    {
      linked_list->start = current->next;
      ret = current->value;
      _free(current);
    }
    else if (current->next == NULL) /* ergo, is last */
    {
      ret = current->value;
      _free(current);
      linked_list->end = last;
      last->next = NULL;
    }
    else
    {
      last->next = current->next;
      ret = current->value;
      _free(current);
    }
  }

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  linked_list->size--;

  return ret;
}


/* END OF INTERNAL METHODS */



/* methods for LinkedList */

LinkedList * linked_list_new()
{
  LinkedList * ret = (LinkedList *) _malloc(sizeof(LinkedList));
  assert(ret);

  ret->base.type = LIST_TYPE_LINKED_LIST;
  ret->base.open_traversals = 0;

  ret->base.list_destroy = (void (*)(List *)) linked_list_destroy;
  ret->base.list_destroy_and_free = (void (*)(List *)) linked_list_destroy_and_free;
  ret->base.list_destroy_and_user_free = (void (*)(List *, void (*)(void *))) linked_list_destroy_and_user_free;
  ret->base.list_destroy_and = (void (*)(List *, void (*)(Any))) linked_list_destroy_and;
  ret->base.list_size = (unsigned int (*)(List *)) linked_list_size;
  ret->base.list_contains = (bool (*)(List *, Any)) linked_list_contains;
  ret->base.list_get = (Any (*)(List *, unsigned int)) linked_list_get;
  ret->base.list_get_char = (char (*)(List *, unsigned int)) linked_list_get_char;
  ret->base.list_get_int = (int (*)(List *, unsigned int)) linked_list_get_int;
  ret->base.list_get_str = (char * (*)(List *, unsigned int)) linked_list_get_str;
  ret->base.list_get_ptr = (void * (*)(List *, unsigned int)) linked_list_get_ptr;
  ret->base.list_add = (void (*)(List *, Any)) linked_list_add;
  ret->base.list_add_range = (void (*)(List *, List *)) linked_list_add_range;
  ret->base.list_set = (void (*)(List *, unsigned int, Any)) linked_list_set;
  ret->base.list_insert = (void (*)(List *, unsigned int, Any)) linked_list_insert;
  ret->base.list_insert_range = (void (*)(List *, unsigned int, List *)) linked_list_insert_range;
  ret->base.list_remove_at = (Any (*)(List *, unsigned int)) linked_list_remove_at;
  ret->base.list_remove = (unsigned int (*)(List *, Any)) linked_list_remove;
  ret->base.list_remove_and_free = (unsigned int (*)(List *, Any)) linked_list_remove_and_free;
  ret->base.list_swap = (void (*)(List *, unsigned int, unsigned int)) linked_list_swap;
  ret->base.list_reposition = (void (*)(List *, unsigned int, unsigned int)) linked_list_reposition;
  ret->base.list_clear = (void (*)(List *)) linked_list_clear;
  ret->base.list_clear_and_free = (void (*)(List *)) linked_list_clear_and_free;
  ret->base.list_clear_and_user_free = (void (*)(List *, void (*)(void *))) linked_list_clear_and_user_free;
  ret->base.list_clear_and = (void (*)(List *, void (*)(Any))) linked_list_clear_and;
  ret->base.list_to_array = (Any * (*)(List *)) linked_list_to_array;
  ret->base.list_sub_list = (List * (*)(List *, unsigned int, unsigned int)) linked_list_sub_list;
  ret->base.list_clone = (List * (*)(List *)) linked_list_clone;
  ret->base.list_to_string = (char * (*)(List *)) linked_list_to_string;
  ret->base.list_foreach = (void (*)(List *, void (*)(Any))) linked_list_foreach;
  ret->base.list_get_traversal = (ListTraversal * (*)(List *)) linked_list_get_traversal;

  ret->start = NULL;
  ret->end = NULL;
  ret->size = 0;

  ret->cache = NULL;
  ret->cache_index = 0;

  sem_init(&ret->base.mutex, 0, 1);

  return ret;
}
void linked_list_destroy(LinkedList * linked_list)
{
  assert(linked_list);
  assert(linked_list->base.open_traversals == 0);

  linked_list_clear(linked_list);

  sem_destroy(&linked_list->base.mutex);
  _free(linked_list);
}
void linked_list_destroy_and_free(LinkedList * linked_list)
{
  assert(linked_list);
  assert(linked_list->base.open_traversals == 0);

  struct LinkedListNode * current = linked_list->start;
  while (current)
  {
    _free(any_to_ptr(current->value));
    current = current->next;
  }

  linked_list_destroy(linked_list);
}
void linked_list_destroy_and_user_free(LinkedList * linked_list, void (*callback)(void *))
{
  assert(linked_list);
  assert(linked_list->base.open_traversals == 0);
  assert(callback);

  struct LinkedListNode * current = linked_list->start;
  while (current)
  {
    callback(any_to_ptr(current->value));
    current = current->next;
  }

  linked_list_destroy(linked_list);
}
void linked_list_destroy_and(LinkedList * linked_list, void (*function)(Any))
{
  assert(linked_list);
  assert(linked_list->base.open_traversals == 0);
  assert(function);

  struct LinkedListNode * current = linked_list->start;
  while (current)
  {
    function(current->value);
    current = current->next;
  }

  linked_list_destroy(linked_list);
}


unsigned int linked_list_size(LinkedList * linked_list)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);

  unsigned int ret = linked_list->size;

  sem_post(&linked_list->base.mutex);

  return ret;
}


bool linked_list_contains(LinkedList * linked_list, Any any)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);

  bool ret = false;
  struct LinkedListNode * current = linked_list->start;

  while (current && !ret)
  {
    if (any_equals(current->value, any))
      ret = true;
    current = current->next;
  }

  sem_post(&linked_list->base.mutex);

  return ret;
}


Any linked_list_get(LinkedList * linked_list, unsigned int index)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);

  assert(index < linked_list->size);

  if (index == linked_list->size - 1)
  {
    Any ret = linked_list->end->value;
    sem_post(&linked_list->base.mutex);

    return ret;
  }

  unsigned int start_index;
  struct LinkedListNode * current;
  Any ret;

  linked_list_get_cached_current(linked_list, index, &current, &start_index);

  for (unsigned int k = start_index; k < index; k++)
  {
    current = current->next;
  }

  linked_list->cache = current;
  linked_list->cache_index = index;

  ret = current->value;

  sem_post(&linked_list->base.mutex);

  return ret;
}



char linked_list_get_char(LinkedList * linked_list, unsigned int index)
{
  assert(linked_list);

  return any_to_char(linked_list_get(linked_list, index));
}
int linked_list_get_int(LinkedList * linked_list, unsigned int index)
{
  assert(linked_list);

  return any_to_int(linked_list_get(linked_list, index));
}
char * linked_list_get_str(LinkedList * linked_list, unsigned int index)
{
  assert(linked_list);

  return any_to_str(linked_list_get(linked_list, index));
}
void * linked_list_get_ptr(LinkedList * linked_list, unsigned int index)
{
  assert(linked_list);

  return any_to_ptr(linked_list_get(linked_list, index));
}



void linked_list_add(LinkedList * linked_list, Any element)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  linked_list_add_imp(linked_list, element);

  sem_post(&linked_list->base.mutex);
}

void linked_list_add_range(LinkedList * linked_list, List * range)
{
  assert(linked_list);
  assert(range);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  ListTraversal * traversal = list_get_traversal(range);
  while (!list_traversal_completed(traversal))
  {
    Any val = list_traversal_next(traversal);

    linked_list_add_imp(linked_list, val);
  }

  sem_post(&linked_list->base.mutex);

}

void linked_list_set(LinkedList * linked_list, unsigned int index, Any element)
{

  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);
  assert(index < linked_list->size);

  struct LinkedListNode * current = linked_list->start;

  for (unsigned int k = 0; k < index; k++)
  {
    current = current->next;
  }

  current->value = element;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);
}

void linked_list_insert(LinkedList * linked_list, unsigned int index, Any element)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(index <= linked_list->size);
  assert(linked_list->base.open_traversals == 0);

  linked_list_insert_imp(linked_list, index, element);

  sem_post(&linked_list->base.mutex);
}

void linked_list_insert_range(LinkedList * linked_list, unsigned int index, List * range)
{
  assert(linked_list);
  assert(range);

  sem_wait(&linked_list->base.mutex);
  assert(index <= linked_list->size);
  assert(linked_list->base.open_traversals == 0);

  ListTraversal * trav = list_get_traversal(range);
  Any value;

  while (!list_traversal_completed(trav))
  {
    value = list_traversal_next(trav);
    linked_list_insert_imp(linked_list, index++, value);
  }

  sem_post(&linked_list->base.mutex);
}




Any linked_list_remove_at(LinkedList * linked_list, unsigned int index)
{

  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);
  assert(index < linked_list->size);

  Any ret =  linked_list_remove_at_imp(linked_list, index);

  sem_post(&linked_list->base.mutex);

  return ret;
}

unsigned int linked_list_remove(LinkedList * linked_list, Any any)
{
  return linked_list_remove_imp(linked_list, any, false);
}

unsigned int linked_list_remove_and_free(LinkedList * linked_list, Any any)
{
  return linked_list_remove_imp(linked_list, any, true);
}


void linked_list_swap(LinkedList * linked_list, unsigned int index_a, unsigned int index_b)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);
  assert(index_a < linked_list->size);
  assert(index_b < linked_list->size);

  if (index_a == index_b)
  {
    sem_post(&linked_list->base.mutex);
    return;
  }

  struct LinkedListNode * a = NULL, * b = NULL, * current;
  unsigned int current_index;
  Any temp;

  linked_list_get_cached_current(linked_list, utilities_uimin(index_a, index_b), &current, &current_index);

  while (!a || !b)
  {
    if (current_index == index_a)
      a = current;
    else if (current_index == index_b)
      b = current;

    current = current->next;
    current_index++;
  } 

  temp = a->value;
  a->value = b->value;
  b->value = temp;

  if (index_a < index_b)
  {
    linked_list->cache = a;
    linked_list->cache_index = index_a;
  }
  else
  {
    linked_list->cache = b;
    linked_list->cache_index = index_b;
  }

  sem_post(&linked_list->base.mutex);
}

void linked_list_reposition(LinkedList * linked_list, unsigned int from_index, unsigned int to_index)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);
  assert(from_index < linked_list->size);
  assert(to_index < linked_list->size);

  Any value;

  if (from_index == to_index)
  {
    sem_post(&linked_list->base.mutex);
    return;
  }

  value = linked_list_remove_at_imp(linked_list, from_index);
  linked_list_insert_imp(linked_list, to_index, value);

  sem_post(&linked_list->base.mutex);
}



void linked_list_clear(LinkedList * linked_list)
{

  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  struct LinkedListNode * current = linked_list->start, * hold;

  while (current)
  {
    hold = current;
    current = hold->next;
    _free(hold);
  }

  linked_list->start = NULL;
  linked_list->end = NULL;
  linked_list->size = 0;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);

}

void linked_list_clear_and_free(LinkedList * linked_list)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  struct LinkedListNode * current = linked_list->start, * hold;
  void * ptr;

  while (current)
  {
    hold = current;
    current = hold->next;
    ptr = any_to_ptr(hold->value);
    _free(ptr);
    _free(hold);
  }

  linked_list->start = NULL;
  linked_list->end = NULL;
  linked_list->size = 0;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);
}

void linked_list_clear_and_user_free(LinkedList * linked_list, void (*callback)(void *))
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  struct LinkedListNode * current = linked_list->start, * hold;
  void * ptr;

  while (current)
  {
    hold = current;
    current = hold->next;
    ptr = any_to_ptr(hold->value);
    callback(ptr);
    _free(hold);
  }

  linked_list->start = NULL;
  linked_list->end = NULL;
  linked_list->size = 0;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);
}

void linked_list_clear_and(LinkedList * linked_list, void (*function)(Any))
{
  assert(linked_list);
  assert(function);

  sem_wait(&linked_list->base.mutex);
  assert(linked_list->base.open_traversals == 0);

  struct LinkedListNode * current = linked_list->start, * hold;

  while (current)
  {
    hold = current;
    current = hold->next;
    function(hold->value);
    _free(hold);
  }

  linked_list->start = NULL;
  linked_list->end = NULL;
  linked_list->size = 0;

  linked_list->cache = NULL;
  linked_list->cache_index = 0;

  sem_post(&linked_list->base.mutex);
}


Any * linked_list_to_array(LinkedList * linked_list)
{
  assert(linked_list);

  Any * ret;

  sem_wait(&linked_list->base.mutex);
  ret = (Any *) _malloc(sizeof(Any) * linked_list->size);
  assert(ret);


  unsigned int index = 0;
  struct LinkedListNode * current = linked_list->start;
  while (current != NULL)
  {
    ret[index++] = current->value;
    current = current->next;
  }

  sem_post(&linked_list->base.mutex);

  return ret;
}

LinkedList * linked_list_sub_list(
  LinkedList * linked_list,
  unsigned int start,
  unsigned int end)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);

  assert(start <= end);
  assert(end <= linked_list->size);

  LinkedList * ret = linked_list_new();

  if (start == end)
  {
    sem_post(&linked_list->base.mutex);
    return ret;
  }

  unsigned int index = 0;
  struct LinkedListNode * current = linked_list->start;
  while (index < start)
  {
    current = current->next;
    index++;
  }

  while (index < end)
  {
    linked_list_add(ret, current->value);
    current = current->next;

    index++;
  }

  sem_post(&linked_list->base.mutex);


  return ret;
}

LinkedList * linked_list_clone(LinkedList * linked_list)
{
  assert(linked_list);

  sem_wait(&linked_list->base.mutex);

  LinkedList * ret = linked_list_new();

  struct LinkedListNode * current = linked_list->start;

  while (current)
  {
    linked_list_add(ret, current->value);
    current = current->next;
  }

  sem_post(&linked_list->base.mutex);

  return ret;
}

char * linked_list_to_string(LinkedList * linked_list)
{
  assert(linked_list);

  if (linked_list->size == 0)
    return strings_clone("[]");

  char * buffer;
  struct LinkedListNode * node = linked_list->start;
  StringBuilder * sb = string_builder_new();

  string_builder_append(sb, "[ ");

  buffer = any_get_string_representation(node->value);
  string_builder_append(sb, buffer);
  _free(buffer);

  do
  {
    node = node->next;
    if(!node)
      continue;

    buffer = any_get_string_representation(node->value);
    string_builder_append(sb, ", ");
    string_builder_append(sb, buffer);
    _free(buffer);
  }
  while (node);

  string_builder_append(sb, " ]");

  return string_builder_to_string_destroy(sb);
}

void linked_list_foreach(LinkedList * linked_list, void (*function)(Any))
{
  assert(linked_list);
  assert(function);

  sem_wait(&linked_list->base.mutex);
  linked_list->base.open_traversals++;
  sem_post(&linked_list->base.mutex);


  struct LinkedListNode * current = linked_list->start;

  while (current)
  {
    function(current->value);
    current = current->next;
  }

  sem_wait(&linked_list->base.mutex);
  linked_list->base.open_traversals--;
  sem_post(&linked_list->base.mutex);



}

/* end of methods for LinkedList */

/* methods for LinkedListTraversal */

LinkedListTraversal * linked_list_get_traversal(LinkedList * linked_list)
{
  assert(linked_list);

  LinkedListTraversal * ret = (LinkedListTraversal *) _malloc(sizeof(LinkedListTraversal));
  assert(ret);

  ret->base.type = LIST_TYPE_LINKED_LIST;
  ret->base.list = (List *) linked_list;
  ret->base.list_traversal_destroy = (void (*)(ListTraversal *)) linked_list_traversal_destroy;
  ret->base.list_traversal_next = (Any (*)(ListTraversal *)) linked_list_traversal_next;
  ret->base.list_traversal_next_char = (char (*)(ListTraversal *)) linked_list_traversal_next_char;
  ret->base.list_traversal_next_int = (int (*)(ListTraversal *)) linked_list_traversal_next_int;
  ret->base.list_traversal_next_str = (char * (*)(ListTraversal *)) linked_list_traversal_next_str;
  ret->base.list_traversal_next_ptr = (void * (*)(ListTraversal *)) linked_list_traversal_next_ptr;
  ret->base.list_traversal_completed = (bool (*)(ListTraversal *)) linked_list_traversal_completed;

  sem_wait(&linked_list->base.mutex);

  ret->current = linked_list->start;
  linked_list->base.open_traversals++;

  sem_post(&linked_list->base.mutex);

  return ret;
}

void linked_list_traversal_destroy(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  sem_wait(&linked_list_traversal->base.list->mutex);

  linked_list_traversal->base.list->open_traversals--;

  sem_post(&linked_list_traversal->base.list->mutex);

  _free(linked_list_traversal);
}


Any linked_list_traversal_next(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);
  assert(!linked_list_traversal_completed(linked_list_traversal));

  Any ret = linked_list_traversal->current->value;

  linked_list_traversal->current = linked_list_traversal->current->next;

  return ret;
}

char linked_list_traversal_next_char(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  return any_to_char(linked_list_traversal_next(linked_list_traversal));
}
int linked_list_traversal_next_int(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  return any_to_int(linked_list_traversal_next(linked_list_traversal));
}
char * linked_list_traversal_next_str(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  return any_to_str(linked_list_traversal_next(linked_list_traversal));
}
void * linked_list_traversal_next_ptr(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  return any_to_ptr(linked_list_traversal_next(linked_list_traversal));
}


bool linked_list_traversal_completed(LinkedListTraversal * linked_list_traversal)
{
  assert(linked_list_traversal);

  bool ret = linked_list_traversal->current == NULL;
  if (ret)
    linked_list_traversal_destroy(linked_list_traversal);

  return ret;
}


/* end of methods for LinkedListTraversal */
