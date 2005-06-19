/*
 * alloc.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_alloc_h
#define INCLUDED_alloc_h

#ifndef INCLUDED_sys_types_h
#include <sys/types.h>        /* size_t */
#define INCLUDED_sys_types_h
#endif

#ifndef INCLUDED_stdlib_h
#include <stdlib.h>        /* free() */
#define INCLUDED_stdlib_h
#endif

#ifndef INCLUDED_config_h
#include "config.h"        /* BLOCK_ALLOCATION */
#define INCLUDED_config_h
#endif

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
} SubBlock;

/* structure definition for a preallocated heap of memory */

typedef struct BlockHeap
{
  SubBlock *base;       /* first sub block in our heap */
  int ElementSize;      /* size of each element */
  int ElementsPerBlock; /* number of elements in each sub block */
  int NumSubBlocks;     /* number of sub blocks we have allocated */
  int FreeElements;     /* number of unused elements in all blocks */
} Heap;

/*
 * Prototypes
 */

Heap *HeapCreate(size_t elementsize, int numperblock);
void *BlockSubAllocate(Heap *heapptr);
void BlockSubFree(Heap *heapptr, void *element);
void InitHeaps();

#endif /* BLOCK_ALLOCATION */

void *MyMalloc(size_t bytes);
void *MyRealloc(void *oldptr, size_t bytes);
char *MyStrdup(const char *str);
void OutOfMem();

/* MyFree - free an argument */
#define MyFree(ptr) \
{                   \
  if (ptr != NULL)  \
  {                 \
    free(ptr);      \
    ptr = NULL;     \
  }                 \
}

/*
 * External declarations
 */

#ifdef BLOCK_ALLOCATION

extern Heap *ClientHeap;
extern Heap *ChannelHeap;
extern Heap *ServerHeap;

#endif /* BLOCK_ALLOCATION */

#endif /* INCLUDED_alloc_h */
