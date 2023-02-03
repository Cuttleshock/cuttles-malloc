#include <string.h>
#include <stdlib.h>

#include "cuttles_malloc.h"
#define CUTTLES_MALLOC_IMPL
#include "cuttles_malloc_impl.h"
#undef CUTTLES_MALLOC_IMPL

int cuttles_malloc_init(size_t capacity)
{
	int actual_capacity = ROUND_DOWN(capacity);
	int actual_chunks = actual_capacity / CHUNK_SIZE; // NUM_CHUNKS would be 1 greater

	if (actual_capacity <= 0) {
		return -1;
	}

	// TODO: replace with mmap() or something
	// TODO: check if already initialised (should we leave untouched and return,
	// or free it and init a new block?)
	data = malloc(actual_capacity);
	if (!data) {
		return -2;
	}
	// TODO: the maximum possible number of free slots should actually be half this
	heap = malloc(actual_chunks * sizeof(*heap));
	if (!heap) {
		free(data);
		return -2;
	}

	DataHeader *data_start = (DataHeader *)data;
	data_start->heap_index = 0;
	data_start->next = NULL;
	data_start->prev = NULL;
	data_start->chunks = actual_chunks;

	heap[0].data = data_start;

	heap_size = 1;

	return 0;
}

void cuttles_malloc_deinit()
{
	// TODO: should this be safe if called multiple times/when uninitialised?
	free(data);
	free(heap);
}

void *cuttles_malloc(size_t size)
{
	unsigned int num_chunks = NUM_CHUNKS(size);

	if (heap_size == 0 || heap[0].data->chunks < num_chunks) {
		// Biggest free slot is not big enough, or no free space: abort
		return NULL;
	}

	// Walk the heap until we find a local smallest slot that can accommodate us
	int i = 0;
	int min_found = 0;
	while (RIGHT_NODE(i) < heap_size) {
		unsigned int left_chunks = heap[LEFT_NODE(i)].data->chunks;
		unsigned int right_chunks = heap[RIGHT_NODE(i)].data->chunks;
		if (left_chunks >= num_chunks && right_chunks >= num_chunks) {
			// Walk down the largest free slots
			i = left_chunks >= right_chunks ? LEFT_NODE(i) : RIGHT_NODE(i);
		} else if (left_chunks >= num_chunks) {
			i = LEFT_NODE(i);
		} else if (right_chunks >= num_chunks) {
			i = RIGHT_NODE(i);
		} else {
			// Both children have insufficient space
			min_found = 1;
			break;
		}
	}

	// Annoying edge condition: we may have zero or one leaves attached
	if (!min_found && LEFT_NODE(i) < heap_size && heap[LEFT_NODE(i)].data->chunks >= num_chunks) {
		i = LEFT_NODE(i);
	}

	DataHeader *header = heap[i].data;
	if (header->chunks == num_chunks) {
		// Perfect fit into a free slot
		remove_heap_node(header->heap_index);
		// DANGER: POINTER ARITHMETIC
		return header + 1;
	} else {
		// Cut off the high end of the free slot for data
		header->chunks -= num_chunks;
		bubble_down_heap(header->heap_index);

		DataHeader *new_header = NEXT_HEADER(header);
		new_header->chunks = num_chunks;
		new_header->prev = header;
		new_header->next = header->next;
		new_header->heap_index = -1;

		if (header->next != NULL) {
			header->next->prev = new_header;
		}
		header->next = new_header;

		// DANGER: POINTER ARITHMETIC
		return new_header + 1;
	}
}

void *cuttles_realloc(void *ptr, size_t size)
{
	int num_chunks = NUM_CHUNKS(size);
	if (ptr == NULL) {
		return cuttles_malloc(size);
	} else if (num_chunks == 0) {
		cuttles_free(ptr);
		return NULL;
	}

	// DANGER: POINTER ARITHMETIC
	DataHeader *header = ((DataHeader *)ptr) - 1;

	if (header->chunks == num_chunks) {
		// New size is still a snug fit - do nothing
		return ptr;
	} else if (header->chunks > num_chunks) {
		// Shrink the space, creating free room at the top
		unsigned int old_chunks = header->chunks;
		header->chunks = num_chunks;

		DataHeader *new_header = NEXT_HEADER(header);
		new_header->prev = header;
		new_header->chunks = old_chunks - header->chunks;

		DataHeader *next_header_up = header->next;
		header->next = new_header;

		if (next_header_up != NULL && next_header_up->heap_index >= 0) {
			// Existing free slot just above us - combine them
			new_header->next = next_header_up->next;
			new_header->chunks += next_header_up->chunks;
			new_header->heap_index = next_header_up->heap_index;

			heap[new_header->heap_index].data = new_header;
			bubble_up_heap(new_header->heap_index);

			if (new_header->next != NULL) {
				new_header->next->prev = new_header;
			}
		} else {
			// Create new free slot
			new_header->next = next_header_up;
			create_heap_node(new_header);

			if (next_header_up != NULL) {
				next_header_up->prev = new_header;
			}
		}

		return ptr;
	} else {
		// Not enough space in existing slot: check the one above us
		if (header->next != NULL && header->next->heap_index >= 0 && header->chunks + header->next->chunks >= num_chunks) {
			// There's a sufficiently large free slot above us: grow into it
			if (header->chunks + header->next->chunks == num_chunks) {
				// Our slot + next slot is a snug fit
				remove_heap_node(header->next->heap_index);

				header->chunks = num_chunks;
				header->next = header->next->next;

				if (header->next != NULL) {
					header->next->prev = header;
				}
				return ptr;
			} else {
				// Cut off the bottom of the free slot to make space
				unsigned int old_chunks = header->chunks;
				header->chunks = num_chunks;

				DataHeader *old_next_header = header->next;
				DataHeader *new_header = NEXT_HEADER(header);
				header->next = new_header;

				new_header->chunks = old_next_header->chunks + old_chunks - num_chunks;
				new_header->heap_index = old_next_header->heap_index;
				new_header->prev = header;
				new_header->next = old_next_header->next;

				heap[new_header->heap_index].data = new_header;
				bubble_down_heap(new_header->heap_index);

				if (new_header->next != NULL) {
					new_header->next->prev = new_header;
				}

				return ptr;
			}
		} else {
			// Must relocate memory
			void *new_ptr = cuttles_malloc(size);
			if (new_ptr != NULL) {
				// TODO: this is the only use of <string.h> in the file. It would be nice
				// to use a custom routine and remove the dependency; we can optimise for
				// the fact that we only ever need to copy a multiple of CHUNK_SIZE (with
				// special treatment for the destination header).
				memcpy(new_ptr, ptr, header->chunks * CHUNK_SIZE - sizeof(DataHeader));
				cuttles_free(ptr);
			}
			// If new_ptr == NULL, the original data will be unmodified
			return new_ptr;
		}
	}
}

void cuttles_free(void *ptr)
{
	// DANGER: POINTER ARITHMETIC
	DataHeader *header = ((DataHeader *)ptr) - 1;
	if (header->heap_index >= 0) {
		// Slot is already free - do nothing
		return;
	}

	int prev_free = (header->prev != NULL && header->prev->heap_index >= 0);
	int next_free = (header->next != NULL && header->next->heap_index >= 0);
	if (prev_free && next_free) {
		// Previous and next slots both free: combine all
		// TODO: does it matter what order we perform the two heap updates in?
		header->prev->chunks += header->chunks + header->next->chunks;
		header->prev->next = header->next->next;
		bubble_up_heap(header->prev->heap_index);

		remove_heap_node(header->next->heap_index);

		if (header->next->next != NULL) {
			header->next->next->prev = header->prev;
		}
		return;
	} else if (prev_free) {
		// Previous slot free: combine
		header->prev->chunks += header->chunks;
		header->prev->next = header->next;
		bubble_up_heap(header->prev->heap_index);

		if (header->next != NULL) {
			header->next->prev = header->prev;
		}
		return;
	} else if (next_free) {
		// Next slot free: combine
		header->chunks += header->next->chunks;
		header->heap_index = header->next->heap_index;
		header->next = header->next->next;
		heap[header->heap_index].data = header;
		bubble_up_heap(header->heap_index);

		if (header->next != NULL) {
			header->next->prev = header;
		}
		return;
	} else {
		// Make a free hole between allocated slots
		create_heap_node(header);
		return;
	}
}
