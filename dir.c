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

#include "dir.h"

struct dir_entry *dir_create_dentry(char *path)
{
	struct dir_entry *entry = (struct dir_entry *)malloc(sizeof(struct dir_entry));

	if (!entry) {
		printf("Error allocating memory for dentry!\n");
		return NULL;
	}

	entry->path = strdup(path);
	entry->path_len = strlen(entry->path);
	entry->last_mdate = NULL;
	entry->bytes = 0;
	entry->children = NULL;
	entry->parent = NULL;
	entry->children_len = 0;
	
	return entry;
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