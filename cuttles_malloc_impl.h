#ifndef CUTTLES_MALLOC_IMPL
	#error "Implementation file: should not be included"
#endif

#ifndef CUTTLES_MALLOC_IMPL_H
#define CUTTLES_MALLOC_IMPL_H

// Allocated memory starts right after one of these, so ensure that DataHeader's
// alignment is good enough for anything the user may need
typedef struct DataHeader {
	struct DataHeader *next;
	struct DataHeader *prev;
	unsigned int chunks;
	int heap_index;
} DataHeader;

typedef struct {
	DataHeader *data;
} HeapNode;

// 512 bytes. Must match DataHeader's alignment.
#define CHUNK_SIZE 0x200

// Used for initialisation. Assumes CHUNK_SIZE is a power of 2.
#define ROUND_DOWN(x) \
	((x) & ~(CHUNK_SIZE - 1))

// Account for the fact that a DataHeader will be eaten out of the data space
#define ROUND_UP(x) \
	(ROUND_DOWN((x) + sizeof(DataHeader) + CHUNK_SIZE - 1))

// Minimum number of chunks required to allocate x bytes
#define NUM_CHUNKS(x) \
	((x) == 0 ? 0 : ROUND_UP(x) / CHUNK_SIZE)

// Pointer arithmetic to find where a new header should be allocated
#define NEXT_HEADER(header) \
	((DataHeader *)(((char *)(header)) + (header)->chunks * CHUNK_SIZE))

// Navigating the heap's implicit tree structure
#define LEFT_NODE(n) \
	((n) * 2 + 1)

#define RIGHT_NODE(n) \
	((n) * 2 + 2)

#define PARENT_NODE(n) \
	(((n) - 1) / 2)

// Private shared state
static void *data = 0;
static HeapNode *heap = 0;
static int heap_size = 0;

static void create_heap_node(DataHeader *header);
static void remove_heap_node(int i);
static void swap_nodes(int i, int j);
static void bubble_up_heap(int i);
static void bubble_down_heap(int i);

// Mutates both heap and linked list, so no need to return index
static void create_heap_node(DataHeader *header)
{
	heap[heap_size].data = header;
	header->heap_index = heap_size;
	++heap_size;
	bubble_up_heap(heap_size - 1);
}

// Fixes up pointers back to the heap, so no need to disassociate header after calling
static void remove_heap_node(int i)
{
	if (i == heap_size - 1) {
		// Node to be removed is already at the end; skip extra steps
		heap[i].data->heap_index = -1;
		--heap_size;
	} else {
		// Step 1: disassociate node to be removed
		heap[i].data->heap_index = -1;
		// Step 2: relocate node from end of heap to i
		heap[i].data = heap[heap_size - 1].data;
		heap[i].data->heap_index = i;
		// Step 3: remove end of heap
		--heap_size;
		// Step 4: fix up the now misplaced node at i. One or both of bubble_up and
		// bubble_down will be a no-op.
		bubble_up_heap(i);
		bubble_down_heap(i);
	}
}

// For internal use by heap methods only
static void swap_nodes(int i, int j)
{
	DataHeader *tmp = heap[i].data;
	heap[i].data = heap[j].data;
	heap[i].data->heap_index = i;
	heap[j].data = tmp;
	heap[j].data->heap_index = j;
}

// Mutates both heap and linked list, so no need to return index
static void bubble_up_heap(int i)
{
	while (i > 0 && heap[PARENT_NODE(i)].data->chunks < heap[i].data->chunks) {
		swap_nodes(i, PARENT_NODE(i));
		i = PARENT_NODE(i);
	}
}

// Mutates both heap and linked list, so no need to return index
static void bubble_down_heap(int i)
{
	int done = 0;
	while (RIGHT_NODE(i) < heap_size) {
		unsigned int num_chunks = heap[i].data->chunks;
		unsigned int left_chunks = heap[LEFT_NODE(i)].data->chunks;
		unsigned int right_chunks = heap[RIGHT_NODE(i)].data->chunks;
		if (left_chunks > num_chunks && right_chunks > num_chunks) {
			int next_node = left_chunks >= right_chunks ? LEFT_NODE(i) : RIGHT_NODE(i);
			swap_nodes(i, next_node);
			i = next_node;
		} else if (left_chunks > num_chunks) {
			swap_nodes(i, LEFT_NODE(i));
			i = LEFT_NODE(i);
		} else if (right_chunks > num_chunks) {
			swap_nodes(i, RIGHT_NODE(i));
			i = RIGHT_NODE(i);
		} else {
			done = 1;
			break;
		}
	}

	// Annoying edge condition: we may have zero or one leaves attached
	if (!done && LEFT_NODE(i) < heap_size && heap[LEFT_NODE(i)].data->chunks > heap[i].data->chunks) {
		swap_nodes(i, LEFT_NODE(i));
	}
}

#endif
