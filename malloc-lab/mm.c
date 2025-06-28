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

/* double size */
#define DSIZE 8
/* 1024bytes만큼 힙 확장 */
#define CHUNKSIZE (1<<12)

/* 매크로 함수들 */
#define MAX(x, y)           ((x) > (y)? (x) : (y))

/* 사이즈와 할당 비트를 word에 Pack */
#define PACK(size, alloc)   ((size) | (alloc))

/* word를 p 주소에 읽기와 쓰기를 실행 */
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))

/* 주소 p의 block size와 할당 비트를 읽음 */
#define GET_SIZE(p)         (GET(p) & ~0b1111)
#define GET_ALLOC(p)        (GET(p) & ~0b1)

/* block 포인터가 주어졌을 때, 그것의 헤더와 푸터를 주소를 계산 */
#define HDRP(bp)            ((char *)(bp) - DSIZE)
#define FTRP(bp)            ((char *)(bp) - GET_SIZE(HDRP(bp)) - (ALIGNMENT))

/* block 포인터가 주어졌을 때, 그것의 다음 블록과 이전 블록 주소를 계산 */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - DSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - (ALIGNMENT))))


// heap_listp
static unsigned *heap_listp;

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
/* 정렬 기준 64bit 8byte * 2 */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0b1111)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp == mem_sbrk(4 * DSIZE)) == (void *)-1) 
        return -1;

    PUT(heap_listp, 0);                                     /* Alignment padding */
    PUT(heap_listp + (1 * DSIZE), PACK(ALIGNMENT, 1));      /* Prologue header */
    PUT(heap_listp + (2 * DSIZE), PACK(ALIGNMENT, 1));      /* Prologue footer */
    PUT(heap_listp + (3 * DSIZE), PACK(0, 1));              /* Epilogue header */
    heap_listp += (2*DSIZE);     /* heap의 시작지점 설정 Prologue의 footer위치로 */

    /* 빈 힙을 CHUNKSIZE bytes의 free block들로 확장 */
    if (extend_heap(CHUNKSIZE / DSIZE) == NULL)
        return -1;
    return 0;
}

/* Extend new free block */
static void *extend_heap(size_t words)
{
    char *bp; /* block pointer */
    size_t size;

    /* double word size인 ALIGNMENT 맞게 정렬 유지 */
    size = (words % 2) ? (words+1) * DSIZE : words * DSIZE;
    /* 
        mem_sbrk 함수는 size가 0보다 작거나 
        (기존 포인터 + size)가 max 메모리 주소 보다 큰 경우 -1 
    */
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    /* free blcok header와 footer와 epilogue 헤더 설정 */
    PUT(HDRP(bp), PACK(size, 0));                   /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));                   /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size, 0));        /* new epilouge header */
    
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    /* 이전 블록 footer의 할당 비트를 받는다 */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    /* 다음 블록 header의 할당 비트를 받는다 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    /* head pointer의 사이즈를 받음 */
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1: 둘 다 사용 중인 상태 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2: 이전 주소가 할당되어 있고, 다음 주소가 free인 상태 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));      /* 다음 블록 header의 blcok size를 size에 더 해준다. */
        PUT(HDRP(bp), PACK(size, 0));               /* 현재 주소의 header에 block size 쓰기 */
        PUT(FTRP(bp), PACK(size, 0));               /* 현재 주소의 footer에 block size 쓰기 */
    }

    else if (!prev_alloc && next_alloc) { /* Case 3: 이전 주소가 free 다음 주소가 할당되어 있는 상태*/
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));      /* 이전 블록 header의 block size를 size에 더 해준다. */
        PUT(FTRP(bp), PACK(size, 0));               /* 현재 주소의 footer에 block size 쓰기 */
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    /* 이전 블록 header의 block size 쓰기 */
        bp = PREV_BLKP(bp);                         /* 병합된 새 블록의 시작은 이전 블록 */
    }
    
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                         /* 병합된 새 블록의 시작은 이전 블록 */
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* 수정된 block size */
    size_t extendSize;  /* if no fit, 힙 확장 */
    char *bp;

    if (size == 0) 
        return NULL;

    /* 블록 크기 조정 */
    if (size <= ALIGNMENT)
        asize = 2*ALIGNMENT;
    else
        asize = ALIGNMENT * ((size + (ALIGNMENT) + (ALIGNMENT-1)) / ALIGNMENT);
    
    /* 할당할 수 있는 free list search */
    if ((bp == find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 할당할 수 없는 경우, extend 메모리 and 블록 할당 */
    extendSize = MAX(asize, CHUNKSIZE);
    /* extendSize / DSIZE가 0보다 작거나 메모리 최대 주소보다 클 경우 NULL */
    if ((bp == extend_heap(extendSize / DSIZE) == NULL))
        return NULL;
    
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));  /* ptr 주소의 header에 있는 block size를 가져옴 */

    PUT(HDRP(ptr), PACK(size, 0));      /* write ptr header의 ALLOC 1 -> 0 */
    PUT(FTRP(ptr), PACK(size, 0));      /* write ptr footer의 ALLOC 1 -> 0 */
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
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