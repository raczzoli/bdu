/* 
 * Copyright (C) 2025 Zoltán Rácz
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

#include <pthread.h>

#ifndef QUEUE_H
#define QUEUE_H

struct queue_elem {
	struct queue_elem *next;
	struct queue_elem *prev;
	void *data;
};

struct queue_list {
	struct queue_elem *head;
	struct queue_elem *tail;
	int num_elements;
	pthread_mutex_t lock;
};

struct queue_list *queue_new_list();
struct queue_elem *queue_get_next_elem(struct queue_list *list);
struct queue_elem *queue_add_elem(struct queue_list *list, void *data);
void queue_free_elem(struct queue_elem *elem);


#endif //QUEUE_H