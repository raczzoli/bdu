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

static unsigned int sort_flags;
static ino_t *hardlinked_inodes;
static int num_hardlinked_inodes = 0;

static pthread_mutex_t inodes_check_mutex = PTHREAD_MUTEX_INITIALIZER;

static int sort_entries_cb(const void* a, const void* b);
static int isreg_hardlinked_ino(ino_t inode_num);

struct dir_entry *dir_create_dentry(char *path)
{
	struct dir_entry *entry = (struct dir_entry *)malloc(sizeof(struct dir_entry));

	if (!entry) {
		printf("Error allocating memory for dentry!\n");
		return NULL;
	}

	entry->path_len = strlen(path);

	/**
	** with the exception of the very root path "/", 
	** we don`t want trailing "/" in the path of the entry.
	** Having paths like /var/lib/some_subentry/ (with the "/"
	** at the end, fucks up the lstat, and even if /var/lib/some_subentry
	** would be a symlink, having a trailing "/" it would result in a directory
	** and we want to ignore them)
	**/
	if (entry->path_len > 1 && path[entry->path_len-1] == '/') 
		entry->path_len--;

	entry->path = (char *) malloc(entry->path_len+1); // +1 for NULL
	snprintf(entry->path, entry->path_len+1, "%s", path);

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
	if (strcmp(dentry->path, "/proc") == 0 || strcmp(dentry->path, "/run") == 0) 
		return NULL;

	if (lstat(dentry->path, &st) == -1) {
		printf("Error while lstat parent path %s (%s)\n", dentry->path, strerror(errno));
		return NULL;
	}

	if (!S_ISDIR(st.st_mode)) // it might be a symlink or a regular file by mistake
		return NULL;

	DIR *dir = opendir(dentry->path);

	if (!dir) {
		printf("Error opening path: %s (%s)\n", dentry->path, strerror(errno));
		return NULL;
	}

	/**
	** if proc_mtime = 1 we extract the date of the last modification to the entry, 
	** and store it in dchild->last_mdate
	**/
	if (proc_mtime) 
		dentry->last_mdate = dir_get_dentry_mdate(st.st_mtime);

	/**
	** last_mtime is stored all the time because it might be used
	** while sorting by date - even if proc_mtime is 0
	**/
	dentry->last_mtime = st.st_mtime;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		// we don`t list contents of /proc and /run
		if (strcmp(entry->d_name, "proc") == 0 || strcmp(entry->d_name, "run") == 0) 
			continue;

		memset(full_path, 0, PATH_MAX-1);

		/**
		** Having a trailing "/" at the end of the dentry->path (parent directory) 
		** is only allowed if "/" is the only character the path contains, 
		** aka the scanned directory is the very root. Otherwise the parent entry 
		** path shouldn`t contain the "/" character. We make sure of this in the 
		** dir_create_dentry function.
		**/ 
		if (dentry->path_len == 1 && dentry->path[0] == '/')
			sprintf(full_path, "/%s", entry->d_name);
		else 
			sprintf(full_path, "%s/%s", dentry->path, entry->d_name);
	
		if (lstat(full_path, &st) == -1) {
			printf("Error while lstat path %s (%s)\n", full_path, strerror(errno));
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			if (S_ISREG(st.st_mode)) { // we only count regular files
				if (st.st_nlink > 1) { // if hardlink, we check if we already summed it
					if (isreg_hardlinked_ino(st.st_ino)) 
						continue;
				}
				
				dir_sum_dentry_bytes(dentry, st.st_blocks * 512);
			}
			continue;
		}

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

		if (dentry_scan_fn)
			dentry_scan_fn(dchild);
	}

	closedir(dir);

	return dentry;
}

void dir_sort_entries(struct dir_entry **entries, int entries_len, int max_depth, int depth, int flags)
{
	if (!entries || !entries_len)
		return;

	sort_flags = flags;

	qsort(entries, entries_len, sizeof(struct dir_entry *), sort_entries_cb);

	/**
	** we only sort the displayed children.
	** ex. if --max-depth=1 then we sort only the first level of children, 
	** since the rest is not displayed, no need to sort it
	**/
	if (max_depth >= 0 && depth == max_depth)
		return;

	for (int i=0;i<entries_len;i++) {
		struct dir_entry *head = entries[i];
		if (head->children_len)
			dir_sort_entries(head->children, head->children_len, max_depth, depth+1, flags);
	}
}

static int sort_entries_cb(const void* a, const void* b) 
{
	int ret = 0;
	struct dir_entry *dentry_a = *(struct dir_entry **)a;
	struct dir_entry *dentry_b = *(struct dir_entry **)b;

	if (sort_flags & SORT_BY_SIZE) 
		ret = dentry_a->bytes < dentry_b->bytes 
				? -1 
				: (dentry_a->bytes == dentry_b->bytes ? 0 : 1);
	else if (sort_flags & SORT_BY_NAME) 
		ret = strcasecmp(dentry_a->path, dentry_b->path);
	else if (sort_flags & SORT_BY_DATE) 
		ret = dentry_a->last_mtime < dentry_b->last_mtime 
				? -1 
				: (dentry_a->last_mtime == dentry_b->last_mtime ? 0 : 1);

	return (sort_flags & SORT_ASC) ? ret : -ret;
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

	if (head->children) {
		dir_free_entries(head->children, head->children_len);
	}

	free(head->path);

	if (head->last_mdate) {
		free(head->last_mdate);
	}

	pthread_mutex_destroy(&head->lock);

	free(head);

	return 0;
}

int dir_cleanup()
{
	if (hardlinked_inodes) {
		free(hardlinked_inodes);
		hardlinked_inodes = NULL;
	}
	
	pthread_mutex_destroy(&inodes_check_mutex);

	return 0;
}

static int isreg_hardlinked_ino(ino_t inode_num)
{
	int found = 0;

	pthread_mutex_lock(&inodes_check_mutex);

	if (num_hardlinked_inodes > 0) {
		for (int i=0;i<num_hardlinked_inodes;i++) {
			if (hardlinked_inodes[i] == inode_num) {
				found = 1;
				goto end;
			}
		}
	}

	if (num_hardlinked_inodes == 0)
		hardlinked_inodes = calloc(1, sizeof(ino_t));
	else 
		hardlinked_inodes = realloc(hardlinked_inodes, (num_hardlinked_inodes+1)*sizeof(ino_t));

	hardlinked_inodes[num_hardlinked_inodes] = inode_num;
	num_hardlinked_inodes++;

end:
	pthread_mutex_unlock(&inodes_check_mutex);
	return found;
}

