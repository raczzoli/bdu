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
#include <ctype.h>

#include "utils.h"

long int human_size_to_bytes(const char *input)
{
	char unit;
	const char units[] = { 'B', 'K', 'M', 'G', 'T', 'P' };
	int units_len = sizeof(units);

	long int bytes = 0;
	long int human_bytes;
	char humar_unit = 0;

	sscanf(input, "%ld%c", &human_bytes, &humar_unit);
	bytes = human_bytes;

	if (humar_unit == 0)
		humar_unit = 'B';

	humar_unit = toupper(humar_unit);

	for (int i=0;i<units_len;i++) {
		unit = units[i];

		if (unit == humar_unit) 
			break;

		bytes *= 1024;
	}

	return bytes;
}