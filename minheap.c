#include "minheap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const size_t MINHEAP_MIN_ALLOC = 8;
/* Set maximum array size to 1 megabyte. */
static const size_t MINHEAP_MAX_ALLOC = 0x100000;

struct MinHeapHandle {
    size_t element_size;
    GreaterThanFunction * gt_func;
    void * * element_array; /* Dynamically allocated. */
    size_t allocated_elements;
    size_t allocated_blocks;
    size_t elements_per_block;
    size_t element_count;
    void * temp;
};

struct MinHeapHandle *
    minheap_construct(size_t element_size, GreaterThanFunction compare)
{
    struct MinHeapHandle * result = malloc(sizeof(struct MinHeapHandle));
    result->element_size = element_size;
    result->gt_func = compare;
    result->allocated_elements = MINHEAP_MIN_ALLOC;
    result->allocated_blocks = 1;
    result->elements_per_block = MINHEAP_MAX_ALLOC / element_size;
    result->element_count = 0;
    result->element_array = malloc(result->allocated_blocks * sizeof(void *));
    if (result->element_array == NULL) {
        free(result);
        return NULL;
    }
    *result->element_array = malloc(MINHEAP_MIN_ALLOC * element_size);
    if (*result->element_array == NULL) {
        free(result->element_array);
        free(result);
        return NULL;
    }
    result->temp = malloc(element_size);
    if (result->temp == NULL) {
        free(*result->element_array);
        free(result->element_array);
        free(result);
        return NULL;
    }

    return result;
}

void minheap_destroy(struct MinHeapHandle * heap)
{
    size_t i;

    free(heap->temp);
    heap->temp = NULL;
    for (i = 0; i < heap->allocated_blocks; ++i) {
        free(heap->element_array[i]);
        heap->element_array[i] = NULL;
    }
    free(heap->element_array);
    heap->element_array = NULL;
    free(heap);
    heap = NULL;
}

static void * _element_at(struct MinHeapHandle * heap, size_t index)
{
    size_t block_index;
    size_t offset;

    block_index = index / heap->elements_per_block;
    offset = index % heap->elements_per_block;

    return (void *)((char *)heap->element_array[block_index] +
                            offset * heap->element_size);
}

static void _swap_elements(struct MinHeapHandle * heap,
    size_t index1, size_t index2)
{
    memcpy(heap->temp, _element_at(heap, index1), heap->element_size);
    memcpy(_element_at(heap, index1), _element_at(heap, index2), heap->element_size);
    memcpy(_element_at(heap, index2), heap->temp, heap->element_size);
}

static void _bubble_down(struct MinHeapHandle * heap, size_t index)
{
    size_t length = heap->element_count;
    size_t left_child_index = 2*index + 1;
    size_t right_child_index = 2*index + 2;
    size_t min_index;

    if (left_child_index >= length) {
        return; /* index is a leaf */
    }

    min_index = index;

    if (heap->gt_func(_element_at(heap, index), _element_at(heap, left_child_index))) {
        min_index = left_child_index;
    }

    if (right_child_index < length &&
        heap->gt_func(_element_at(heap, min_index),
                      _element_at(heap, right_child_index)) )
    {
        min_index = right_child_index;
    }

    if (min_index != index) {
        /* need to swap */
        _swap_elements(heap, index, min_index);
        _bubble_down(heap, min_index);
    }
}

static void _bubble_up(struct MinHeapHandle * heap, size_t index)
{
    int parent_index;

    if (index == 0) {
        return;
    }

    /* Integer division. */
    parent_index = (index-1)/2;

    if (heap->gt_func(_element_at(heap, parent_index), _element_at(heap, index))) {
        _swap_elements(heap, index, parent_index);
        _bubble_up(heap, parent_index);
    }
}

/* Call when the heap needs to grow. */
/* For simplicity, use different strategies once the number of elements
   exceeds the block size. */
static void _realloc(struct MinHeapHandle * heap)
{
    void * new_array;
    void * * new_blocks;
    size_t old_block_count;
    size_t i;

    heap->allocated_elements *= 2;
    if (heap->allocated_elements > heap->elements_per_block) {
        old_block_count = heap->allocated_blocks;
        heap->allocated_blocks *= 2;
        heap->allocated_elements = heap->allocated_blocks * heap->elements_per_block;
        new_blocks = malloc(heap->allocated_blocks * sizeof(void *));
        if (new_blocks == NULL) {
            printf("out of memory");
            exit(1);
        }
        memcpy(new_blocks, heap->element_array, old_block_count * sizeof(void *));
        /* Special case when first transitioning to block allocation. We need to
           upgrade the first block pointer to a full block size and copy the data
           before proceeding. */
        if (old_block_count == 1) {
            new_blocks[0] = malloc(heap->elements_per_block * heap->element_size);
            if (new_blocks[0] == NULL) {
                printf("out of memory");
                exit(1);
            }
            memcpy(new_blocks[0], heap->element_array[0],
                   heap->element_count * heap->element_size);
            free(heap->element_array[0]);
        }
        for (i = old_block_count; i < heap->allocated_blocks; ++i) {
            new_blocks[i] = malloc(heap->elements_per_block * heap->element_size);
            if (new_blocks[i] == NULL) {
                printf("out of memory");
                exit(1);
            }
        }
        free(heap->element_array);
        heap->element_array = new_blocks;
    } else {
        new_array = malloc(heap->allocated_elements * heap->element_size);
        if (new_array == NULL) {
            printf("out of memory");
            exit(1);
        }
        memcpy(new_array, heap->element_array[0],
               heap->element_count * heap->element_size);
        free(heap->element_array[0]);
        heap->element_array[0] = new_array;
    }
}

bool minheap_empty(struct MinHeapHandle * heap)
{
    return heap->element_count == 0;
}

size_t minheap_size(struct MinHeapHandle * heap)
{
    return heap->element_count;
}

void minheap_insert(struct MinHeapHandle * heap, void * element)
{
    size_t length;

	if (heap->element_count >= heap->allocated_elements) {
        _realloc(heap);
    }

    length = heap->element_count;
    memcpy(_element_at(heap, length), element, heap->element_size);
    ++heap->element_count;

    _bubble_up(heap, length);
}

void * minheap_minimum(struct MinHeapHandle * heap)
{
    return heap->element_array[0];
}

void minheap_delete_minimum(struct MinHeapHandle * heap)
{
    size_t length = heap->element_count;

    if (length == 0) {
        return;
    }

    memcpy(_element_at(heap, 0), _element_at(heap, length - 1), heap->element_size);
    --heap->element_count;
    _bubble_down(heap, 0);
}
