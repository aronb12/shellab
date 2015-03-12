/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this approach, a block is allocated by selecting the first free
 * block in the free explicit list that the block fits into. If no suitable
 * free block is found then the brk pointer is simply incremented by the
 * size of the new block.
 *
 * All blocks contain a header whose value is both the size of the block
 * as well the allocated bit(0 is block is free).
 * Free blocks also contain two pointers, previous free block and next
 * free block, that are used in the doubly linked explicit free list.
 * 
 * Whenever a block is freed, either by a call to free or when a selected
 * free block is larger than the allocated block, it will be simply added
 * to the front of the explicit free list unless the previous block is
 * also free(see coalescing).
 * 
 * COALESCING:
 * Previous block is free: Size of previous block is simply incremented by the size
 * of the block to be freed, removing the freed block from the explicit free list
 * is unnecessary as it has, at this point, not been added to the list in the
 * first place.
 *
 * Next block is free: Size of freed block is incremented by the size of
 * the next block. Next block is then removed from the explicit free list
 * and the new block added.
 *
 * Each block has a header and footer of the form:
 * 
 *   31							 ...3  2  1  0
 *   ----------------------------------------------
 *  | s  s  s					  ... s  s  s  a/f|
 *   ----------------------------------------------
 *
 * where a/f is a single bit denoting if the block is allocated or not
 * 
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
 * provide your team information in below _AND_ in the
 * struct that follows.
 *
 * === User information ===
 * Group: The_Rowdyruff_Boys
 * User 1: arona12
 * SSN: 2106844339
 * User 2: sveinnt12
 * SSN: 2803872909
 * === End User Information ===
 ********************************************************/
team_t team = {
	/* Group name */
	"The_Rowdyruff_Boys",
	/* First member's full name */
	"Aron Bachmann Árnason",
	/* First member's email address */
	"arona12@ru.is",
	/* Second member's full name (leave blank if none) */
	"Sveinn Þórhallson",
	/* Second member's email address (leave blank if none) */
	"sveinnt12@ru.is",
	/* Leave blank */
	"",
	/* Leave blank */
	""
};

/* $begin mallocmacros */

/* Basic constants and macros */
#define WSIZE	   4	   /* word size (bytes) */  
#define DSIZE	   8	   /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
 
#define MAX(x, y) ((x) > (y)? (x) : (y))  
 
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
 
/* Read and write a word at address p */
#define GET(p)	   (*(size_t *)(p))		
#define PUT(p, val)  (*(size_t *)(p) = (val))
 
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
 
/* Given block ptr bp, compute address of its header and FOOTER */
#define HEADER(bp)	   ((char *)(bp) - WSIZE)  
#define FOOTER(bp)	   ((char *)(bp) + GET_SIZE(HEADER(bp)) - DSIZE)
 
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given FREE block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREE(bp) ((char *)(bp) + WSIZE)
#define PREV_FREE(bp) ((char *)(bp))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* $end mallocmacros */

/* Global variables */
static char *heap_listp;  	/* pointer to first block */
char *free_listp;			/* pointer to first free block */

extern int verbose;

/* Prototypes for internal helper functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void add_free_list(void *bp);
static void remove_free_list(void *bp);
static void mm_checkheap(int verbose);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	/* Create the initial empty heap */
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
	   return -1;

	PUT(heap_listp, 0);							 	/* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));	  	/* Epilogue header */
	heap_listp += (2 * WSIZE);
	free_listp = NULL;
 
	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
	   return -1;

	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *	 Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize; 		/* Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;
	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL)
	{
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
 * mm_free - Free memory allocated for block bp.
 */
void mm_free(void *bp)
{
	size_t size = GET_SIZE(HEADER(bp));
	
	/* set allocated bit in header and footer as 0 */
	PUT(HEADER(bp), PACK(size, 0));
	PUT(FOOTER(bp), PACK(size, 0));

	add_free_list(bp);

	coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *newptr;
	size_t copySize;
	
	/* If ptr is not defined, allocate new space in memory */
	if(ptr == NULL)
		return mm_malloc(size);

	/* If size is 0, free memory pointed to by ptr */
	if(size == 0)
	{
		mm_free(ptr);
		return ptr;
	}
 	
 	/* If reallocating fails the block is left unchanged */
	if ((newptr = mm_malloc(size)) == NULL)
	{
		printf("ERROR: Fail in mm_realloc\n");
		return 0;
	}

	copySize = GET_SIZE(HEADER(ptr));

	if (size < copySize)
		copySize = size;

	memcpy(newptr, ptr, copySize);
	
	/* Free old block */
	mm_free(ptr);

	return newptr;
}

/*
 * coalesce - boundary tag coalescing.
 * 
 * Returns:   ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
	size_t prev_alloc = GET_ALLOC(FOOTER(PREV_BP(bp)));
	size_t next_alloc = GET_ALLOC(HEADER(NEXT_BP(bp)));
	size_t size = GET_SIZE(HEADER(bp));
	
	/* If no coalescing can be performed */
	if (prev_alloc && next_alloc)
		return bp;
	
	/* If only next block is free, update header and footer accordingly */
	if (!next_alloc && prev_alloc) 
	{
		/* remove next block from free list as it is to be merged
			with bp, thereby inheriting its header */
		remove_free_list(NEXT_BP(bp));

		/* update header, footer and size of altered block */
		size += GET_SIZE(HEADER(NEXT_BP(bp)));
		PUT(HEADER(bp), PACK(size, 0));
		PUT(FOOTER(bp), PACK(size,0));
	}
	
	/* If only previous block is free, update header and footer accordingly */
	else if (next_alloc && !prev_alloc) 
	{
		/* remove bp from free list as it is to be merged
			into the one in front of it */
		remove_free_list(bp);

		/* update header, footer and size of altered block */		
		size += GET_SIZE(HEADER(PREV_BP(bp)));
		PUT(FOOTER(bp), PACK(size, 0));
		PUT(HEADER(PREV_BP(bp)), PACK(size, 0));

		/* set correct return value */
		bp = PREV_BP(bp);
	}
	
	/* If prev and next blocks are free, update header and footer accordingly */
	else 
	{
		/*
		 * both current and next blocks in heap are to be merged into
		 * the one in front of bp
		 */
		remove_free_list(bp);
		remove_free_list(NEXT_BP(bp));

		/* update header, footer and size of altered block */  
		size += GET_SIZE(HEADER(PREV_BP(bp))) + GET_SIZE(FOOTER(NEXT_BP(bp)));
		PUT(HEADER(PREV_BP(bp)), PACK(size, 0));
		PUT(FOOTER(NEXT_BP(bp)), PACK(size, 0));

		/* set correct return value */
		bp = PREV_BP(bp);
	}
	return bp;
}

/* 
 * extend_heap - Extend heap with free block
 * 
 * Returns:	  The block pointer of the new free block
 */
static void *extend_heap(size_t words) 
{
	char *bp;
	size_t size;
	 
	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1) 
		return NULL;

	PUT(HEADER(bp), PACK(size, 0));		 /* free block header */
	PUT(FOOTER(bp), PACK(size, 0));		 /* free block FOOTER */

	add_free_list(bp);

	PUT(HEADER(NEXT_BP(bp)), PACK(0, 1)); /* new epilogue header */
 
	return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp 
 *	  and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
	remove_free_list(bp);

	size_t csize = GET_SIZE(HEADER(bp));

	/* determine if block needs to be split */
	if ((csize - asize) >= (2*DSIZE)) 
	{
		/* update header/footer of assigned block */
		PUT(HEADER(bp), PACK(asize, 1));
		PUT(FOOTER(bp), PACK(asize, 1));

		bp = NEXT_BP(bp);

		/* update header/footer of freed block */
		PUT(HEADER(bp), PACK(csize-asize, 0));
		PUT(FOOTER(bp), PACK(csize-asize, 0));

		add_free_list(bp);
	}
	else 
	{
		/* update header/footer of assigned block */
		PUT(HEADER(bp), PACK(csize, 1));
		PUT(FOOTER(bp), PACK(csize, 1));
	}
 }

 /* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
	/* first fit search */
	void *bp;

	for(bp = free_listp; bp != NULL; bp = GET(NEXT_FREE(bp)))
		if(GET_SIZE(HEADER(bp)) >= asize)
			return bp;

	return NULL;
}

/*
 * add_free_list - Add block to the free explicit list
 */
static void add_free_list(void *bp)
{
	if(free_listp != NULL)
	{
		/* set bp as the previous block of current first block */
		PUT(PREV_FREE(free_listp), bp);
	}

	/* Add new block to FRONT of free explicit list */
	PUT(PREV_FREE(bp), NULL);		/* New block is in front */
	PUT(NEXT_FREE(bp), free_listp); /* Next free block was first in list */
	free_listp = bp;				/* New free block is now first in list */
}

/*
 * remove_free_list - Remove block from free list
 */
static void remove_free_list(void *bp)
{
	/* if bp is the only one in the free list */
	if((GET(PREV_FREE(bp)) == NULL) && (GET(NEXT_FREE(bp)) == NULL))
		free_listp = NULL;

	/* if bp is the first in the free list */
	else if(GET(PREV_FREE(bp)) == NULL)
	{
		free_listp = GET(NEXT_FREE(bp));	// next block is now first
		PUT(PREV_FREE(free_listp), NULL);	// first block has no previous
	}

	/* if bp is the last in the free list */
	else if(GET(NEXT_FREE(bp)) == NULL)
	{
		/* last block can have no next block */
		PUT(NEXT_FREE(GET(PREV_FREE(bp))), NULL);
	}

	/* if bp is between two blocks in the free list */
	else
	{
		/* previous and next blocks must point to each other */
		PUT(NEXT_FREE(GET(PREV_FREE(bp))), GET(NEXT_FREE(bp)));
		PUT(PREV_FREE(GET(NEXT_FREE(bp))), GET(PREV_FREE(bp)));
	}
}

static void mm_checkheap(int verbose)
{
	char *bp = heap_listp;
	if(verbose)
		printf("Heap (%p):\n", heap_listp);

	if((GET_SIZE(HEADER(heap_listp)) != DSIZE) || !GET_ALLOC(HEADER(heap_listp)))
		printf("Bad prolouge header\n");

	checkblock(heap_listp);

	for(bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT_BP(bp))
	{
		if(verbose)
			printblock(bp);
		checkblock(bp);
	}

	if(verbose)
		printblock(bp);

	if((GET_SIZE(HEADER(bp)) != 0) || !(GET_ALLOC(HEADER(bp))))
		printf("Bad epilogue header\n");
}