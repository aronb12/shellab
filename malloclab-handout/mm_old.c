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
 * Group: The Rowdyruff Boys
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* $end mallocmacros */

/* Global variables */
static char *heap_listp;  /* pointer to first block */ 

/* Prototypes for internal helper functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);


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
    void *newptr;
    size_t copySize;
 
    if ((newptr = mm_malloc(size)) == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }
    copySize = GET_SIZE(HEADER(ptr));
    if (size < copySize){
        copySize = size;
    }
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
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
    
    /* If prev block is allocated, update header and footer accordingly */
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HEADER(NEXT_BP(bp)));
        PUT(HEADER(bp), PACK(size, 0));
        PUT(FOOTER(bp), PACK(size,0));
        return bp;
    }
    
    /* If prev next is allocated, update header and footer accordingly */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HEADER(PREV_BP(bp)));
        PUT(FOOTER(bp), PACK(size, 0));
        PUT(HEADER(PREV_BP(bp)), PACK(size, 0));
        bp = PREV_BP(bp);
        return bp;
    }
    
    /* If prev and next blocks are free, update header and footer accordingly */
    else {
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
 
    /* Initialize free block header/FOOTER and the epilogue header */
    PUT(HEADER(bp), PACK(size, 0));         /* free block header */
    PUT(FOOTER(bp), PACK(size, 0));         /* free block FOOTER */
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
    size_t csize = GET_SIZE(HEADER(bp));
    if ((csize - asize) >= (2*DSIZE)) 
    {
        PUT(HEADER(bp), PACK(asize, 1));
        PUT(FOOTER(bp), PACK(asize, 1));
        bp = NEXT_BP(bp);
        PUT(HEADER(bp), PACK(csize-asize, 0));
        PUT(FOOTER(bp), PACK(csize-asize, 0));
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
 
    for (bp = heap_listp; GET_SIZE(HEADER(bp)) > 0; bp = NEXT_BP(bp)) {
        if (!GET_ALLOC(HEADER(bp)) && (asize <= GET_SIZE(HEADER(bp)))) {
            return bp;
        }
    }
    return NULL; /* no fit */
}