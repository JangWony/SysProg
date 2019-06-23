/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Additional Macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_ALLOC_PREV_BLOCK(bp)   (GET(HDRP(bp)) & 2) 

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)- WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)- DSIZE)))

#define NEXT_PTR(bp)   ((char *)(bp) + WSIZE)
#define PREV_PTR(bp)   ((char *)(bp))

#define GET_ALLOC_PREV_BLOCK(bp)   (GET(HDRP(bp)) & 2) 

#define FREE_LIST_ARRAY_SIZE 16

/* Global var */
static char *heap_listp=0;
static char **freeListArray = NULL;

/* functions */
static void *extend_heap(size_t size);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static inline void *actualAddressFromOffset(int offset);
static inline int offsetFromActualAddress(void *bp);
static inline int indexOfFreeListArray(int size);

static void addToFreeList(void *bp);
static void deleteFromFreeList(void *bp);

/* Given a an offset, convert it to actual address */

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {

	 if((heap_listp = mem_sbrk(FREE_LIST_ARRAY_SIZE*WSIZE)) + 4*WSIZE == (void *)-1)
        return -1;

    freeListArray = (char **)heap_listp;
    memset(freeListArray, 0, (FREE_LIST_ARRAY_SIZE*DSIZE));

    heap_listp += (FREE_LIST_ARRAY_SIZE*DSIZE);

    PUT(heap_listp, 0);                                     /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));            /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));            /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0,3));                 /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;

}
/* $end mminit */


/*
 * malloc
 */
void *mm_malloc (size_t size) {
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	char *bp;      

	/* $end mmmalloc */
	if (heap_listp == 0){
		mm_init();
	}
	/* $begin mmmalloc */
	/* Ignore spurious requests */
	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)                                        
		asize = 2*DSIZE;                               
	else
		asize = DSIZE * ((size + (WSIZE) + (DSIZE-1)) / DSIZE);

	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) { 
		place(bp, asize);          
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize,CHUNKSIZE);                 
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
		return NULL;                             
	place(bp, asize);                         
	return bp;
}

/*
 * free
 */
void mm_free (void *bp) {
	/* $end mmfree */
	if(bp == 0) 
		return;

	/* $begin mmfree */
	size_t size = GET_SIZE(HDRP(bp));
	/* $end mmfree */
	if (heap_listp == 0){
		mm_init();
	}
	/* $begin mmfree */


	//Preserving the old values of allocation for the
	//block before this block in the header and 
	//the footer and setting the allocated bit
	//for current block to 0
	PUT(HDRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0));
	PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0));

	// Setting the allocated bit for this block to 0
	// in the header of the next block
	PUT(HDRP(NEXT_BLKP(bp)),GET(HDRP(NEXT_BLKP(bp)))&~2);

	coalesce(bp);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *mm_realloc(void *oldptr, size_t size) {

	size_t oldsize;
	void *newptr;

	/* If size == 0 then this is just free, and we return NULL. */
	if(size == 0) {
		mm_free(oldptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if(oldptr == NULL) {
		return mm_malloc(size);
	}

	newptr = mm_malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if(!newptr) {
		return 0;
	}

	/* Copy the old data. */
	oldsize = GET_SIZE(HDRP(oldptr));
	if(size < oldsize) oldsize = size;
	memcpy(newptr, oldptr, oldsize);

	/* Free the old block. */
	mm_free(oldptr);

	return newptr;

}


/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
	size_t prev_alloc = GET_ALLOC_PREV_BLOCK(bp);
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {            /* Case 1 */
		addToFreeList(bp);
		return bp;
	}

	else if (prev_alloc && !next_alloc) {      /* Case 2 */

		deleteFromFreeList(NEXT_BLKP(bp));	

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

		//Preserving old values of allocation stored in header
		PUT(HDRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0));
		PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0));

	}

	else if (!prev_alloc && next_alloc) {      /* Case 3 */

		deleteFromFreeList(PREV_BLKP(bp));	

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));

		//Preserving old values of allocation stored in header
		PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(PREV_BLKP(bp))|0));
		PUT(HDRP(PREV_BLKP(bp)), 
				PACK(size, GET_ALLOC_PREV_BLOCK(PREV_BLKP(bp))|0));

		bp = PREV_BLKP(bp);
	}

	else {                                     /* Case 4 */

		deleteFromFreeList(NEXT_BLKP(bp));	
		deleteFromFreeList(PREV_BLKP(bp));	

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
			GET_SIZE(FTRP(NEXT_BLKP(bp)));

		//Preserving old values of allocation stored in header
		PUT(HDRP(PREV_BLKP(bp)), 
				PACK(size, GET_ALLOC_PREV_BLOCK(PREV_BLKP(bp))|0));
		PUT(FTRP(NEXT_BLKP(bp)), 
				PACK(size, GET_ALLOC_PREV_BLOCK(PREV_BLKP(bp))|0));

		bp = PREV_BLKP(bp);
	}

	addToFreeList(bp);

	return bp;
}

/* 
 * extend_heap - Extend heap with free block and 
 * return its block pointer
 */
static void *extend_heap(size_t words) 
{
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)  
		return NULL;                                    

	/* Initialize free block header/footer and the epilogue header */

	// Free block header
	PUT(HDRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0)); 

	//Free block footer
	PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV_BLOCK(bp)|0)); 

	//New epilouge header
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	/* Coalesce if the previous block was free */
	return coalesce(bp);                             
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
	/* $end mmplace-proto */
{
	size_t csize = GET_SIZE(HDRP(bp));   
	deleteFromFreeList(bp);

	if ((csize - asize) >= (2*DSIZE)) { 

		//Preserving old values of allocation stored in header
		//and setting self allcated bit to 1
		PUT(HDRP(bp),PACK(asize, GET_ALLOC_PREV_BLOCK(bp)|1));

		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 2));
		PUT(FTRP(bp), PACK(csize-asize, 2));

		addToFreeList(bp);
	}
	else { 

		//Preserving old values of allocation stored in header
		//and setting self allcated bit to 1
		PUT(HDRP(bp),PACK(csize, GET_ALLOC_PREV_BLOCK(bp)|1));

		//Setting bit in next block's header to show that
		//the current block has been allocated
		if(NEXT_BLKP(bp))
			PUT(HDRP(NEXT_BLKP(bp)),(GET(HDRP(NEXT_BLKP(bp)))|2));
	}
}


/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
	void *bp;
	int index = indexOfFreeListArray(asize);

	// Traverse the segregated lists from the index
	// to which the size belongs till the last list,
	// until a match is found.

	while(index < FREE_LIST_ARRAY_SIZE)
	{
		for (bp = freeListArray[index]; bp && GET_SIZE(HDRP(bp)) > 0; 
				bp = actualAddressFromOffset(GET(NEXT_PTR(bp)))){
			if (asize <= GET_SIZE(HDRP(bp)) && !GET_ALLOC(HDRP(bp))){
				return bp;
			}
		}
		index ++;
	}

	return NULL; /* No fit */
}


static void deleteFromFreeList(void *bp)
{
	int index = indexOfFreeListArray(GET_SIZE(HDRP(bp)));

	//CASE 1 : Delete from the beginning of free list
	if(bp == freeListArray[index])
	{
		int offsetValue = GET(NEXT_PTR(bp));

		if(0==offsetValue)
		{
			freeListArray[index] = NULL;
			PUT(NEXT_PTR(bp),0);
			PUT(PREV_PTR(bp),0);
		}	

		else if(offsetValue)
		{
			void *bp_nextBlock = actualAddressFromOffset(offsetValue);
			freeListArray[index] = bp_nextBlock;

			PUT(PREV_PTR(bp_nextBlock),0);
			PUT(NEXT_PTR(bp),0);
		}
	}

	// CASE 2 : Delete from the end of the free list
	else if(0 == GET(NEXT_PTR(bp)))
	{
		int offsetValue = GET(PREV_PTR(bp));

		if(offsetValue)
		{
			void *bp_prevBlock = actualAddressFromOffset(offsetValue);
			PUT(NEXT_PTR(bp_prevBlock),0);
		}

		PUT(PREV_PTR(bp),0);
	}

	// CASE 3 : Delete from middle of the free list
	else
	{	
		if((GET(PREV_PTR(bp))!=0) && (GET(NEXT_PTR(bp))!=0))
		{
			void *bp_prevBlock = actualAddressFromOffset(GET(PREV_PTR(bp)));
			void *bp_nextBlock = actualAddressFromOffset(GET(NEXT_PTR(bp)));

			PUT(NEXT_PTR(bp_prevBlock), GET(NEXT_PTR(bp)));
			PUT(PREV_PTR(bp_nextBlock), GET(PREV_PTR(bp)));
			PUT(NEXT_PTR(bp),0);
			PUT(PREV_PTR(bp),0);
		}

	}
	//mm_checkheap(579);
}

static void addToFreeList(void *bp)
{
	int index = indexOfFreeListArray(GET_SIZE(HDRP(bp)));

	// If there is no block in free list
	if(!freeListArray[index])
		PUT(NEXT_PTR(bp),0);

	// Put block in the beginning of the free list
	else
	{
		PUT(NEXT_PTR(bp),offsetFromActualAddress(freeListArray[index]));
		PUT(PREV_PTR(freeListArray[index]),offsetFromActualAddress(bp));
	}

	freeListArray[index] = bp;
	PUT(PREV_PTR(bp),0);
}

static inline void *actualAddressFromOffset(int offset)
{

	if(offset==0)
		return NULL;

	return (void *)(offset + heap_listp);
}

/* Given an address, convert it to an offset */
static inline int offsetFromActualAddress(void *bp)
{

	if(!bp)
		return 0;

	return (int)((char*)bp - heap_listp);
}

/* Given a size, calculate the index of FreeList array  */
static inline int indexOfFreeListArray(int size)
{
	int index=0;

	// Max size of first segregated list is 2^4
	int maxSizeForFirstSizeClass = 1<<4;	
	
	// Max size of last segregated list is 2^19
	int maxSizeForLastSizeClass = (maxSizeForFirstSizeClass<<15);

	for(int blockSize = maxSizeForFirstSizeClass; 
			blockSize <= maxSizeForLastSizeClass; blockSize <<= 1)
	{
		if(size<=blockSize)
			return index;		
		index++;
	}

	return index-1;
}
