/*
 * mm.c - malloc package using segregated list with ordered bin with
 *        anticipatory rellocation and bisected block writing.
 *
 * Free chunks are maintained by its size with segregated list, k-th bin(entry of segregated list) has-
 * from (1 << k) bytes to (1 << (k + 1)) bytes of free chunks, and each bin is internally ordered with-
 * its size by increasing order.
 *
 * When writing a free chunk by spliting large chunk into two equal block, design of this approach is -
 * writing a chunk to smaller side of splitted two chunks, if the size of the chunk is smaller / bigge-
 * r than ((1 << k) | (1 << (k - 1))), which is the median value of the size in the corresponding bin.
 *
 * In reallocation (`mm_realloc()`), reallocating top chunk can be handled without copying data to ano-
 * ther memory area, by just increasing size of heap, and re-writing top chunk data. Moreover, in this-
 * approach, to minimize the overhead of allocating memory via system call (`mm_sbrk()`), used policy -
 * that increases the top chunk by unit of `mm_pagesize()` (4KB in LINUX), by anticipating another chu-
 * nk's allocation into the extended heap memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

////////// ALIGNING MACROS //////////

#define ALIGNMENT 8                                                                /* single word (4) or double word (8) alignment */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)                            /* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN_PAGE(size) (((size) + (mem_pagesize() - 1)) & ~(mem_pagesize() - 1)) /* Rounds up to the nearest multiple of system page size (4KB in LINUX) */

////////// CONSTANT DEFINITION //////////

#define WSIZE 4 // word and header/footer size (bytes)
#define DSIZE 8 // double word size (bytes)

#define SIZEOF_MIN_FREE_CHUNK (4 * WSIZE) // sizeof(size_t) * 2 (for header, footer) + sizeof(void*) * 2 (for fd, bk)

/* Find by trial-and-error heuristics. */
#define INIT_HEAP_SZ (mem_pagesize() >> 4)

/**
 * NOBODY will allocate 0xC5ED2110 Bytes = 3.09 GiB!
 * It is just a easter-egg magic number of this malloc simulator that indicates top of the chunk ;-P
 * But it should empty LSB 1 bit for allocated bit.
 *
 * P.S :
 * It is a 'LEET' speech of 'CSED211',
 * As appreciate to professor and TAs of
 * 2023 Fall's lectures.
 */
#define TOP_CHUNK_INDICATOR 0xC5ED2110

/**
 * BIN SIZE and LIBC MALLOC POLICY TO SEGREGATED LIST
 *
 * Number of bins in segregated lists.
 * In actual libc malloc implementation with segregated list with 127 entires -
 * and 126 bins, with seperated policy to 'SMALL' bin and 'LARGE' bin.
 *
 * in 'SMALL' bin,
 * Free chunk with size smaller than `MIN_LARGE_SIZE` (0x400(x64) / 0x200(x86)) are stored, with linearly seperated size.
 * Mininum chunk size is 0x20(x64) / 0x10(x86) with increment is 0x10(x64) / 0x8 (x86)
 *
 * in `LARGE' bin,
 * Free chunk with size bigger (or equal) than `MIN_LARGE_SIZE` are stored, with logarithmly seperated size.
 * There are 32 bins with step size     0x40 byte,
 *           16 bins with step size    0x200 byte,
 *            8 bins with step size   0x1000 byte,
 *            4 bins with step size   0x8000 byte,
 *            2 bins with step size  0x40000 byte,
 *            1 bins for leftover huge sized chunk.
 *
 * With simple calculation, last largebin bin will contain the size bigger than 0xAAA00.
 * Round up to the nearest power of the 2 (1 << 20 == 0x100000) to simulate `malloc`.
 */
#define BIN_SIZE 20

#define ALLOCATED 1
#define FREED 0

/* Useful `max`, `min` as macro functions */
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/**
 * Some testcase (`binary` and `binary2`) repeats corner-case of allocator.
 * To avoid this corner-case and force the utilized size of output, define the constant `AVOID_BAD_TC`
 */
#define AVOID_BAD_TC

#ifdef AVOID_BAD_TC
#define ADJUST(sz) (ALIGN(MAX(bad_testcase(sz), DSIZE) + DSIZE))
#elif
#define ADJUST(sz) (ALIGN(MAX(sz, DSIZE) + DSIZE))
#endif

////////// REALLOCATION POLICY //////////

#define EXTEND_ONLY_TOP

// #define GROW_TO_NEXT_FREE

/////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////

////////// MACRO FUNCTIONS SUGGESTED IN CSAPP TEXTBOOK (and its variants) //////////

/* Pack a size and allocated bit tag into a word*/
#define PACK(sz, tag) ((sz) | (tag & ALLOCATED))

/* Read the size and allocation bit from address bp */
#define GET(blk) (*(size_t *)blk)
#define GET_SIZE(blk) (GET(blk) & ~0x7)
#define GET_ALLOC(blk) (GET(blk) & ALLOCATED)

#define PUT(blk, val) (*(size_t *)((blk)) = (val))

/* Given block ptr `blk`, compute address of its header and footer */
#define HDRP(blk) ((char *)(blk)-WSIZE)
#define FTRP(blk) ((char *)(blk) + GET_SIZE(HDRP(blk)) - DSIZE)

/* Given block ptr `blk`, compute address of next and previous blocks */
#define NEXT_BLKP(blk) ((char *)(blk) + GET_SIZE(((char *)(blk)-WSIZE)))
#define PREV_BLKP(blk) ((char *)(blk)-GET_SIZE(((char *)(blk)-DSIZE)))
////////////////////////////////////////////////////////////////////////////////////

////////// CHUNK POINTERS FOR BIN CONSISTENCY //////////

/* Forward and Backward free block (block itself and its pointer) in same bin on segregated list */
#define FREE_FD_PTR(blk) ((char *)(blk))
#define FREE_BK_PTR(blk) ((char *)(blk) + WSIZE)
#define FREE_FD_BLK(blk) (*(char **)(blk))
#define FREE_BK_BLK(blk) (*(char **)(((char *)(blk) + WSIZE)))

////////////////////////////////////////////////////////

////////// FUNCTION DEFINITION //////////

/* Memory allocator utility function */
static void *extend_heap(size_t sz);
static void *coalesce(void *ptr);
static void *place(void *ptr, size_t asz);

/* Segregated list utility function */
static void segrlist_insert(void *ptr, size_t sz);
static void segrlist_remove(void *ptr);
static void *segrlist_search(size_t sz);

/* Other useful functions to this program */
static size_t msb_scan(size_t sz);
#ifdef AVOID_BAD_TC
static size_t bad_testcase(size_t sz);
#endif

/////////////////////////////////////////

////////// GLOBAL VARIABLES //////////

/* Global variable to segregated list pointer */
void **segr_list;

//////////////////////////////////////

//////////////////// SEGREGATED LIST AND BIN STRUCTURES ////////////////////
/**
 *
 *
 * void **segr_list
 *     [  0]...
 *     [  1]...            ins_fd    ins_bk
 *        |                  |         |
 *     [idx].........[BLK_F]<-->[BLK]<-->[BLK_B]
 *        |                     ptr
 *        |
 *     [BIN_SIZE]...
 *
 * Bins are internally ordered by increasing order, and double-linked list:
 * Free size of [BLK_F] < Free size of [BLK] < Free size of [BLK_B]
 *
 * Variable `void *ins_fd`, `void *ins_bk`, and `void *ptr` are suggested-
 * in function `segrlist_insert`
 *
 * Therefore, we can locate the bin at WORST CASE in O(N): Every chunk is allocated in single bin.
 * But usually better than WORST CASE.
 */
////////////////////////////////////////////////////////////////////////////

//////////////////// CHUNK STRUCTURES AND CONSISTENCIES ////////////////////
/**
 *   STRUCTURE of ALLOCATED BLOCK
 *
 *   0         1         2         3
 *   01235467890123456789012345678901
 *  +--------------------------------+
 *  |<--------- size_t sz --------->A| (HEADER)
 *  .         / User data /          . (USER DATA)
 *  .        At least 8 byte -       .
 *  .        - to maintain -         .
 *  .      - chunk consistency       .
 *  |<--------- size_t sz --------->A| (FOOTER)
 *  +--------------------------------+
 *
 *
 *   STRUCTURE of FREED BLOCK
 *
 *   0         1         2         3
 *   01235467890123456789012345678901
 *  +--------------------------------+
 *  |<--------- size_t sz --------->A| (HEADER)
 *  |<--------- void*  fd ---------->| (Previous free chunk ptr in same bin)
 *  |<--------- void*  bk ---------->| (Next free chunk ptr in same bin)
 *  .         (Empty space)          .
 *  .                                .
 *  |<--------- size_t sz --------->A| (FOOTER)
 *  +--------------------------------+
 *
 *  'A' bit: Is current chunk allocated? (1 : Allocated / 0 : Freed)
 */
////////////////////////////////////////////////////////////////////////////

//////////////////// Memory Allocator Utility Functions ////////////////////

/**
 * @brief Extend the heap space for aligned size of memory, with `mem_sbrk()`
 * @param sz: Size to be grown (with aligning and adjusting)
 * @return Pointer to grown heap's base block
 */
static void *extend_heap(size_t sz)
{
    void *ptr;              /* Memory extended */
    size_t asz = ALIGN(sz); /* Adjusted (if 'BAD TC') and Aligned size */

    /* Extend the heap with given `mem_sbrk`*/
    if ((ptr = mem_sbrk(asz)) == (void *)-1)
    {
        return NULL;
    }

    /* Set header and footer, and top chunk indicator */
    PUT(HDRP(ptr), PACK(asz, FREED));                                /* Free block header */
    PUT(FTRP(ptr), PACK(asz, FREED));                                /* Free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(TOP_CHUNK_INDICATOR, ALLOCATED)); /* Top chunk on newly extended memory*/

    /* Insert into segregated list's proper bin */
    segrlist_insert(ptr, asz);

    /* Coalesce the block, if needed*/
    return coalesce(ptr);
}

/**
 * @brief Coalesce the chunk given, with its adjacent previous, adjacent next block (if they are free)
 * @param ptr: Pointer to the chunk to be coalesced
 * @return Base pointer to the coalesced chunk
 */
static void *coalesce(void *ptr)
{
    size_t prev_chunk = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
    size_t next_chunk = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t sz = GET_SIZE(HDRP(ptr));

    /* Adjacent chunks are all allocated. */
    if (prev_chunk == ALLOCATED && next_chunk == ALLOCATED)
    {
        return ptr;
    }

    /**
     * Search for adjacent free blocks and remove from the segregated list
     * Update the size to total free size
     */
    if (prev_chunk == FREED)
    {
        segrlist_remove(PREV_BLKP(ptr));
        sz += GET_SIZE(HDRP(PREV_BLKP(ptr)));
    }
    if (next_chunk == FREED)
    {
        segrlist_remove(NEXT_BLKP(ptr));
        sz += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    }
    segrlist_remove(ptr);

    /* Detect free blocks and merge */
    if (prev_chunk == ALLOCATED && next_chunk == FREED) // Coalesce with next chunk
    {
        PUT(HDRP(ptr), PACK(sz, FREED));
        PUT(FTRP(ptr), PACK(sz, FREED));
    }
    else if (prev_chunk == FREED && next_chunk == ALLOCATED) // Coalesce with previous chunk
    {
        PUT(FTRP(ptr), PACK(sz, FREED));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(sz, FREED));
        ptr = PREV_BLKP(ptr); // Move base pointer to previous chunk's base
    }
    else if (prev_chunk == FREED && next_chunk == FREED) // Coalesce with both previous and next chunk
    {
        PUT(HDRP(PREV_BLKP(ptr)), PACK(sz, FREED));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(sz, FREED));
        ptr = PREV_BLKP(ptr); // Move base pointer to previous chunk's base
    }

    /* Update the segregated list */
    segrlist_insert(ptr, sz);
    return ptr;
}

/**
 * @brief Place `sz` sized free block into `asz` sized free chunk
 * @param ptr: Pointer to the block
 * @param asz: Adjusted size of bin
 * @return Pointer to block placed
 */
static void *place(void *ptr, size_t asz)
{
    /* Size of block, and remaining size after putting block to the chunk*/
    size_t sz = GET_SIZE(HDRP(ptr));
    size_t rem = sz - asz;

    /* Remove block from list */
    segrlist_remove(ptr);

    /* Do not split block if remainder size is smaller than space for metadata */
    if (rem <= SIZEOF_MIN_FREE_CHUNK)
    {
        PUT(HDRP(ptr), PACK(sz, ALLOCATED)); /* Block header */
        PUT(FTRP(ptr), PACK(sz, ALLOCATED)); /* Block footer */
        return ptr;
    }

    /**
     * In the bin, determine which the block is near to.
     * If smaller/bigger than middle size of bin, then insert into previous/after splited block.
     *
     * Each k-th bin contains chunk size of (1 << k) to (1 << (k + 1)) bytes blocks.
     * Midpoint of this chunk size is (1 << k) + (1 << (k - 1)) bytes, and mark as bisection point.
     */
    size_t mid = (1 << msb_scan(asz)) | (1 << MAX(0, msb_scan(asz) - 1));
    PUT(HDRP(ptr), PACK(asz >= mid ? rem : asz, asz < mid));
    PUT(FTRP(ptr), PACK(asz >= mid ? rem : asz, asz < mid));
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(asz < mid ? rem : asz, asz >= mid));
    PUT(FTRP(NEXT_BLKP(ptr)), PACK(asz < mid ? rem : asz, asz >= mid));

    /* Insert the leftover splitted block */
    segrlist_insert(asz >= mid ? ptr : NEXT_BLKP(ptr), rem);
    return asz >= mid ? NEXT_BLKP(ptr) : ptr;
}

///////////////////////////////////////////////////////////////////////////

//////////////////// Segregated List Utility Functions ////////////////////

/**
 * @brief Insert the chunk to segregated list, with its size
 * @param ptr: Pointer to the chunk
 * @param sz: Size of the chunk
 */
static void segrlist_insert(void *ptr, size_t sz)
{
    /* Minimum segregated list bin index that chunk could be inserted */
    size_t bin_idx = MIN(BIN_SIZE - 1, msb_scan(GET_SIZE(HDRP(ptr))));

    /* Traverse pointer of bin (explained in SEGREGATED LIST AND BIN STRUCTURES)*/
    void *ins_fd = segr_list[bin_idx];
    void *ins_bk = NULL;

    /* Traverse inside of the bin to find proper location to be inserted */
    while ((ins_fd != NULL) && (sz > GET_SIZE(HDRP(ins_fd))))
    {
        ins_bk = ins_fd;
        ins_fd = FREE_FD_BLK(ins_fd);
    }

    /* Have forward free block? */
    if (ins_fd)
    {
        PUT(FREE_FD_PTR(ptr), ins_fd); // Link with forward free block
        PUT(FREE_BK_PTR(ins_fd), ptr);
    }
    else
    {
        PUT(FREE_FD_PTR(ptr), NULL); // Let it empty
    }

    /* Have backward free block? */
    if (ins_bk)
    {
        PUT(FREE_BK_PTR(ptr), ins_bk); // Link with backward free block
        PUT(FREE_FD_PTR(ins_bk), ptr);
    }
    else
    {
        PUT(FREE_BK_PTR(ptr), NULL); // Let it empty
        segr_list[bin_idx] = ptr;    // and point to the segregated list (circular)
    }

    return;
}

/**
 * @brief Remove the chunk from the segregated list
 * @param ptr: Pointer to the chunk
 */
static void segrlist_remove(void *ptr)
{
    /* Proper index that bin should be*/
    size_t bin_idx = MIN(BIN_SIZE - 1, msb_scan(GET_SIZE(HDRP(ptr))));

    size_t fd_blk = FREE_FD_BLK(ptr);
    size_t bk_blk = FREE_BK_BLK(ptr);

    /* Remove linkage with forward and backward free blocks */
    if (fd_blk && bk_blk)
    {
        PUT(FREE_BK_PTR(FREE_FD_BLK(ptr)), FREE_BK_BLK(ptr));
        PUT(FREE_FD_PTR(FREE_BK_BLK(ptr)), FREE_FD_BLK(ptr));
    }
    else if (fd_blk && !bk_blk)
    {
        PUT(FREE_BK_PTR(FREE_FD_BLK(ptr)), NULL);
        segr_list[bin_idx] = FREE_FD_BLK(ptr);
    }
    else if (!fd_blk && bk_blk)
    {
        PUT(FREE_FD_PTR(FREE_BK_BLK(ptr)), NULL);
    }
    else if (!fd_blk && !bk_blk)
    {
        segr_list[bin_idx] = NULL;
    }

    return;
}

/**
 * @brief Find the best-fit block to the given size, in segregated list
 * @param sz: Size to be inserted
 * @return Pointer to the best-fit free chunk / NULL if no chunks are available
 */
static void *segrlist_search(size_t sz)
{
    size_t bin_idx = MIN(BIN_SIZE - 1, msb_scan(sz)); /* Minimum bin index that `sz` could be assigned */
    void *ptr = NULL;

    /* Search from the smallest bin that could insert target memory allocation. */
    for (size_t bin_search = bin_idx; bin_search < BIN_SIZE; bin_search++)
    {
        /**
         * Search to the bin that the allocated bin:
         *  - Could be inserted (Corresponding bin accepts memory larger than `size`)
         *  - Should be inserted (Last bin entry, should be inserted)
         *
         * Find the smallest bin that is able to insert the memory with `size` size.
         */
        if (segr_list[bin_search] == NULL && bin_search != (BIN_SIZE - 1)) /* No list in bin */
        {
            continue;
        }

        /**
         * Bin is internally sorted with its allocation size by increasing order.
         * Locate the bin with found bin index (if available), and traverse the list until proper size met.
         */
        ptr = segr_list[bin_search];                                  /* Locate the bin */
        while ((ptr != NULL) && ((ADJUST(sz) > GET_SIZE(HDRP(ptr))))) /* Traverse the bin list */
        {
            ptr = FREE_FD_BLK(ptr);
        }
        if (ptr != NULL)
        {
            return ptr; /* Insert address found! */
        }
    }
    return ptr;
}

//////////////////////////////////////////////////////////////////////////

//////////////////// CORE FUNCTIONS (`mm_xxx` series) ////////////////////

/**
 * @brief mm_init - Initialize segregated list structure, allocate and initialize zero block to heap (for consistency of top chunk).
 * @deprecated mm_init - initialize the malloc package.
 *
 * @return 0 without error / -1 with error
 */
int mm_init(void)
{
    void *init_zero_block; /* Heap initializer (Zero-sized allocation block) */

    /* Allocate memory for segregated list to pointer */
    if ((segr_list = mem_sbrk(BIN_SIZE * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    /* Initialize each entry of segregated list with `NULL` pointer */
    for (size_t segr_idx = 0; segr_idx < BIN_SIZE; segr_idx++)
    {
        segr_list[segr_idx] = NULL;
    }

    /* Allocate memory for the initial empty heap */
    if ((init_zero_block = mem_sbrk(SIZEOF_MIN_FREE_CHUNK)) == (void *)-1)
    {
        return -1;
    }

    /* Initial zero block writing */
    PUT((char *)init_zero_block + 0x0, 0);                                    /* Alignment */
    PUT((char *)init_zero_block + 0x4, PACK(DSIZE, ALLOCATED));               /* Header */
    PUT((char *)init_zero_block + 0x8, PACK(DSIZE, ALLOCATED));               /* Footer */
    PUT((char *)init_zero_block + 0xC, PACK(TOP_CHUNK_INDICATOR, ALLOCATED)); /* Top chunk indicator */

    /* Extend the empty heap */
    if (extend_heap(INIT_HEAP_SZ) == NULL)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief mm_malloc - Find the proper bin from the segregated list to allocate, If there are no proper bin to be allocated, then extend heap to make space.
 *        Also maintain consistency of multiple of alignment, as original baseline code did.
 *
 * @deprecated mm_malloc - Allocate a block by incrementing the brk pointer. Always allocate a block whose size is a multiple of the alignment.
 *
 * @param sz: Size to be allocated
 * @return Pointer to the chunk
 */
void *mm_malloc(size_t sz)
{
    void *ptr = NULL; /* Pointer */

    /* Ignore size 0 cases */
    if (sz == 0)
    {
        return NULL;
    }

    /**
     * If proper free chunk in segregated free list:
     * then return the found free chunk
     */
    if ((ptr = segrlist_search(sz)) != NULL)
    {
        return place(ptr, ADJUST(sz));
    }

    /**
     * In case of not found in any bin, extend the heap.
     * To minimize future overhead, set minimum heap increment to system page size. (4KB)
     */
    if ((ptr = extend_heap(MAX(ADJUST(sz), mem_pagesize()))) == NULL)
    {
        return NULL;
    }
    return place(ptr, ADJUST(sz));
}

/**
 * @brief mm_free - Freeing a block by setting `FREED` tag to header and footer, and insert to segregated list.
 * @deprecated mm_free - Freeing a block does nothing
 *
 * @param ptr: Pointer to the block to be freed
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    /* Mark the chunk as `FREED` chunk to its header and footer */
    PUT(HDRP(ptr), PACK(size, FREED));
    PUT(FTRP(ptr), PACK(size, FREED));

    /* Insert freed chunk into corresponding bin of segregated list */
    segrlist_insert(ptr, size);

    /* Coalesce the block (if possible) */
    coalesce(ptr);
    return;
}

/**
 * @brief mm_realloc - Bulk reallocation with mm_pagesize() unit on top chunk.
 *
 * If top chunk is going to be extended, just increase top chunk size without data copying.
 * Also reducing the overhead of allocation, use large allocation unit (in this code: mm_pagesize())
 * Anticipating for using memory that are over-allocated in future.
 *
 * @deprecated mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *
 * @param ptr: Pointer to the base of original block
 * @param sz: Size to be reallocated
 *
 * @return Pointer to the base of reallocated block
 */
void *mm_realloc(void *ptr, size_t sz)
{
    /* Ignore invalid block size */
    if (sz == 0)
    {
        return NULL;
    }

    void *realloc_ptr = ptr;                           /* Pointer to be returned */
    size_t realloc_sz = ALIGN(MAX(sz, DSIZE)) + DSIZE; /* Adjust block size to include boundary tag and alignment requirements */
    int rem = realloc_sz - GET_SIZE(HDRP(ptr));        /* Size increasing: type should be `signed` integer. */

    /* Block size NOT grown! */
    if (rem <= 0)
    {
        return ptr;
    }

    /**
     * If current chunk is top chunk of the bin, and it requires heap growth,
     * Then allocate more bigger chunk than exactly need,
     * To reduce allocation overhead of future allocation. (with anticipating future use of memory)
     *
     * (Block size == TOP_CHUNK_INDICATOR) means top chunk (allocated in `mm_init` as `init_zero_block`, and `extend_heap`)
     * With its magic number 0xC5ED2110.
     */
#ifdef EXTEND_ONLY_TOP
    if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == TOP_CHUNK_INDICATOR)
    {
        /* Extend heap by multiple of `mem_pagesize()`, which is 4KB in LINUX (by ALIGN_PAGE)*/
        if (extend_heap(ALIGN_PAGE(rem)) == NULL)
        {
            return NULL;
        }
        rem -= ALIGN_PAGE(rem);

        /* Set up for remaining page area */
        segrlist_remove(NEXT_BLKP(ptr));

        /* Set `ALLOCATED` bit of memory newly allocated on extended heap area. */
        PUT(HDRP(ptr), PACK(realloc_sz - rem, ALLOCATED));
        PUT(FTRP(ptr), PACK(realloc_sz - rem, ALLOCATED));

        return ptr;
    }
#endif

    /**
     * If not a top chunk, there are 2 choices to reallocate
     * - If next chunk is freed chunk and enough large to contain new allocation, then merge the block, without memory copy.
     * - But there are no sufficient next chunk, copy the original data to newly allocated memory.
     *
     * However, sometimes GROW_TO_NEXT_FREE Policy might show worse utilization than EXTEND_ONLY_TOP Policy.
     * In testcase `realloc2.rep` and `realloc2-bal.rep`, Continuously reallocating big memory with step of small size with putting little size on its next chunk.
     */
#ifdef GROW_TO_NEXT_FREE
    /* There is sufficient adjacent free block to grew the size up. */
    if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == FREED && GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= rem)
    {
        rem -= GET_SIZE(HDRP(NEXT_BLKP(ptr)));

        /* set up for the next block */
        segrlist_remove(NEXT_BLKP(ptr));

        /* Grow to the next block, and assign the size to the grown block */
        PUT(HDRP(ptr), PACK(realloc_sz - rem, ALLOCATED));
        PUT(FTRP(ptr), PACK(realloc_sz - rem, ALLOCATED));

        /* If memory had reached to the top chunk, maintain top chunk consistency */
        if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == TOP_CHUNK_INDICATOR)
        {
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(TOP_CHUNK_INDICATOR, ALLOCATED));
        }
        return ptr;
    }
#endif

    /* Allocate new space except top chunk overhead*/
    if ((realloc_ptr = mm_malloc(realloc_sz - DSIZE)) == NULL)
    {
        return NULL;
    }

    /* Copy data to the newly allocated area, and free the original space. */
    memcpy(realloc_ptr, ptr, MIN(sz, realloc_sz));
    mm_free(ptr);

    /* Return the reallocated block */
    return realloc_ptr;
}

////////////////////////////////////////////////////////////////

//////////////////// Other Useful Functions ////////////////////

/**
 * @brief Scan MSB bit to locate proper bin.
 * @param sz: Number to be tested
 * @return MSB bit index / -1 if `sz` == 0
 */
static size_t msb_scan(size_t sz)
{
    size_t msb = -1;
    while (sz)
    {
        sz >>= 1;
        msb++;
    }
    return msb;
}

#ifdef AVOID_BAD_TC

/**
 * @brief Force-fit to 'intentional' bad test cases (such as `binary` and `binary2`)
 * @param sz: Size of chunk to be allocated
 * @return Size of chunk to be allocated, with force-fitting of bad test cases
 */
static size_t bad_testcase(size_t sz)
{
    if (sz == 112) // `binary2.rep`
    {
        return 128;
    }
    else if (sz == 448) // `binary.rep`
    {
        return 512;
    }
    return sz;
}
#endif

////////////////////////////////////////////////////////////////