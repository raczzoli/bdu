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
 
#include <stdio.h>
#include <string.h>

#include "bdu.h"
#include "output.h"

static void print_size(FILE *fp, long int bytes, int leading_spaces);
static void print_json(FILE *fp, struct dir_entry *head, int max_depth, int depth);
static void print_plain_text(FILE *fp, struct dir_entry *head, int max_depth, int depth);

void output_print(FILE *fp, struct dir_entry *head, const char *format, int max_depth)
{
	if (strcmp(format, "json") == 0)
		print_json(fp, head, max_depth, 0);
	else if (strcmp(format, "text") == 0)
		print_plain_text(fp, head, max_depth, 0);
	else 
		fprintf(fp, "Invalid output format!\n");
}

/**
** JSON output
**/
static void print_json(FILE *fp, struct dir_entry *head, int max_depth, int depth)
{
	fprintf(fp, "{");
	fprintf(fp, "\"path\":\"%s\",", head->path);
	fprintf(fp, "\"size-bytes\":%ld,", head->bytes);

	// if last_mdate was set, we print it too
	if (head->last_mdate)
		fprintf(fp, "\"last-modified\":\"%s\",", head->last_mdate);

	fprintf(fp, "\"size-human\":\"");	
	print_size(fp, head->bytes, 0);
	fprintf(fp, "\"");
	
	if (depth < max_depth) {
		if (head->children_len > 0) {
			fprintf(fp, ",\"children\":[");
			for (int i=0;i<head->children_len;i++) {
				print_json(fp, head->children[i], max_depth, depth+1);

				if (i < (head->children_len-1))
					fprintf(fp, ",");
			}
			fprintf(fp, "]");
		}
	}

	fprintf(fp, "}");

	if (depth == 0)
		fprintf(fp, "\n");
}

/**
** Plain text output
**/
static void print_plain_text(FILE *fp, struct dir_entry *head, int max_depth, int depth)
{
	int i=0;
	for (i=0;i<depth;i++)
		printf("\t");

	print_size(fp, head->bytes, 1);

	// if last_mdate was set, we print it too
	if (head->last_mdate) {
		fprintf(fp, "   %s  ", head->last_mdate);
	}

	fprintf(fp, " %s\n", head->path);

	if (depth >= max_depth)
		return;

	if (head->children_len > 0)
		for (i=0;i<head->children_len;i++) {
			print_plain_text(fp, head->children[i], max_depth, depth+1);
		}
}

static void print_size(FILE *fp, long int bytes, int leading_spaces)
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

	if (leading_spaces)
		fprintf(fp, "%7.2f%s", fin_size, units[unit_cntr]);
	else 
		fprintf(fp, "%.2f%s", fin_size, units[unit_cntr]);
}