# cuttles-malloc

A custom memory allocator that avoids any system calls except at startup and shutdown. Probably not the best out there.

## Usage

```c
#include "cuttles_malloc.h"

cuttles_malloc_init(1 << 20); // 1 MB

// Use cuttles_malloc(), cuttles_realloc() and cuttles_free() as drop-ins
// for the standard library functions

cuttles_malloc_deinit();
```

## Possible modifications

To avoid heap usage entirely, change `data` to `static char[1 << 20]` (or whatever) and `heap` to `static HeapNode[sizeof(data) / CHUNK_SIZE]`; then skip `cuttles_malloc_init()` and `_deinit()`.

Try tweaking `CHUNK_SIZE` in `cuttles_malloc_impl.h`. Ensure that it's a multiple of 8 so that `DataHeader` and all possible allocations remain properly aligned.

To look at how fragmented the virtual heap is getting, `#include "cuttles_malloc_debug.h"` inside the `CUTTLES_MALLOC_IMPL` guard at the top of `cuttles_malloc.c` then call `print_data()` liberally.

For a single-file library, place the contents of `cuttles_malloc_impl.h` and `cuttles_malloc.c` inside the header guards of `cuttles_malloc.h`, removing includes and other guards as appropriate. To avoid polluting your `#define`s, namespace all `#define`s present and `#undef` them at the end of the file.

For a two-file library without the `#define` problem, stick `cuttles_malloc_impl.h` onto the top of `cuttles_malloc.c`. The only reason it's a separate file is to share code with `cuttles_malloc_debug.h`.

## Implementation details

Internally, `malloc()`ed data is stored in a virtual heap - a doubly linked list that stores each slot's size and an index to a max-heap. The size and 'next' links are strictly redundant, but make the implementation more intuitive, and calculating one from the other might introduce more overhead than it saves.

This virtual heap is made of discrete chunks of `CHUNK_SIZE`. It's impossible to allocate less than that, and the actual amount of memory given by `malloc()` is always the smallest multiple of `CHUNK_SIZE` that fits the request.

The max-heap stores pointers to all free slots. This allows a sufficiently large slot for allocations to always be found in `O(log n)`, or to determine in `O(1)` that we aren't able to allocate space.

`realloc()` prioritises immediate speed (and simplicity). It _could_ take the opportunity to defragment the virtual heap by moving pointers around; instead, it leaves data in place unless impossible to do so, and when it moves data around, it uses the first free slot it finds via the heap.
