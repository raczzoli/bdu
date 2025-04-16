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

#ifndef OUTPUT_H
#define OUTPUT_H

struct output_options {
	int max_depth;
	long unsigned int show_warn_at_bytes;
	long unsigned int show_critical_at_bytes;
	unsigned int no_styles;
};

void output_print(FILE *fp, struct dir_entry **entries, int entries_len, const char *format, struct output_options options);

#endif //OUTPUT_H