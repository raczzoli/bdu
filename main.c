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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>
#include <time.h>

#include "bdu.h"
#include "queue.h"
#include "output.h"

#define NUM_THREADS 12

char root_path[PATH_MAX];
int max_depth = 1;
char output_format[6];

size_t sizeB = 0;

int active_workers = 0;

pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

struct option cmdline_options[] =
	{
		/* These options set a flag. */
		//{"verbose", no_argument,       &verbose_flag, 1},
		//{"brief",   no_argument,       &verbose_flag, 0},
		/* These options don’t set a flag.
			We distinguish them by their indices. */
		{"max-depth",     required_argument, NULL, 'd'},
		{"output-format",     required_argument, NULL, 'o'},
		/*
		{"append",  no_argument,       0, 'b'},
		{"delete",  required_argument, 0, 'd'},
		{"create",  required_argument, 0, 'c'},
		{"file",    required_argument, 0, 'f'},
		*/
		{0, 0, 0, 0}
	};

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

int compare_dentries(const void* a, const void* b) 
{
	struct dir_entry *dentry_a = *(struct dir_entry **)a;
    struct dir_entry *dentry_b = *(struct dir_entry **)b;

	return dentry_b->bytes > dentry_a->bytes 
				? 1 
				: (dentry_b->bytes == dentry_a->bytes ? 0 : -1);
}
 
void sort_dentries(struct dir_entry *head, int depth)
{
	int i;

	/**
	** we only sort the the displayed children.
	** ex. if --max-depth=1 then we sort only the first level of children, 
	** since the rest is not displayed, no need to sort it
	**/
	if (depth == max_depth)
		return;
	
	if (head->children_len > 0) {
		qsort(head->children, head->children_len, sizeof(struct dir_entry *), compare_dentries);
		for (i=0;i<head->children_len;i++) {
			sort_dentries(head->children[i], ++depth);
		}
	}
}

int parse_args(int argc, char *argv[])
{
	int option_index = 0;
	int c;

	while (1) {
		c = getopt_long (argc, argv, "abc:d:f:",
			cmdline_options, &option_index);

		switch(c) {
			case 'd': // --max-depth or -d option
				max_depth = atoi(optarg);
				break;
			case 'o':
				strncpy(output_format, optarg, strlen(optarg));
				break;
		}

		if (c == -1)
			break;
	}

	if (strlen(output_format) < 1)
		strcpy(output_format, "plain");

	/**
	** checking for the argument containing the path to be scanned
	**/
	if (argc > optind) {
		int path_len = strlen(argv[optind]);
		strncpy(root_path, argv[optind], path_len);

		if (root_path[path_len-1] != '/') {
			root_path[path_len] = '/';
			path_len++;
		}

		root_path[path_len] = '\0';
	}
	else {
		/**
		** if no path was specified in the command line options we default it to "./"
		**/
		root_path[0] = '.';
		root_path[1] = '/';
		root_path[2] = '\0';
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	struct dir_entry *root_entry;
	struct thread_data *tdata;
	pthread_t threads[NUM_THREADS];
	struct queue_list *list = NULL;
	
	time_t start = time(NULL);

	ret = parse_args(argc, argv);

	if (ret < 0) {
		printf("Error parsing command line arguments!\n");
		return -1;
	}
	
	list = queue_new_list();

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

	sort_dentries(root_entry, 0);
	output_print(stdout, root_entry, output_format, max_depth);

    time_t end = time(NULL);
    double elapsed = difftime(end, start);

	printf("-------------------------------------------\n");
	printf("Number of threads used: %d\n", NUM_THREADS);
	printf("Active workers at the end: %d\n", active_workers);
	printf("Took: %.2f seconds\n", elapsed);

	return 0;
}


