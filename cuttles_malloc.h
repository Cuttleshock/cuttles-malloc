#ifndef CUTTLES_MALLOC_H
#define CUTTLES_MALLOC_H

#include <stddef.h>

// Call once before using other methods
// Return values:
//  0 indicates success
// -1 indicates your request was below the minimum capacity
// -2 indicates the system calls to allocate memory failed
int cuttles_malloc_init(size_t capacity);

// Call at end of program to be nice
void cuttles_malloc_deinit();

void *cuttles_malloc(size_t size);
void *cuttles_realloc(void *ptr, size_t size);
void cuttles_free(void *ptr);

#endif
