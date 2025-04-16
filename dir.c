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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dir.h"

struct dir_entry *dir_create_dentry(char *path)
{
	struct dir_entry *entry = (struct dir_entry *)malloc(sizeof(struct dir_entry));
	int leading_slash = 1;

	if (!entry) {
		printf("Error allocating memory for dentry!\n");
		return NULL;
	}

	entry->path_len = strlen(path);

	if (path[entry->path_len-1] != '/') {
		entry->path_len++;
		leading_slash = 0;
	}

	entry->path = (char *) malloc(entry->path_len+1); // +1 for NULL
	snprintf(entry->path, entry->path_len+1, "%s%s", path, !leading_slash ? "/" : "");

	entry->last_mdate = NULL;
	entry->bytes = 0;
	entry->children = NULL;
	entry->parent = NULL;
	entry->children_len = 0;
	
	pthread_mutex_init(&entry->lock, NULL);

	return entry;
}

struct dir_entry *dir_scan(struct dir_entry *dentry, void (dentry_scan_fn)(struct dir_entry*), int proc_mtime)
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
	** if proc_mtime = 1 we extract the date of the last modification to the entry, 
	** and store it in dchild->last_mdate
	**/
	if (proc_mtime) 
		dentry->last_mdate = dir_get_dentry_mdate(st.st_mtime);

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
			if (S_ISREG(st.st_mode)) {
				// we only count regular files
				dir_sum_dentry_bytes(dentry, st.st_blocks * 512);
			}

			continue;
		}

		full_path[strlen(full_path)] = '/';

		dchild = dir_create_dentry(full_path);

		if (!dchild) {
			closedir(dir);
			return NULL;
		}

		/**
		** if show_file_mtime = 1 we extract the date of the last modification to the entry, 
		** and store it in dchild->last_mdate
		**/
		if (proc_mtime) 
			dchild->last_mdate = dir_get_dentry_mdate(st.st_mtime);

		dchild->parent = dentry;

		if (dentry->children_len == 0)
			dentry->children = calloc(1, sizeof(struct dir_entry *));
		else 
			dentry->children = realloc(dentry->children, (dentry->children_len+1) * sizeof(struct dir_entry *));

		dentry->children[dentry->children_len] = dchild;
		dentry->children_len++;

		if (dentry_scan_fn)
			dentry_scan_fn(dchild);
	}

	closedir(dir);

	return dentry;
}

void dir_sum_dentry_bytes(struct dir_entry *dentry, long int bytes)
{
	pthread_mutex_lock(&dentry->lock);
	dentry->bytes += bytes;
	pthread_mutex_unlock(&dentry->lock);

	if (dentry->parent)
		dir_sum_dentry_bytes(dentry->parent, bytes);
}

char *dir_get_dentry_mdate(time_t mtime) 
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

int dir_free_entries(struct dir_entry **entries, int entries_len)
{
	if (!entries || entries_len <= 0) 
		return -1;

	for (int i=0;i<entries_len;i++)
		dir_free_entry(entries[i]);

	free(entries);

	return 0;
}

int dir_free_entry(struct dir_entry *head)
{
	if (!head)
		return -1;

	if (head->children_len > 0) 
		for (int i=0;i<head->children_len;i++) 
			dir_free_entry(head->children[i]);


	free(head->path);

	if (head->children)
		free(head->children);

	if (head->last_mdate)
		free(head->last_mdate);

	pthread_mutex_destroy(&head->lock);

	free(head);

	return 0;
}