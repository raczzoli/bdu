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

static void print_size(FILE *fp, long int bytes, int human_readable, int leading_spaces);

// json
static void print_json(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth);

// plain text
void print_plain_text(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth);

// html
static void print_html(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth);
static void print_html_entries(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth);


void output_print(FILE *fp, struct dir_entry **entries, int entries_len, const char *format, struct output_options options)
{
	if (strcmp(format, "json") == 0)
		print_json(fp, entries, entries_len, options, 0);
	else if (strcmp(format, "text") == 0)
		print_plain_text(fp, entries, entries_len, options, 0);
	else if (strcmp(format, "html") == 0)
		print_html(fp, entries, entries_len, options, 0);
	else 
		fprintf(fp, "Invalid output format!\n");
}

/**
** JSON output
**/
static void print_json(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth)
{
	fprintf(fp, "[");
	for (int i=0;i<entries_len;i++) {
		struct dir_entry *head = entries[i];

		fprintf(fp, "{");
		fprintf(fp, "\"path\":\"%s\",", head->path);
		fprintf(fp, "\"size-bytes\":%ld,", head->bytes);
	
		// if last_mdate was set, we print it too
		if (head->last_mdate)
			fprintf(fp, "\"last-modified\":\"%s\",", head->last_mdate);
	
		fprintf(fp, "\"size-human\":\"");	
		print_size(fp, head->bytes, 1, 0);
		fprintf(fp, "\"");
		
		if (depth < options.max_depth || options.max_depth < 0) {
			if (head->children_len > 0) {
				fprintf(fp, ",\"children\":");
				print_json(fp, head->children, head->children_len, options, depth+1);
			}
		}
	
		fprintf(fp, "}");
		if (i < (entries_len-1))
			fprintf(fp, ",");
	}
	fprintf(fp, "]");

	if (depth == 0)
		fprintf(fp, "\n");
}

/**
** Plain text output
**/
void print_plain_text(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth)
{
	if (!entries || entries_len == 0)
		return;

	for (int i=0;i<entries_len;i++) {
		struct dir_entry *head = entries[i];

		if (!head)
			continue;

		if (!options.no_leading_tabs) {
			for (int j=0;j<depth;j++)
				printf("\t");
		}

		if (!options.no_styles)
			if (options.show_critical_at_bytes > 0 || options.show_warn_at_bytes) {
				if (options.show_critical_at_bytes > 0 && head->bytes >= options.show_critical_at_bytes)
					fprintf(fp, "\033[31m"); // red
				else if (options.show_warn_at_bytes > 0 && head->bytes >= options.show_warn_at_bytes)
					fprintf(fp, "\033[33m"); // yellow
				else 
					fprintf(fp, "\033[32m"); // green
			}
	
		print_size(fp, head->bytes, options.human_readable, 1);
	
		if (!options.no_styles)
			fprintf(fp, "\033[0m"); // reset font color
	
		// if last_mdate was set, we print it too
		if (head->last_mdate) {
			fprintf(fp, "   %s  ", head->last_mdate);
		}
	
		fprintf(fp, " %s\n", head->path);
	
		if (options.max_depth < 0 || depth < options.max_depth)
		{
			print_plain_text(fp, head->children, head->children_len, options, depth+1);
		}
	}
}

static void print_html(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth)
{
	fprintf(fp, "<!DOCTYPE html>\n<html lang=\"en\">\n");
	fprintf(fp, "<head><meta charset=\"UTF-8\"><title>Disk Usage Report</title><style>body {font-family: monospace; background: #1e1e1e; color: #dcdcdc; padding: 20px;} ul {list-style-type: none; padding-left: 20px;} li {margin: 4px 0;} .size {display: inline-block; width: 80px; font-weight: bold;} .date {display: inline-block; width: 185px; } .red {color: #ff5c5c;} .orange {color: #ffa500;} .yellow {color: #ffd700;} .green {color: #7fff00;}</style></head>\n");
	fprintf(fp, "<body>");
		fprintf(fp, "<h1>Disk Usage Report</h1>");
		print_html_entries(fp, entries, entries_len, options, depth);
	fprintf(fp, "</body>\n");
	fprintf(fp, "</html>\n");
}

static void print_html_entries(FILE *fp, struct dir_entry **entries, int entries_len, struct output_options options, int depth)
{
	fprintf(fp, "<ul>");

	for (int i=0;i<entries_len;i++) {
		char size_cls[10];
		struct dir_entry *head = entries[i];

		if (options.show_critical_at_bytes > 0 || options.show_warn_at_bytes) {
			if (options.show_critical_at_bytes > 0 && head->bytes >= options.show_critical_at_bytes)
				strcpy(size_cls, "red");
			else if (options.show_warn_at_bytes > 0 && head->bytes >= options.show_warn_at_bytes)
				strcpy(size_cls, "orange");
			else 
				strcpy(size_cls, "green");
		}
	
		fprintf(fp, "<li><span class=\"size %s\">", size_cls);
		print_size(fp, head->bytes, options.human_readable, 0);
		fprintf(fp, "</span> ");

		if (head->last_mdate) {
			fprintf(fp, "<span class=\"date\">%s</span>", head->last_mdate);
		}

		fprintf(fp, "%s", head->path);

		if (depth < options.max_depth || options.max_depth < 0) {
			if (head->children_len > 0)
				print_html_entries(fp, head->children, head->children_len, options, depth+1);
		}
	
		fprintf(fp, "</li>");
	}

	fprintf(fp, "</ul>\n");
}

static void print_size(FILE *fp, long int bytes, int human_readable, int leading_spaces)
{
	const char *units[] = { "B", "K", "M", "G", "T", "P" };
	double fin_size = bytes;
	int unit_cntr = 0;
	int precision = 0;
	int show_unit = 0;

	if (human_readable) {
		precision = 2;
		show_unit = 1;

		while (1) {
			if (fin_size >= 1024) {
				fin_size /= 1024;
				unit_cntr++;
			}
			else 
				break;
		}
	}
	else {
		leading_spaces = 0;
	}

	fprintf(fp, "%*.*f%s", leading_spaces, precision, fin_size, show_unit ? units[unit_cntr] : "");

	return;
}