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

#ifndef DIR_H
#define DIR_H

struct dir_entry {
	char *path;
	int path_len;
	size_t bytes;
	char *last_mdate;
	struct dir_entry *parent;
	struct dir_entry **children;
	int children_len;
	pthread_mutex_t lock;
};

struct dir_entry *dir_create_dentry(char *path);
struct dir_entry *dir_scan(struct dir_entry *dentry, void (dentry_scan_fn)(struct dir_entry*), int proc_mtime);
void dir_sum_dentry_bytes(struct dir_entry *dentry, long int bytes);
char *dir_get_dentry_mdate(time_t mtime);

#endif //DIR_H