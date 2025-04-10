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