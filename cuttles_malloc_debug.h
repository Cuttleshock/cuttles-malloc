#ifndef CUTTLES_MALLOC_DEBUG_H
#define CUTTLES_MALLOC_DEBUG_H

#include <stdio.h>

#include "cuttles_malloc_impl.h"

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YLW "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define COLOR_RESET "\x1B[0m"

// Arbitrary number to break out if we think our doubly-linked list might be in a loop
#define LOOP_THRESHOLD 1000

// Prints each entry of the linked list, alerting to local inconsistencies
static void print_data()
{
	printf("DATA START\n");
	int i = 0;
	for (DataHeader *header = (DataHeader *)data; header != NULL; header = header->next) {
		if (++i > LOOP_THRESHOLD) {
			printf(RED "LOOP SUSPECTED: EARLY ESCAPE" COLOR_RESET "\n");
			break;
		}
		if (header->heap_index >= 0) {
			printf(BLU "F %d" COLOR_RESET, header->chunks);
			if (header->heap_index >= heap_size) {
				printf(RED " HEAP OUT OF BOUNDS" COLOR_RESET);
			} else if (heap[header->heap_index].data != header) {
				printf(YLW " HEAP UNLINKED" COLOR_RESET);
			}
		} else {
			printf(GRN "D %d" COLOR_RESET, header->chunks);
		}
		if (header->prev != NULL && header->prev->next != header) {
			printf(YLW " PREV UNLINKED" COLOR_RESET);
		}
		if (header->next != NULL && header->next->prev != header) {
			printf(MAG " NEXT UNLINKED" COLOR_RESET);
		}
		if (header->next != NULL && header->next != NEXT_HEADER(header)) {
			printf(RED " HEADER POSITION MISMATCH: actual %p / linked %p" COLOR_RESET, NEXT_HEADER(header), header->next);
		}
		printf("\n");
		if (header == header->next) {
			printf(RED "SELF-LINKED: EARLY ESCAPE" COLOR_RESET "\n");
			break;
		}
	}
	printf("DATA END\n");
}

// Prints the tree in a vaguely tree-like shape
static void print_heap()
{
	printf("HEAP START\n");
	int spaces = 1;
	int row_start = 0;
	while (LEFT_NODE(row_start) < heap_size) {
		row_start = LEFT_NODE(row_start);
	}
	for ( ; row_start > 0; row_start = PARENT_NODE(row_start)) {
		for (int i = row_start; i < LEFT_NODE(row_start) && i < heap_size; ++i) {
			printf("%4d", heap[i].data->chunks);
			for (int i = 0; i < spaces; ++i) {
				printf("    ");
			}
		}
		printf("\n");
		spaces = spaces * 2 + 1;
	}
	printf("%4d\n", heap[0].data->chunks);
	printf("HEAP END\n");
}

#endif
