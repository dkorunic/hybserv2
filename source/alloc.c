/*
 * HybServ TS Services, Copyright (C) 1998-1999 Patrick Alken
 * This program comes with absolutely NO WARRANTY
 *
 * Should you choose to use and/or modify this source code, please
 * do so under the terms of the GNU General Public License under which
 * this program is distributed.
 *
 * $Id$
 */

#include "defs.h"

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "alloc.h"
#include "channel.h"
#include "client.h"
#include "config.h"
#include "log.h"
#include "misc.h"
#include "server.h"

#ifdef LIBDMALLOC
#include <dmalloc.h>
#endif

#ifdef BLOCK_ALLOCATION

Heap *ClientHeap;  /* pointer to client heap structure */
Heap *ChannelHeap; /* pointer to channel heap structure */
Heap *ServerHeap;  /* pointer to server heap structure */

static int IsHole(SubBlock *, void *);

#endif /* BLOCK_ALLOCATION */

/*
 * MyMalloc()
 *
 * attempt to malloc 'bytes' bytes of memory; returns a pointer to
 * allocated memory if successful, otherwise calls OutOfMem()
*/
void *MyMalloc(size_t bytes)
{
  void *ptr;

  ptr = malloc(bytes);
  if (!ptr)
    OutOfMem();

  return(ptr);
} /* MyMalloc() */

/*
 * MyRealloc()
 *
 * calls realloc() with 'oldptr' and 'newsize'; returns pointer to
 * allocated memory if successful, otherwise calls OutOfMem()
 */
void *MyRealloc(void *oldptr, size_t newsize)
{
  void *ptr;

  ptr = realloc(oldptr, newsize);
  if (!ptr)
    OutOfMem();

  return(ptr);
} /* MyRealloc() */

/*
 * MyStrdup()
 *
 * duplicates the string 'str' and returns a pointer to the new string
 */
char *MyStrdup(const char *str)
{
  char *newstr;

  newstr = strdup(str);
  if (!newstr)
    OutOfMem();

  return(newstr);
} /* MyStrdup() */

/*
 * MyFree()
 *
 * Free memory pointed to by 'ptr'
 */
inline void MyFree(void *ptr)
{
  if (!ptr)
    return;

  free(ptr);

  /* rollback change i've done long time ago - seems it causes hybserv
     to crash at various different places. unfortunately, original
     author made services require this :-( -kre */
  /* thanks, adx -kre */
  *ptr = NULL;
} /* MyFree() */

/*
 * OutOfMem()
 *
 * Called when a memory allocation attempt has failed - make a log entry
 * and a core file to work with
 */
void OutOfMem()
{
  putlog(LOG1, "Out of Memory, shutting down");
  DoShutdown(NULL, "Out of Memory");
} /* OutOfMem() */

#ifdef BLOCK_ALLOCATION

/*
 * InitHeaps()
 *
 * Initialize client/channel heaps
 */
void InitHeaps(void)
{
  ClientHeap = HeapCreate((size_t) sizeof(struct Luser), CLIENT_ALLOCATE);
  ChannelHeap = HeapCreate((size_t) sizeof(struct Channel), CHANNEL_ALLOCATE);
  ServerHeap = HeapCreate((size_t) sizeof(struct Server), SERVER_ALLOCATE);
} /* InitHeaps() */

/*
 * MakeBlock()
 *
 * Allocate a new sub block in the heap pointed to by 'heapptr'; If
 * unsuccessful, the program will terminate with an out of memory error
 */
static void MakeBlock(Heap *heapptr)
{
  SubBlock *sub;

  sub = (SubBlock *) MyMalloc(sizeof(SubBlock));
  sub->FreeElements = heapptr->ElementsPerBlock;

  /*
   * sub->SlotHoles will not actually contain data until an element is
   * deleted, because as long as it is empty we know that we can keep
   * adding elements onto the end of the last element in the sub block, so
   * something like a netjoin will not take many calculations to determine
   * where the next client/channel go in the sub block
   */
  sub->SlotHoles = (void **) MyMalloc(heapptr->ElementsPerBlock
      * sizeof(void *));
  memset((void *) sub->SlotHoles, 0, heapptr->ElementsPerBlock
      * sizeof(void *));
  sub->LastSlotHole = 0;

  /* Now actually allocate memory for the elements */
  sub->first = (void *) MyMalloc(heapptr->ElementsPerBlock
      * heapptr->ElementSize);

  sub->last = (void *) ((unsigned long) sub->first + (unsigned long)
      ((heapptr->ElementsPerBlock - 1) * heapptr->ElementSize));

  /* Insert sub into the chain of blocks */
  sub->next = heapptr->base;
  heapptr->base = sub;

  heapptr->NumSubBlocks++;
  heapptr->FreeElements += heapptr->ElementsPerBlock;
} /* MakeBlock() */

/*
 * HeapCreate()
 *
 * Allocate a block of memory equal to (elementsize * numperblock) from
 * which smaller blocks can be suballocated.  This improves efficiency by
 * reducing multiple calls to malloc() during netjoins etc.
 * Returns a pointer to new heap
 */
Heap *HeapCreate(size_t elementsize, int numperblock)
{
  Heap *hptr;

  hptr = (Heap *) MyMalloc(sizeof(Heap));

  hptr->ElementSize = elementsize;
  hptr->ElementsPerBlock = numperblock;
  hptr->NumSubBlocks = 0;
  hptr->FreeElements = 0;
  hptr->base = (SubBlock *) NULL;

  /* Now create the first sub block in our heap; hptr->base will point to
   * it */
  MakeBlock(hptr);

  return(hptr);
} /* HeapCreate() */

/*
 * BlockSubAllocate()
 *
 * Find an unused chunk of memory to use for a structure, in one of
 * heapptr's sub blocks, and return a pointer to it; If there are no free
 * elements in the current sub blocks, allocate a new block
 */
void *BlockSubAllocate(Heap *heapptr)
{
  SubBlock *subptr;
  unsigned long offset;

  if (!heapptr)
    return(NULL);

  if (heapptr->FreeElements == 0)
  {
    /* There are no free slots left anywhere, allocate a new sub block */
    MakeBlock(heapptr);

    /* New sub block is inserted in the beginning of the chain */
    if (!(subptr = heapptr->base))
      return(NULL);

    subptr->FreeElements--;
    heapptr->FreeElements--;

    /* return the first slot */
    return(subptr->first);

  }

  /* Walk through each sub block trying to find a free slot */
  for (subptr = heapptr->base; subptr; subptr = subptr->next)
  {
    if (subptr->FreeElements == 0)
    {
      /* there are no unused slots in this sub block */
      continue;
    }

    if (subptr->LastSlotHole == 0)
    {
      /*
       * There are no "holes" in the block (ie: there are no empty slots
       * between elements in the block that need to be filled), so just
       * use the slot right after the last element in the block
       */
      offset = (unsigned long) ((heapptr->ElementsPerBlock
            - subptr->FreeElements) * heapptr->ElementSize);
    #if 0
      debug("ElsPerBlock-FreeEls=[%d], offset=[%d] addy=[%p]\n",
        heapptr->ElementsPerBlock - subptr->FreeElements, offset,
        (void *) ((unsigned long) subptr->first + offset));
    #endif

      heapptr->FreeElements--;
      subptr->FreeElements--;
      subptr->LastUsedSlot = (void *) ((unsigned long) subptr->first + offset);

      /* Why duplicating code? -kre */
      /* return((void *) ((unsigned long) subptr->first + offset)); */

      return(subptr->LastUsedSlot);
    }
    else
    {
      int ii;
      void *ptr = NULL;

      /*
       * There is one or more "holes" in the block, where an element was
       * previously deleted, so fill a hole in with this element.
       * subptr->SlotHoles contains pointers to all of the holes, so use
       * the last index of subptr->SlotHoles as the new slot
       */
    #if 0
      debug("there are [%d] holes in the sub block\n",
        subptr->LastSlotHole);
    #endif

      ptr = subptr->SlotHoles[subptr->LastSlotHole - 1];
      subptr->SlotHoles[subptr->LastSlotHole - 1] = NULL;

      if (!ptr)
      {
        for (ii = 0; ii < subptr->LastSlotHole; ii++)
        {
          if (subptr->SlotHoles[ii] != NULL)
          {
            ptr = subptr->SlotHoles[ii];
            subptr->SlotHoles[ii] = NULL;
            break;
          }
        }
      }

      /* Check if the pointer we're using is the only pointer in
       * subptr->SlotHoles[], if so, decrement subptr->LastSlotHole */
      while ((subptr->LastSlotHole >= 1) &&
             (subptr->SlotHoles[subptr->LastSlotHole - 1] == NULL))
        subptr->LastSlotHole--;
    #if 0
      debug("LastSlotHole is now equal to [%d]\n", subptr->LastSlotHole);
    #endif

      subptr->FreeElements--;
      heapptr->FreeElements--;

      /* ptr should NEVER be null, and if it is, subptr->LastSlotHole was
       * incorrectly calculated */
    #if 0
      debug("returning ptr [%p]\n", ptr);
    #endif
      return(ptr);
    }
  }

  /* we should never get here */
  return(NULL);
} /* BlockSubAllocate() */

/*
 * BlockSubFree()
 *
 * Opposite of BlockSubAllocate(), instead of finding a free slot to use
 * for an element, take the given element and remove it from the sub block
 */
void BlockSubFree(Heap *heapptr, void *element)
{
  SubBlock *subptr;

  if (!heapptr || !element)
    return;

  for (subptr = heapptr->base; subptr; subptr = subptr->next)
  {
    if ((element >= subptr->first) && (element <= subptr->last))
    {
      /*
       * if: firstelement <= element <= lastelement, then element must be
       * in this sub block; add element's address to the SlotHoles array,
       * so we can fill the slot with another element later
       */

    #if 0
      debug("LastUsedEl=[%p] element=[%p]\n",
        subptr->LastUsedSlot, element);
    #endif

      /*
       * Check if the last used element in the block is the same as the
       * element we are about to delete - if so, don't bother adding
       * 'element' to subptr->SlotHoles[] because 'element' is the last
       * element in the block and so the next element will automatically
       * be added in the same position
       */
      if (subptr->LastUsedSlot == element)
      {
        int index;

        /*
         * element is the last used slot in the block so make
         * subptr->LastUsedSlot point the slot right before 'element',
         * unless element is the first slot in the block
         */
        if (subptr->LastUsedSlot == subptr->first)
        {
          /* 'element' is the only entry in this block, so the block will
           * be empty after it is deleted */
        #if 0
          debug("LASTUSED SLOT IS FIRST LastSLotHole=[%p] first [%p]\n",
            subptr->LastUsedSlot, subptr->first);
        #endif
          subptr->LastSlotHole = 0;
        }
        else
        {
          subptr->LastUsedSlot -= heapptr->ElementSize;

          /*
           * If the element right before 'element' was also
           * deleted, we must set subptr->LastUsedSlot to the
           * slot before THAT element, and keep going until
           * we find a valid element
           */
          while ((index = IsHole(subptr, subptr->LastUsedSlot)) != -1)
          {
          #if 0
            debug("LAST element [%p] is a NULL, decrementing LastUsedSlot\n",
                subptr->LastUsedSlot);
          #endif
            subptr->LastUsedSlot -= heapptr->ElementSize;
            subptr->SlotHoles[index] = NULL;
          }
        #if 0
          debug("Right after IsHole() loop, lastslothole=[%d]\n",
              subptr->LastSlotHole);
        #endif

          /*
           * Several elements of subptr->SlotHoles[] could have been
           * deleted in the above loop, so we have to recalculate
           * subptr->LastSlotHole now
           */
        #if 0
          debug("DECREMENTING LASTSLOTHOLE last=[%p] first=[%p]\n",
            subptr->LastUsedSlot, subptr->first);
        #endif

          while ((subptr->LastSlotHole >= 1) &&
                 (subptr->SlotHoles[subptr->LastSlotHole - 1] == NULL))
          {
            subptr->LastSlotHole--;
          #if 0
            debug("slot: [%p] first [%p]\n",
                subptr->SlotHoles[subptr->LastSlotHole], subptr->first);
          #endif
          }
        #if 0
          debug("LAST SLOT HOLE IS NOW: [%d]\n", subptr->LastSlotHole);
          debug("element was the last used slot, new LastUsedSlot=[%p]\n",
              subptr->LastUsedSlot);
        #endif
        }
      }
      else
      {
        /* element is in the middle of the block somewhere, add it to the
         * end of subptr->SlotHoles[] */

        subptr->SlotHoles[subptr->LastSlotHole] = (void *) element;
        /* subptr->LastSlotHole needs to be incremented since 'element'
         * was stuck at the very end */
        subptr->LastSlotHole++;
      #if 0
        debug("free'd element was put on the END of SlotHoles, index=[%d]\n",
            subptr->LastSlotHole);
        for (ii = 0; ii < subptr->LastSlotHole; ii++)
          debug("SlotHole[%d] = [%p]\n", ii, subptr->SlotHoles[ii]);
      #endif
      }

      heapptr->FreeElements++;
      subptr->FreeElements++;
    }
  }
} /* BlockSubFree() */

/*
 * IsHole()
 *
 * Determine if 'ptr' is a valid element in sub block 'sub' by checking if
 * it matches a pointer in sub->SlotHoles[] which represents deleted
 * elements Returns index in SlotHoles[] if ptr is found to be deleted,
 * otherwise -1
 */
static int IsHole(SubBlock *sub, void *ptr)
{
  int index;

  if (!sub || !ptr)
    return(-1);

  index = 0;
  while (index < sub->LastSlotHole)
  {
    if (sub->SlotHoles[index] == ptr)
      return(index);
    index++;
  }

  return(-1);
} /* IsHole() */

#endif /* BLOCK_ALLOCATION */
