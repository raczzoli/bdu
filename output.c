#include <stdio.h>
#include <string.h>

#include "bdu.h"
#include "output.h"

static void print_size(long int bytes);
static void print_json(FILE *fp, struct dir_entry *head, int max_depth, int depth);
static void print_plain_text(FILE *fp, struct dir_entry *head, int max_depth, int depth);

void output_print(FILE *fp, struct dir_entry *head, const char *format, int max_depth)
{
	if (strcmp(format, "json") == 0)
		print_json(fp, head, max_depth, 0);
	else if (strcmp(format, "plain") == 0)
		print_plain_text(fp, head, max_depth, 0);
	else 
		fprintf(fp, "Invalid output format!\n");
}

static void print_json(FILE *fp, struct dir_entry *head, int max_depth, int depth)
{
	fprintf(fp, "printing json...\n");
}

static void print_plain_text(FILE *fp, struct dir_entry *head, int max_depth, int depth)
{
	int i=0;
	for (i=0;i<depth;i++)
		printf("\t");

	print_size(head->bytes);
	fprintf(fp, "%s\n", head->path);

	if (depth >= max_depth)
		return;

	if (head->children_len > 0)
		for (i=0;i<head->children_len;i++) {
			print_plain_text(fp, head->children[i], max_depth, depth+1);
		}
}

static void print_size(long int bytes)
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

	printf("%7.2f%s ", fin_size, units[unit_cntr]);
}