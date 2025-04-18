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
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>
#include <unistd.h>

#include "bdu.h"
#include "dir.h"
#include "queue.h"
#include "output.h"
#include "utils.h"

#define NUM_THREADS_DEFAULT 12

char output_file_path[PATH_MAX];
int output_file_path_len;

struct dir_entry **root_entries;
int root_entries_len = 0;
int show_file_mtime = 0;
int show_help = 0;
int show_summary = 0;
int show_in_bytes = 0;
int show_no_leading_tabs = 0;
int num_threads = 0;

int max_depth = -1;
long unsigned int warn_at_bytes = 0;
long unsigned int critical_at_bytes = 0;

int sort_flags = 0;

char output_format[6];

pthread_t **threads;
int active_workers = 0;

struct queue_list *qlist = NULL;

pthread_mutex_t active_workers_mutex = PTHREAD_MUTEX_INITIALIZER;

struct option cmdline_options[] =
	{
		// options without arguments
		{"summarize",     no_argument, NULL, 's'},
		{"in-bytes",     no_argument, &show_in_bytes, 1},
		{"no-leading-tabs",     no_argument, &show_no_leading_tabs, 1},
		{"time",     no_argument, &show_file_mtime, 1},
		{"help",     no_argument, &show_help, 1},

		// options with argument
		{"max-depth",     required_argument, NULL, 'd'},
		{"output-format",     required_argument, NULL, 'o'},
		{"threads",     required_argument, NULL, 0},
		{"warn-at",     required_argument, NULL, 0},
		{"critical-at",     required_argument, NULL, 0},
		{"output-file",     required_argument, NULL, 0},
		{"sort-by",     required_argument, NULL, 0},
		{"sort-order",     required_argument, NULL, 0},

		{0, 0, 0, 0}
	};
 
static int parse_args(int argc, char *argv[]);
static int get_num_cpu_cores();
static void print_help();
static int process_output();


int add_root_entry_if_directory(char *path)
{
	struct stat st;
	if (lstat(path, &st) == -1) {
		printf("Error while lstat path %s (%s)\n", path, strerror(errno));
		return -1;
	}

	/**
	** we check if the current path is a directory or something else.
	** we (obviously) don`t add only folders to the root_entries stack
	**/
	if (!S_ISDIR(st.st_mode)) 
		return -1;

	if (root_entries_len == 0)
		root_entries = calloc(root_entries_len+1, sizeof(char *));
	else 
		root_entries = realloc(root_entries, (root_entries_len+1) * sizeof(char *));

	if (!root_entries) {
		printf("Error allocating memory for root entries array!\n");
		return -1;
	}

	struct dir_entry *d = dir_create_dentry(path);

	if (!d) {
		printf("Error allocating memory for root entry!\n");
		return -1;
	}

	root_entries[root_entries_len] = d;
	root_entries_len++;

	queue_add_elem(qlist, d);

	return 0;
}

int process_files_args(int argc, char **argv)
{
	/**
	** checking for the argument containing the path to be scanned
	**/
	if (argc > optind) {
		for (int i=optind;i<argc;i++) {
			add_root_entry_if_directory(argv[i]);
		}
	}
	else {
		/**
		** if no path was specified in the command line options we default it to "./"
		**/
		add_root_entry_if_directory("./");
	}

	return 0;
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

void subdir_scan_callback(struct dir_entry *d)
{
	queue_add_elem(qlist, d);
}

void* thread_worker() 
{
	struct queue_elem *elem = NULL;
	struct dir_entry *dentry = NULL;

	while (1) {
		elem = queue_get_next_elem(qlist);

		if (!elem) {
			if (num_active_workers() == 0) {
				return NULL;
			}
			
			continue;
		}

		increment_active_workers();

		dentry = (struct dir_entry *)elem->data;
		dir_scan(dentry, subdir_scan_callback, show_file_mtime);

		decrement_active_workers();

		queue_free_elem(elem);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	time_t start = time(NULL);

	ret = parse_args(argc, argv);

	if (ret < 0) 
		return -1;

		
	if (show_help > 0) {
		print_help();
		return 0;
	}

	/**
	** if sort-order was not set, we default it to desc
	**/
	if (!(sort_flags & SORT_FULL_MASK))
		sort_flags |= SORT_DESC;

	/**
	** if sort-by was not set, we default it to size
	**/
	if (!(sort_flags & SORT_BY_FULL_MASK))
		sort_flags |= SORT_BY_SIZE;


	/**
	** we check if qlist has been initialized. If not, we initialize it. 
	** If queue_new_list() returns NULL, no need to go further
	**/
	if (!qlist) {
		qlist = queue_new_list();
		if (!qlist) {
			printf("Error allocating memory for scan queue list!\n");
			return -1;
		}
	}

	process_files_args(argc, argv);

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


	threads = calloc(num_threads, sizeof(pthread_t *));

	for (int i = 0; i < num_threads; i++) {
		threads[i] = (pthread_t *) malloc(sizeof(pthread_t));

		pthread_create(threads[i], NULL, thread_worker, NULL);
	}

	for (int i = 0; i < num_threads; i++) {
		pthread_join(*(threads[i]), NULL);
	}

	printf("-------------------------------------------\n");

	dir_sort_entries(root_entries, root_entries_len, max_depth, 0, sort_flags);

	process_output();

	dir_free_entries(root_entries, root_entries_len);

	time_t end = time(NULL);
	double elapsed = difftime(end, start);

	printf("-------------------------------------------\n");
	printf("Number of threads used: %d\n", num_threads);
	printf("Active workers at the end: %d\n", active_workers);
	printf("Took: %.2f seconds\n", elapsed);

	return 0;
}

static int parse_args(int argc, char *argv[])
{
	struct option opt;
	int option_index = 0;
	int c;

	while (1) {
		c = getopt_long (argc, argv, "shd:o:",
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
				else if (strcmp(opt.name, "warn-at") == 0)
					warn_at_bytes = human_size_to_bytes(optarg);
				else if (strcmp(opt.name, "critical-at") == 0)
					critical_at_bytes = human_size_to_bytes(optarg);
				else if (strcmp(opt.name, "output-file") == 0) {
					strcpy(output_file_path, optarg);
					output_file_path_len = strlen(output_file_path);
				}
				else if (strcmp(opt.name, "sort-by") == 0) {
					if (strcmp(optarg, "size") == 0) 
						sort_flags |= SORT_BY_SIZE;
					else if (strcmp(optarg, "name") == 0)
						sort_flags |= SORT_BY_NAME;
					else if (strcmp(optarg, "date") == 0)
						sort_flags |= SORT_BY_DATE;
					else {
						printf("Invalid sort field! Should be \"size\", \"name\" or \"date\".");
						return -1;
					}
				}
				else if (strcmp(opt.name, "sort-order") == 0) {
					if (strcmp(optarg, "asc") == 0) 
						sort_flags |= SORT_ASC;
					else if (strcmp(optarg, "desc") == 0)
						sort_flags |= SORT_DESC;
					else {
						printf("Invalid sort order! Should be \"asc\" or \"desc\"");
						return -1;
					}
				}
				break;
		}

		if (c == -1)
			break;
	}

	return 0;
}

static int get_num_cpu_cores()
{
	long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

	if (num_cores < 1) {
		printf("Error detecting number of CPU cores, returning default of: %d cores.\n", NUM_THREADS_DEFAULT);
		return NUM_THREADS_DEFAULT;
	}

    return num_cores;
}

static int process_output()
{
	struct output_options output_opts = {.no_styles=0, .human_readable=0};
	FILE *out;

	output_opts.max_depth = max_depth;
	output_opts.show_warn_at_bytes = warn_at_bytes;
	output_opts.show_critical_at_bytes = critical_at_bytes;
	output_opts.human_readable = !show_in_bytes;
	output_opts.no_leading_tabs = show_no_leading_tabs;
	

	if (output_file_path_len) {
		FILE *fp = fopen(output_file_path, "w");		

		if (!fp) {
			printf("Error opening output file path \"%s\" for writing!", output_file_path);
			return -1;
		}

		out = fp;
		// when writing to a file we don`t want any styles (colors for ex.)
		output_opts.no_styles = 1; 
	}
	else 
		out = stdout;

	output_print(out, root_entries, root_entries_len, output_format, output_opts);

	return 0;
}

static void print_help() 
{
    printf("Usage: bdu [OPTIONS] [DIRECTORY...]\n");
    printf("Multithreaded disk usage analyzer inspired by the classic \"du\" command, written from scratch.\n\n");
    printf("Options:\n");
    printf("  -s, --summarize                     Display only the total size for each argument\n");
    printf("  -d, --max-depth=N                   Limit depth of directory traversal\n");
    printf("  -o, --output-format=FMT             Output format: \"text\", \"json\" or \"html\"\n");
    printf("      --threads=N                     Number of threads to use\n");
    printf("      --time                          Show last file modification time\n");
	printf("      --sort-by=[FIELD]               The field sorting whould be done after - \"size\", \"name\" or \"date\"\n");
	printf("      --sort-order=[asc/desc]         Ascending or descending order\n");
	printf("      --warn-at=[VALUE][UNIT]         If set and the size of the entry is greater than this value, the size will be printed in yellow\n");
	printf("                                         ex: --warn-at=100M, warn-at=1G etc.\n");
	printf("      --critical-at=[VALUE][UNIT]     If set and the size of the entry is greater than this value, the size will be printed in red\n");
	printf("                                         ex: --critical-at=10G, critical-at=50G etc.\n");
	printf("      --output-file=[FILE_PATH]       Writes the output to the given file path\n");
	printf("\n");
	printf("  -h, --help                          Show this help message and exit\n");
	printf("\n");
	printf("***** more arguments to be implemented soon (i hope) *****\n");
    printf("\n");
}
