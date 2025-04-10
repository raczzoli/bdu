#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>

#include "bdu.h"
#include "queue.h"

#define NUM_THREADS 12

char *root_path = "/home/rng89/";
int max_depth = 1;
size_t sizeB = 0;

int active_workers = 0;

pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

void increment_active_workers()
{
	pthread_mutex_lock(&active_workers_mutex);
	active_workers++;
	pthread_mutex_unlock(&active_workers_mutex);
}

void decrement_active_workers()
{
	pthread_mutex_lock(&active_workers_mutex);
	active_workers--;
	pthread_mutex_unlock(&active_workers_mutex);
}

int num_active_workers()
{
	int num = 0;

	pthread_mutex_lock(&active_workers_mutex);
	num = active_workers;
	pthread_mutex_unlock(&active_workers_mutex);

	return num;
}

struct dir_entry *dir_create_dentry(char *path)
{
	struct dir_entry *entry = (struct dir_entry *)malloc(sizeof(struct dir_entry));

	if (!entry) {
		printf("Error allocating memory for dentry!\n");
		return NULL;
	}

	entry->path = strdup(path);
	entry->path_len = strlen(entry->path);
	entry->bytes = 0;
	entry->children = NULL;
	entry->parent = NULL;
	entry->children_len = 0;
	
	return entry;
}

void dir_sum_dentry_bytes(struct dir_entry *dentry, long int bytes)
{
	dentry->bytes += bytes;

	if (dentry->parent)
		dir_sum_dentry_bytes(dentry->parent, bytes);
}

struct dir_entry *scan_dir(struct dir_entry *dentry, struct queue_list *qlist)
{
	struct dir_entry *dchild;
	char full_path[PATH_MAX];

	DIR *dir = opendir(dentry->path);

	if (!dir) {
		printf("Error opening path: %s\n", dentry->path);
		return NULL;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		memset(full_path, 0, PATH_MAX-1);

		sprintf(full_path, "%s%s", dentry->path, entry->d_name);

		struct stat st;
		if (lstat(full_path, &st) == -1) {
			printf("Error while lstat path %s\n", full_path);
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			dir_sum_dentry_bytes(dentry, st.st_blocks * 512);

			pthread_mutex_lock(&stats_mutex);
			sizeB += st.st_size;
			pthread_mutex_unlock(&stats_mutex);
			
			continue;
		}

		full_path[strlen(full_path)] = '/';

		dchild = dir_create_dentry(full_path);

		if (!dchild) {
			closedir(dir);
			return NULL;
		}

		dchild->parent = dentry;

		if (dentry->children_len == 0)
			dentry->children = calloc(1, sizeof(struct dir_entry *));
		else 
			dentry->children = realloc(dentry->children, (dentry->children_len+1) * sizeof(struct dir_entry *));

		dentry->children[dentry->children_len] = dchild;
		dentry->children_len++;

		queue_add_elem(qlist, dchild);
	}

	closedir(dir);

	return dentry;
}

void* thread_worker(void *arg) 
{
	struct thread_data *tdata = (struct thread_data *)arg;
	struct queue_list *list = tdata->list;
	struct queue_elem *elem = NULL;
	struct dir_entry *dentry = NULL;

	while (1) {
		elem = queue_get_next_elem(list);

		if (!elem) {
			if (num_active_workers() == 0) {
				//printf("Thread %d closing (no more active workers)...\n", tdata->thread_id);
				return NULL;
			}
			
			continue;
		}

		increment_active_workers();

		dentry = (struct dir_entry *)elem->data;
		scan_dir(dentry, list);

		decrement_active_workers();

		queue_free_elem(elem);
	}

	return NULL;
}

void print_size(long int bytes)
{
	const char *units[] = { "B", "K", "M", "G", "T", "P" };
	double fin_size = bytes;
	int unit_cntr = 0;

	while (1) {
		if (fin_size >= 1024) {
			fin_size /= 1024;
			unit_cntr++;
		}
		else 
			break;
	}

	printf("%7.2f%s ", fin_size, units[unit_cntr]);
}

void print_dentries(struct dir_entry *head, int depth) 
{	
	int i=0;
	for (i=0;i<depth;i++)
		printf("\t");

	print_size(head->bytes);
	printf("%s\n", head->path);

	if (depth >= max_depth)
		return;

	if (head->children_len > 0)
		for (i=0;i<head->children_len;i++) {
			print_dentries(head->children[i], depth+1);
		}
}


int main()
{
	/**
	** elements: 279069
	**/
	struct dir_entry *root_entry;
	
	struct thread_data *tdata;
	pthread_t threads[NUM_THREADS];

	struct queue_list *list = queue_new_list();

	if (!list)
		return -1;

	root_entry = dir_create_dentry(root_path);

	if (!root_entry)
		return -1;

	queue_add_elem(list, root_entry);

	for (int i = 0; i < NUM_THREADS; i++) {
		tdata = (struct thread_data *)malloc(sizeof(struct thread_data));
		tdata->thread_id = i;
		tdata->list = list;

        pthread_create(&threads[i], NULL, thread_worker, (void *)tdata);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

	printf("-------------------------------------------\n");
	print_dentries(root_entry, 0);
	printf("-------------------------------------------\n");
	printf("Number of threads used: %d\n", NUM_THREADS);
	printf("Active workers at the end: %d\n", active_workers);

	return 0;
}
