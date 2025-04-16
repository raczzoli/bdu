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
#include <string.h>

#include "dir.h"
#include "output.h"

static void print_size(FILE *fp, long int bytes, int leading_spaces);

// json
static void print_json(FILE *fp, struct dir_entry *head, struct output_options options, int depth);

// plain text
static void print_plain_text(FILE *fp, struct dir_entry *head, struct output_options options, int depth);

// html
static void print_html(FILE *fp, struct dir_entry *head, struct output_options options, int depth);
static void print_html_entries(FILE *fp, struct dir_entry *head, struct output_options options, int depth);


void output_print(FILE *fp, struct dir_entry *head, const char *format, struct output_options options)
{
	if (strcmp(format, "json") == 0)
		print_json(fp, head, options, 0);
	else if (strcmp(format, "text") == 0)
		print_plain_text(fp, head, options, 0);
	else if (strcmp(format, "html") == 0)
		print_html(fp, head, options, 0);
	else 
		fprintf(fp, "Invalid output format!\n");
}

/**
** JSON output
**/
static void print_json(FILE *fp, struct dir_entry *head, struct output_options options, int depth)
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
	
	if (depth < options.max_depth) {
		if (head->children_len > 0) {
			fprintf(fp, ",\"children\":[");
			for (int i=0;i<head->children_len;i++) {
				print_json(fp, head->children[i], options, depth+1);

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
static void print_plain_text(FILE *fp, struct dir_entry *head, struct output_options options, int depth)
{
	int i=0;
	for (i=0;i<depth;i++)
		printf("\t");

	if (!options.no_styles)
		if (options.show_critical_at_bytes > 0 || options.show_warn_at_bytes) {
			if (options.show_critical_at_bytes > 0 && head->bytes >= options.show_critical_at_bytes)
				fprintf(fp, "\033[31m"); // red
			else if (options.show_warn_at_bytes > 0 && head->bytes >= options.show_warn_at_bytes)
				fprintf(fp, "\033[33m"); // yellow
			else 
				fprintf(fp, "\033[32m"); // green
		}

	print_size(fp, head->bytes, 1);

	if (!options.no_styles)
		fprintf(fp, "\033[0m"); // reset font color

	// if last_mdate was set, we print it too
	if (head->last_mdate) {
		fprintf(fp, "   %s  ", head->last_mdate);
	}

	fprintf(fp, " %s\n", head->path);

	if (depth >= options.max_depth)
		return;

	if (head->children_len > 0)
		for (i=0;i<head->children_len;i++) {
			print_plain_text(fp, head->children[i], options, depth+1);
		}
}

static void print_html(FILE *fp, struct dir_entry *head, struct output_options options, int depth)
{
	fprintf(fp, "<!DOCTYPE html>\n<html lang=\"en\">\n");
	fprintf(fp, "<head><meta charset=\"UTF-8\"><title>Disk Usage Report</title><style>body {font-family: monospace; background: #1e1e1e; color: #dcdcdc; padding: 20px;} ul {list-style-type: none; padding-left: 20px;} li {margin: 4px 0;} .size {display: inline-block; width: 80px; font-weight: bold;} .red {color: #ff5c5c;} .orange {color: #ffa500;} .yellow {color: #ffd700;} .green {color: #7fff00;}</style></head>\n");
	fprintf(fp, "<body>");
		fprintf(fp, "<h1>Disk Usage Report for %s</h1>", head->path);
		fprintf(fp, "<ul>");
			print_html_entries(fp, head, options, depth);
		fprintf(fp, "</ul>\n");
	fprintf(fp, "</body>\n");
	fprintf(fp, "</html>\n");
}

static void print_html_entries(FILE *fp, struct dir_entry *head, struct output_options options, int depth)
{
	char size_cls[10];

	if (options.show_critical_at_bytes > 0 || options.show_warn_at_bytes) {
		if (options.show_critical_at_bytes > 0 && head->bytes >= options.show_critical_at_bytes)
			strcpy(size_cls, "red");
		else if (options.show_warn_at_bytes > 0 && head->bytes >= options.show_warn_at_bytes)
			strcpy(size_cls, "orange");
		else 
			strcpy(size_cls, "green");
	}

	fprintf(fp, "<li><span class=\"size %s\">", size_cls);//
	print_size(fp, head->bytes, 0);
	fprintf(fp, "</span> %s",head->path);
	
	if (depth < options.max_depth) {
		if (head->children_len > 0)
			for (int i=0;i<head->children_len;i++) {
				fprintf(fp, "<ul>");
				print_html_entries(fp, head->children[i], options, depth+1);
				fprintf(fp, "</ul>");
			}
	}

	fprintf(fp, "</li>");
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