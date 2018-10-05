#ifndef __BAH_MINHEAP_H__
#define __BAH_MINHEAP_H__

/* Since this isn't c++, this library will have to treat objects as
   opaque. It will be up to the caller to ensure that the objects inserted
   into the heap are properly initialized. */

#include <stddef.h>
#include <stdbool.h>

struct MinHeapHandle;

typedef bool GreaterThanFunction(void * left, void * right);

struct MinHeapHandle *
    minheap_construct(size_t element_size, GreaterThanFunction compare);
void minheap_destroy(struct MinHeapHandle * heap);

/* Access the top element. Does not remove it. */
bool minheap_empty(struct MinHeapHandle * heap);
size_t minheap_size(struct MinHeapHandle * heap);

void * minheap_minimum(struct MinHeapHandle * heap);
void minheap_insert(struct MinHeapHandle * heap, void * element);
void minheap_delete_minimum(struct MinHeapHandle * heap);

#endif

