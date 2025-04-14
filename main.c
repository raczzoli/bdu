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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>
#include <time.h>
#include <unistd.h>

#include "bdu.h"
#include "dir.h"
#include "queue.h"
#include "output.h"

#define NUM_THREADS_DEFAULT 12

char root_path[PATH_MAX];
int max_depth = 1;
int show_file_mtime = 0;
int show_help = 0;
int show_summary = 0;
int num_threads = 0;
char output_format[6];

pthread_t **threads;
int active_workers = 0;

pthread_mutex_t active_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

struct option cmdline_options[] =
	{
		// options without arguments
		{"summarize",     no_argument, NULL, 's'},
		{"time",     no_argument, &show_file_mtime, 1},
		{"help",     no_argument, &show_help, 1},

		// options with argument
		{"max-depth",     required_argument, NULL, 'd'},
		{"output-format",     required_argument, NULL, 'o'},
		{"threads",     required_argument, NULL, 0},

		{0, 0, 0, 0}
	};
 
static int get_num_cpu_cores();

void print_help() 
{
    printf("Usage: bdu [OPTIONS] [DIRECTORY...]\n");
    printf("Multithreaded disk usage analyzer inspired by the classic \"du\" command, written from scratch.\n\n");
    printf("Options:\n");
    printf("  -s, --summarize           Display only the total size for each argument\n");
    printf("  -d, --max-depth=N         Limit depth of directory traversal\n");
    printf("  -o, --output-format=FMT   Output format: \"text\" or \"json\"\n");
    printf("      --threads=N           Number of threads to use\n");
    printf("      --time                Show last file modification time\n");
    printf("  -h, --help                Show this help message and exit\n");
    printf("\n");
}

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

struct dir_entry *create_dentry(char *path)
{
	struct dir_entry *entry = dir_create_dentry(path);
	
	if (!entry)
		return NULL;

	pthread_mutex_init(&entry->lock, NULL);

	return entry;
}

void dir_sum_dentry_bytes(struct dir_entry *dentry, long int bytes)
{
	pthread_mutex_lock(&dentry->lock);
	dentry->bytes += bytes;
	pthread_mutex_unlock(&dentry->lock);

	if (dentry->parent)
		dir_sum_dentry_bytes(dentry->parent, bytes);
}

char *get_dentry_mdate(time_t mtime) 
{
	char *date_str = (char *) malloc(20);

	if (!date_str) {
		printf("Error allocating memory for date buffer!");
		return NULL;
	}

	struct tm *tm_info = localtime(&mtime);
	strftime(date_str, 20, "%Y-%m-%d %H:%M:%S", tm_info);

	return date_str;
}

struct dir_entry *scan_dir(struct dir_entry *dentry, struct queue_list *qlist)
{
	struct dir_entry *dchild;
	char full_path[PATH_MAX];
	struct stat st;

	// we don`t list contents of /proc and /run
	if (strcmp(dentry->path, "/proc/") == 0 || strcmp(dentry->path, "/run/") == 0) 
		return NULL;


	DIR *dir = opendir(dentry->path);

	if (!dir) {
		printf("Error opening path: %s (%s)\n", dentry->path, strerror(errno));
		return NULL;
	}

	if (lstat(dentry->path, &st) == -1) {
		printf("Error while lstat path %s (%s)\n", dentry->path, strerror(errno));
		return NULL;
	}

	/**
	** if show_file_mtime = 1 we extract the date of the last modification to the entry, 
	** and store it in dchild->last_mdate
	**/
	if (show_file_mtime) 
		dentry->last_mdate = get_dentry_mdate(st.st_mtime);

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		// we don`t list contents of /proc and /run
		if (strcmp(entry->d_name, "proc") == 0 || strcmp(entry->d_name, "run") == 0) 
			continue;

		memset(full_path, 0, PATH_MAX-1);

		sprintf(full_path, "%s%s", dentry->path, entry->d_name);
	
		if (lstat(full_path, &st) == -1) {
			printf("Error while lstat path %s (%s)\n", full_path, strerror(errno));
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			dir_sum_dentry_bytes(dentry, st.st_blocks * 512);

			continue;
		}

		full_path[strlen(full_path)] = '/';

		dchild = create_dentry(full_path);

		if (!dchild) {
			closedir(dir);
			return NULL;
		}

		/**
		** if show_file_mtime = 1 we extract the date of the last modification to the entry, 
		** and store it in dchild->last_mdate
		**/
		if (show_file_mtime) 
			dchild->last_mdate = get_dentry_mdate(st.st_mtime);

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
	** we only sort the displayed children.
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
	struct option opt;
	int option_index = 0;
	int c;

	while (1) {
		c = getopt_long (argc, argv, "sd:o:",
			cmdline_options, &option_index);

		switch(c) {
			case 'd': // --max-depth or -d option
				max_depth = atoi(optarg);
				break;
			case 'o':
				strncpy(output_format, optarg, strlen(optarg));
				break;
			case 's':
				show_summary = 1;
				break;
			case 0:
				opt = cmdline_options[option_index];

				if (strcmp(opt.name, "threads") == 0) 
					num_threads = atoi(optarg);

				break;
		}

		if (c == -1)
			break;
	}


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
	struct queue_list *list = NULL;
	
	time_t start = time(NULL);

	ret = parse_args(argc, argv);

	if (ret < 0) {
		printf("Error parsing command line arguments!\n");
		return -1;
	}
	
	if (show_help > 0) {
		print_help();
		return 0;
	}


	/**
	** we check if the user didn`t for some reason set --threads=0
	** if it did, we set it to 1
	**/
	if (num_threads <= 0) 
		num_threads = get_num_cpu_cores();

	/**
	** if -s or --summarize was set it contradicts with --max-depth option, 
	** so we set max_depth = 0 which basically means summary
	**/
	if (show_summary > 0)
		max_depth = 0;

	/**
	** if no --output argument was specified, we set the output_format 
	** to plain text
	**/
	if (strlen(output_format) < 1)
		strcpy(output_format, "text");


	list = queue_new_list();

	if (!list)
		return -1;

	root_entry = create_dentry(root_path);

	if (!root_entry)
		return -1;

	queue_add_elem(list, root_entry);
	
	threads = calloc(num_threads, sizeof(pthread_t *));
	

	for (int i = 0; i < num_threads; i++) {
		threads[i] = (pthread_t *) malloc(sizeof(pthread_t));

		tdata = (struct thread_data *)malloc(sizeof(struct thread_data));
		tdata->thread_id = i;
		tdata->list = list;

		pthread_create(threads[i], NULL, thread_worker, (void *)tdata);
	}

	for (int i = 0; i < num_threads; i++) {
		pthread_join(*(threads[i]), NULL);
	}

	printf("-------------------------------------------\n");

	sort_dentries(root_entry, 0);
	output_print(stdout, root_entry, output_format, max_depth);

	time_t end = time(NULL);
	double elapsed = difftime(end, start);

	printf("-------------------------------------------\n");
	printf("Number of threads used: %d\n", num_threads);
	printf("Active workers at the end: %d\n", active_workers);
	printf("Took: %.2f seconds\n", elapsed);

	return 0;
}

int get_num_cpu_cores()
{
	long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

	if (num_cores < 1) {
		printf("Error detecting number of CPU cores, returning default of: %d cores.\n", NUM_THREADS_DEFAULT);
		return NUM_THREADS_DEFAULT;
	}

    return num_cores;
}