#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>

#define SWAP(T, a, b) { T __t = a; a = b; b = __t; }

template <typename T>
bool heap_test(List<T> * heap, bool(compare)(T, T))
{
	for (int i = 1; i < heap->len; i++) {
		int p = (i - 1) / 2;
		if (compare((*heap)[i], (*heap)[p])) {
			printf("Pos %d is larger than pos %d. "
				"Heap test failed.", i, p);
		}
		return false;
	}
	return true;
}

// TODO(pixlark): We can optimize this by searching on a
// branch-by-branch basis, and stopping search on a branch if we reach
// vertices with values that pass the value we are searching for based
// on the passed comparison function.
#if 0
// For now, just use List.find()
template <typename T>
int heap_find(List<T> * heap, T item, bool(equality)(T, T), bool(compare)(T, T))
{
	for (int i = 0; i < heap->len; i++) {
		if (equality((*heap)[i], item)) {
			return i;
		}
	}
	return -1;
}
#endif

template <typename T>
void heap_insert(List<T> * heap, T item, bool(compare)(T, T))
{
	int pos = heap->len;
	heap->push(item);
	while (pos != 0) {
		int parent_pos = (pos - 1) / 2;
		if (!compare((*heap)[parent_pos], (*heap)[pos])) {
			SWAP(T, (*heap)[parent_pos], (*heap)[pos]);
			pos = parent_pos;
		} else {
			break;
		}
	}
}

template <typename T>
T heap_pop(List<T> * heap, bool(compare)(T, T))
{
	T popped = (*heap)[0];
	(*heap)[0] = (*heap)[heap->len - 1];
	heap->pop();
	int pos = 0;
	while (1) {
		int left  = 2 * pos + 1;
		int right = 2 * pos + 2;
		int largest = pos;
		if (left < heap->len &&
			compare((*heap)[left], (*heap)[largest])) {
			largest = left;
		}
		if (right < heap->len &&
			compare((*heap)[right], (*heap)[largest])) {
			largest = right;
		}
		if (largest != pos) {
			SWAP(T, (*heap)[pos], (*heap)[largest]);
			pos = largest;
		} else {
			break;
		}
	}
	return popped;
}

template <typename T>
List<T> heap_from_array(T * arr, int len, bool(compare)(T, T))
{
	List<T> heap;
	heap.alloc();
	for (int i = 0; i < len; i++) {
		heap_insert(&heap, arr[i], compare);
	}
	return heap;
}

template <typename T>
List<T> heap_as_sorted(List<T> * heap, bool(compare)(T, T))
{
	List<T> sorted;
	sorted.alloc();
	List<T> heap_copy = heap->copy();
	while (heap_copy.len > 0) {
		T popped = heap_pop(&heap_copy, compare);
		sorted.push(popped);
	}
	heap_copy.dealloc();
	return sorted;
}

#endif
