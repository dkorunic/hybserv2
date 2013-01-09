/*
 * alloc.h
 * Hybserv2 Services by Hybserv2 team
 *
 */

#ifndef INCLUDED_alloc_h
#define INCLUDED_alloc_h

#include "stdinc.h"
#include "config.h"

#if defined HAVE_GC_GC_H
#  include <gc/gc.h>
#elif defined HAVE_GC_H
#  include <gc.h>
#endif

#if defined HAVE_BOEHM_GC
# undef GC_DEBUG
# define malloc(n) GC_MALLOC(n)
# define calloc(m,n) GC_MALLOC((m)*(n))
# define free(p) GC_FREE(p)
# define realloc(p,n) GC_REALLOC((p),(n))
# define CHECK_LEAKS() GC_gcollect()
#endif /* HAVE_BOEHM_GC */

#ifdef BLOCK_ALLOCATION
/* structure definition for a sub block in a preallocated heap */
typedef struct sBlock
{
	struct sBlock *next; /* next sub block in our heap */
	void *first;         /* pointer to first element in sub block */
	void *last;          /* pointer to last element in sub block */
	int FreeElements;    /* number of unused elements in sub block */

	/*
	 * This is the pointer to the last used element in the block,
	 * meaning every slot between 'LastUsedSlot' and 'last', will
	 * be unused. This is needed for BlockSubFree().
	 * Normally, (Heap->ElementsPerBlock - Block->FreeElements)
	 * would be the same thing, but if middle elements are deleted,
	 * Block->FreeElements would increase, while the last used
	 * element in the block would remain the same, so we need a
	 * separate pointer to keep track.
	 */
	void *LastUsedSlot;

	/*
	 * SlotHoles is an array of pointers to all of the
	 * "holes" in the sub block; when an element is deleted,
	 * its pointer is added to this array so the next time
	 * we add an element, we know where to fill in the hole
	 */
	void **SlotHoles;
	int LastSlotHole; /* number of used indices in SlotHoles[] */
}
SubBlock;

/* structure definition for a preallocated heap of memory */

typedef struct BlockHeap
{
	SubBlock *base;       /* first sub block in our heap */
	int ElementSize;      /* size of each element */
	int ElementsPerBlock; /* number of elements in each sub block */
	int NumSubBlocks;     /* number of sub blocks we have allocated */
	int FreeElements;     /* number of unused elements in all blocks */
}
Heap;

Heap *HeapCreate(size_t, int);
void *BlockSubAllocate(Heap *);
void BlockSubFree(Heap *, void *);
void InitHeaps(void);
#endif /* BLOCK_ALLOCATION */

void *MyMalloc(size_t);
void *MyRealloc(void *, size_t);
char *MyStrdup(const char *);
void OutOfMem(void);

/* MyFree - free an argument */
#define MyFree(ptr) \
{                   \
  if (ptr != NULL)  \
  {                 \
    free(ptr);      \
    ptr = NULL;     \
  }                 \
}

#ifdef BLOCK_ALLOCATION
extern Heap *ClientHeap;
extern Heap *ChannelHeap;
extern Heap *ServerHeap;
#endif /* BLOCK_ALLOCATION */

#endif /* INCLUDED_alloc_h */
