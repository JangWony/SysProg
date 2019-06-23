/*
 * mm-implicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 201602042 
 * @name : 이수정
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define WSIZE 4 
#define DSIZE 8 
#define CHUNKSIZE (1<<12)//set initial heap size(4096)
#define OVERHEAD 8//header + footer

#define MAX(x, y) ((x) > (y)? (x) : (y))//max value

#define PACK(size, alloc) ((size) | (alloc))//bind values of size and alloc to word it can save datas easily

#define GET(p) (*(unsigned int *)(p))//read pointer p' word value
#define PUT(p,val) (*(unsigned int *)(p) = (val))//write val to pointer p's word 

#define GET_SIZE(p) (GET(p) & ~0x7)//size, header to block size
#define GET_ALLOC(p) (GET(p) & 0x1)//be allocated or not(1 or 0)

#define HDRP(bp) ((char *)(bp) - WSIZE)//calculate bp's header addr
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)//calculate bp's footer addr

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))//calculate next block addr
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))//calculate prev block addr


#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
/*
 * Initialize: return -1 on error, 0 on success.
 */

char* heap_listp = 0;//new heap's starting addr
char* free_block = 0;
static void* extend_heap(size_t words);//extend the heap's space, if it isn't enought
static void* find_fit(size_t asize);// searching the free block
static void place(void* bp, size_t asize);//place the memory as much as size
static void* coalesce(void* ptr);//combine free blocks
static int in_heap(const void* p);
static int aligned(const void* p);


int mm_init(void) {
	if((heap_listp = mem_sbrk(4*WSIZE)) == NULL)
	{
		return -1;
	}

	PUT(heap_listp, 0);//Initiallize to 0
	PUT(heap_listp + WSIZE, PACK(OVERHEAD, 1));//prologue header
	PUT(heap_listp + DSIZE, PACK(OVERHEAD, 1));//prologue footer
	PUT(heap_listp + WSIZE + DSIZE, PACK(0, 1));//epilogue header
	heap_listp += DSIZE;
	free_block = heap_listp;

	if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
	{
		return -1;
	}

	return 0;

}

/*
 * malloc
 */
void *malloc (size_t size) {
	size_t asize;//size for allocation
	size_t extend_size;//heap size for extention 
	char* bp;

	if(size <= 0)
	{
		return NULL;
	}

	if(size <= DSIZE) //alignment
	{
	    asize = 2 * DSIZE;
	}
	else
	{
	    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
	}

	if((bp = find_fit(asize)) != NULL)//find
	{
	    place(bp, asize);//allocate 
	    return bp;
	}
	//if cannot find 
	
	extend_size = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extend_size/WSIZE)) == NULL)
	{
	    return NULL;
	}
	place(bp, asize);
	free_block = bp;

	return bp;

}

/*
 * free
 */
void free (void *ptr) {
	if(!ptr) return;

    size_t size = GET_SIZE(HDRP(ptr));//read a block size to bp's header

	PUT(HDRP(ptr), PACK(size, 0));
	//save the block size and alloc = 0 to bp's header 
	PUT(FTRP(ptr), PACK(size, 0));
	//save the block size and alloc = 0 to bp's footer

	free_block = coalesce(ptr);//if there are free blocks, combine them
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
	size_t oldsize;
	void* newptr;

	if(size == 0) 
	{
		free(oldptr);
		return 0;
	}
		    
	if(oldptr == NULL) 
	{
		return malloc(size);
	}
			
	newptr = malloc(size);
	
	if(!newptr) 
	{
		return 0;
	}

	// Copy the old data
	//oldsize = GET_SIZE(HDRP(oldptr));
	
	oldsize = ((size_t*)(((char*)(oldsize)) - (ALIGN(sizeof(size_t)))));
	if(size < oldsize) oldsize = size;
		
	memcpy(newptr, oldptr, size);
						
	//Free the old block
	free(oldptr);

	return newptr;    
}
/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
	void *newptr;
	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

	return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {

}

static void* extend_heap(size_t words)
{
	char* bp;//pointer for extension
	size_t size;//size for extension
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	
	if((long)(bp = mem_sbrk(size)) < 0)//gain the memory to mem_sbrk()
	{
	    return NULL;
	}
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));//Initiallize the block's header

	return coalesce(bp);//combine free blocks
}
static void* coalesce(void* bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));//present block size

	if(prev_alloc && next_alloc)//prev and next blocks are alloc
	{
		return bp;
	}
	else if(prev_alloc && !next_alloc)//just prev alloc
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		//add the input size and prev block's size
		PUT(HDRP(bp), PACK(size, 0));
	    PUT(FTRP(bp), PACK(size, 0));
	}
	else if(!prev_alloc && next_alloc)//just next alloc
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	
		bp = PREV_BLKP(bp);//move the bp to prev block
	}
	else//both free
	{
		size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
	    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	    bp = PREV_BLKP(bp);
	}

	return bp;
}

static void* find_fit(size_t asize)
{
	void* bp;
	for(bp = free_block; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
		if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
		{
			return bp;	
		}
	}
	return NULL;
}

static void place(void* bp, size_t asize)//place the memory to suitable space 
{
	size_t csize = GET_SIZE(HDRP(bp));
	if((csize - asize) >= (2 * DSIZE))
	{
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
	}
	else
	{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}
