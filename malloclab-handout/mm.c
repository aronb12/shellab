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
 *   31                             ...3  2  1  0
 *   ----------------------------------------------
 *  | s  s  s                      ... s  s  s  a/f|
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
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
 
#define MAX(x, y) ((x) > (y)? (x) : (y))  
 
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
 
/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))        
#define PUT(p, val)  (*(size_t *)(p) = (val))
 
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
 
/* Given block ptr bp, compute address of its header and FOOTER */
#define HEADER(bp)       ((char *)(bp) - WSIZE)  
#define FOOTER(bp)       ((char *)(bp) + GET_SIZE(HEADER(bp)) - DSIZE)
 
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
char *free_listp;	/* pointer to first free block */

extern int verbose;

/* Prototypes for internal helper functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void free_block(void *bp);
static void add_free_list(void *bp);
static void remove_free_list(void *bp);
static void mm_checkheap(int verbose);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1){
       return -1;
    }

    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* Epilogue header */
    heap_listp += (2 * WSIZE);
    free_listp = NULL;

    /* the initial heap is just one free block */
    // free_listp = heap_listp;
 
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
       return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
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

    /*
     * Original, given code. 
     *
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
    */
}

/*
 * mm_free - Freeing a block does naaathing.
 *
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HEADER(bp));
    
    PUT(HEADER(bp), PACK(size, 0));
    PUT(FOOTER(bp), PACK(size, 0));

    free_block(bp);
    
    /* 
     *  TODO: 
     *  Add some rules as to when coalescing should be done:
     *  Every time a block is freed?
     *  Coalesce all possible blocks when no sufficiently large free block is found?
     */

    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	if(ptr == NULL)
	{
		return mm_malloc(size);
	}

	if(size == 0)
	{
		mm_free(ptr);
	}

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
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
    
    /* If prev and next blocks are allocated return pointer of the freed block */
    if (prev_alloc && next_alloc) {
        return bp;
    }
    
    /* If only prev block is allocated, update header and footer accordingly */
    else if (!next_alloc && prev_alloc) {
        remove_free_list(NEXT_BP(bp));

        size += GET_SIZE(HEADER(NEXT_BP(bp)));
        PUT(HEADER(bp), PACK(size, 0));
        PUT(FOOTER(bp), PACK(size,0));

        return bp;
    }
    
    /* If only next is allocated, update header and footer accordingly */
    else if (next_alloc && !prev_alloc) {
        remove_free_list(bp);

        size += GET_SIZE(HEADER(PREV_BP(bp)));
        PUT(FOOTER(bp), PACK(size, 0));
        PUT(HEADER(PREV_BP(bp)), PACK(size, 0));
        bp = PREV_BP(bp);
        return bp;
    }
    
    /* If prev and next blocks are free, update header and footer accordingly */
    else {
    	remove_free_list(bp);
    	remove_free_list(NEXT_BP(bp));

        size += GET_SIZE(HEADER(PREV_BP(bp))) + GET_SIZE(FOOTER(NEXT_BP(bp)));
        PUT(HEADER(PREV_BP(bp)), PACK(size, 0));
        PUT(FOOTER(NEXT_BP(bp)), PACK(size, 0));
        bp = PREV_BP(bp);
        return bp;
    }
}

/* 
 * extend_heap - Extend heap with free block
 * 
 * Returns:      The block pointer of the new free block
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
     
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    PUT(HEADER(bp), PACK(size, 0));         /* free block header */
    PUT(FOOTER(bp), PACK(size, 0));         /* free block FOOTER */

    add_free_list(bp);

    PUT(HEADER(NEXT_BP(bp)), PACK(0, 1)); /* new epilogue header */
 
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp 
 *      and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
	remove_free_list(bp);

    size_t csize = GET_SIZE(HEADER(bp));
    if ((csize - asize) >= (2*DSIZE)) 
    {
        PUT(HEADER(bp), PACK(asize, 1));
        PUT(FOOTER(bp), PACK(asize, 1));

        bp = NEXT_BP(bp);
        PUT(HEADER(bp), PACK(csize-asize, 0));
        PUT(FOOTER(bp), PACK(csize-asize, 0));
        free_block(bp);
    }
    else 
    {
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
    {
    	if(GET_SIZE(HEADER(bp)) >= asize)
    	{
    		return bp;
    	}
    }

    return NULL; /* no fit */

    // for (bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT_BP(bp)) {
    //     if (!GET_ALLOC(HEADER(bp)) && (asize <= GET_SIZE(HEADER(bp)))) {
    //         return bp;
    //     }
    // }
    // return NULL; /* no fit */
}

/*
 * free_block - Set block header/footer allocation bit to 0 and add block to free list
 */
static void free_block(void *bp)
{
    /* Initialize free block header/FOOTER and the epilogue header */
    PUT(HEADER(bp), PACK(GET_SIZE(HEADER(bp)), 0));         /* free block header */
    PUT(FOOTER(bp), PACK(GET_SIZE(HEADER(bp)), 0));         /* free block FOOTER */

    add_free_list(bp);
}

/*
 * add_free_list - Add block to the free explicit list
 */
static void add_free_list(void *bp)
{
    /* Add new block to FRONT of free explicit list */
    PUT(PREV_FREE(bp), NULL);				/* New block is in front */
    PUT(NEXT_FREE(bp), free_listp);		/* Next free block was first in list */
    free_listp = bp;						/* New free block is now first in list */
}

/*
 * remove_free_list - Remove block from free list
 */
static void remove_free_list(void *bp)
{
	if(GET(PREV_FREE(bp)) == NULL)
	{
		free_listp = GET(NEXT_FREE(bp));
	}
	else if(GET(NEXT_FREE(bp)) == NULL)
	{
		/* set the next pointer of previous block as null */
		PUT(NEXT_FREE(PREV_FREE(bp)), NULL);
	}
	else
	{
		PUT(NEXT_FREE(PREV_FREE(bp)), GET(NEXT_FREE(bp)));
		PUT(PREV_FREE(NEXT_FREE(bp)), GET(PREV_FREE(bp)));
	}
}

// static void mm_checkheap(int verbose)
// {
// 	char *bp = heap_listp;
// 	if(verbose)
// 		printf("Heap (%p):\n", heap_listp);

// 	if((GET_SIZE(HEADER(heap_listp)) != DSIZE) || !GET_ALLOC(HEADER(heap_listp)))
// 		printf("Bad prolouge header\n");

// 	checkblock(heap_listp);

// 	for(bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT_BP(bp))
// 	{
// 		if(verbose)
// 			printblock(bp);
// 		checkblock(bp);
// 	}

// 	if(verbose)
// 		printblock(bp);

// 	if((GET_SIZE(HEADER(bp)) != 0) || !(GET_ALLOC(HEADER(bp))))
// 		printf("Bad epilogue header\n");
// }