#ifndef BDU_H
#define BDU_H

struct dir_entry {
	char *path;
	int path_len;
	size_t bytes;
	struct dir_entry *parent;
	struct dir_entry **children;
	int children_len;
};

struct thread_data {
	int thread_id;
	struct queue_list *list;
};

#endif //BDU_H