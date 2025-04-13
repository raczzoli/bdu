/* 
 * Copyright (C) 2025 Zoltan Racz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"

struct queue_list *queue_new_list()
{
	struct queue_list *list = (struct queue_list *)malloc(sizeof(struct queue_list));

	if (!list) {
		printf("Error allocating memory for work queue list!\n");
		return NULL;
	}

	pthread_mutex_init(&list->lock, NULL);

	list->head = NULL;
	list->tail = NULL;
	list->num_elements = 0;

	return list;
}


struct queue_elem *queue_add_elem(struct queue_list *list, void *data)
{
	pthread_mutex_lock(&list->lock);

	struct queue_elem *elem = (struct queue_elem *)malloc(sizeof(struct queue_elem));

	if (!elem) {
		printf("Error allocating memory for queue element!\n");
		goto end;
	}

	elem->next = NULL;
	elem->prev = NULL;

	if (!list->tail) {
		list->tail = elem;
		list->head = elem;
	}
	else {
		list->tail->next = elem;
		elem->prev = list->tail;
		list->tail = elem;
	}

	elem->data = data;
	list->num_elements++;
	
end:	
	pthread_mutex_unlock(&list->lock);

	return elem;
}

struct queue_elem *queue_get_next_elem(struct queue_list *list)
{
	pthread_mutex_lock(&list->lock);

	struct queue_elem *elem = NULL;

	if (!list) {
		printf("List is NULL\n");
		goto end;
	}

	if (list->num_elements < 1) {
		//printf("No more elements in queue list...\n");
		goto end;
	}

	elem = list->head;

	if (!elem)
		goto end;

	list->head = elem->next;
	list->num_elements--;

	if (list->head) {
		list->head->prev = NULL;

		if (list->num_elements == 1)
			list->tail = list->head;
	}
	else 
		list->tail = NULL;

end:
	pthread_mutex_unlock(&list->lock);
	return elem;
}


void queue_free_elem(struct queue_elem *elem)
{
	if (!elem)
		return;

	free(elem);

	return;
}