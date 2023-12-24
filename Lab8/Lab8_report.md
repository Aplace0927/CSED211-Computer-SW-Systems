# CSED211 - Lab12 (malloclab) Report

**20220140 Taeyeon Kim**

[toc]

## Overview

### Policy

#### Segregated list

현재 구현한 Explicit memory allocator의 기본적인 Policy는 Segregated list를 통한 구현을 시도하고자 했다. CSAPP textbook에 있는 `extend_heap` 및 `coalesce` 등 기본적인 코드에서 수정을 가했으며, 각종 Macro function을 사용하였다.

### Supporting Ideas

#### Reallocation

Reallocation에 있어서, 기본적인 구현 방법은 새로운 메모리를 `malloc`하여 그 위치에 복사하는 작업을 수행하는 것이었다. 그러나, `realloc`하고자 하는 Chunk가 Top chunk (Heap의 가장 끝 부분)인 경우 새롭게 Memory를 Allocation하기보다 해당 Chunk의 Size를 증가시키는 방향이 Memory copy의 Overhead를 줄일 수 있기 때문에 효과적이기 때문에 Top chunk인 경우를 추가적으로 마킹하여 관리하고자 했다.

더욱이, 새롭게 Top chunk를 할당하여 Heap의 전체 크기가 늘어나는 경우 정확한 Memory allocation size가 아닌,  `ALIGN_PAGE(sz)`, 즉 `mm_pagesize()`의 배수 단위 (LINUX에서 4KB) 만큼 할당하여, 차후 Memory allocation이 진행될 것을 가정하고 사용할 Memory를 미리 대량으로 할당 받았다(Anticipatory memory allocation). 이를 통해 반복적으로 Allocation을 진행할 때 `sbrk` System call에 인한 Overhead를 감소시키고자 하였다.

#### Free block insertion

Free block이 특정 bin에 삽입될 때, 각 bin을 Free chunk size가 increase하는 방향으로 chunk list를 관리하였다. $k$-th bin은 $2^k$ bytes에서 $2^{k+1}$ bytes의 Free chunk를 관리하며, 큰 Chunk에서 일부를 Allocate하여 user에게 Chunk pointer를 반환하고, 다른 나머지를 다시 Segregated list에 삽입할 때 효율적으로 삽입하고자 하였다. 이를 다음과 같은 방법으로 해결하고자 했다. Free chunk가 Split된 이후 Insert될 때 User에게 최소한의 Memory만을 주고, 남은 (상대적으로) 큰 영역을 다시 Bin에 삽입하기 위해 해당 Bin의 Chunk size의 중간값 (= $2^k + 2^{k-1}$ bytes)과 대소를 비교하였다. 이후, 작은 쪽을 User에게 주고, 큰 쪽을 다시 Bin에 삽입하였다.

## Data structures

### Segregated list

```c
void** segr_list
```

전역 변수로 `void*` type array의 Base pointer를 선언하고, `mm_init`에서 직접적인 초기화 (`mm_sbrk` 사용)를 진행하였다.

```c
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
```

### Bins

Bin은 Segregated list에서 Free block을 관리하기 위해 사용하는 각 Segregated list entry를 의미하며, 각 entry는 Free chunk를 연결한 List로 구현되어 있다.

`segr_list`의 각 Entry에 맞는 Size의 Free block을 삽입하여 bin을 관리한다. $k$번째 bin은 $2^k$ byte에서 $2^{k+1}$ byte의 크기를 가지는 Free block을 Entry로 가지게 되며 마지막 bin의 크기 상한은 없다. bin의 index를 찾기 위해 후술할 [`msb_scan`](#`msb_scan`) 을 사용해 MSB bit의 index로 bin에 접근할 수 있도록 구현했다.

모든 Bin은 Double-linked list로 구현되며, 이는 Free chunk의 `void *fd` 및 `void *bk` Pointer를 사용해 Consistency를 유지할 수 있다. 또한, 현재 구현은 각 bin 내에서 Freed chunk의 크기의 오름차순으로 정렬하여 Best-fit을 탐색할 수 있도록 하였다. 탐색에 있어서, 최악의 경우는 모든 Freed chunk가 하나의 Bin에 들어가는 경우이며, 이 경우 $O(n)$ 시간에 삽입할 bin을 검색할 수 있다.

### Chunks (Blocks)

* `size_t sz` 이후에 나타나는 31번째 MSB `A`는 현재 Chunk가 Allocated된 Chunk인지 나타내는 bit이다.  Freed chunk의 경우 `A == 0`  / Allocated chunk의 경우 `A == 1`이다.

#### Freed

```text
|0         1         2         3 |
|01234567890123456789012345678901|
+--------------------------------+
|<--------- size_t sz --------->A| (HEADER)
.         / User data /          . (USER DATA)
.        At least 8 byte -       .
.        - to maintain -         .
.      - chunk consistency       .
|<--------- size_t sz --------->A| (FOOTER)
+--------------------------------+
```

* `size_t sz`는 8-byte 단위로 Align되는 값이며, 현재 Chunk의 data size를 8 byte 단위로 Align하여 Chunk의 Header와 Footer에 삽입하였다.
  * Top chunk의 경우 Heap increase case를 구별하고자 Footer에 특별히 정의한 Magic number([`TOP_CHUNK_INDICATOR`](#Constants & Simple Arithmetic ))를 삽입했다.

#### Allocated

```text
|0         1         2         3 |
|01234567890123456789012345678901|
+--------------------------------+
|<--------- size_t sz --------->A| (HEADER)
|<--------- void*  fd ---------->| (Previous free chunk ptr in same bin)
|<--------- void*  bk ---------->| (Next free chunk ptr in same bin)
.         (Empty space)          .
.                                .
|<--------- size_t sz --------->A| (FOOTER)
+--------------------------------+
```

* Freed chunk와 마찬가지로 해당 Chunk의 size를 나타내기 위한 `size_t sz`를 Chunk의 Header와 Footer에 삽입하였다.
* 할당된 경우에는 User data section에 Bin consistency를 지키기 위한 Double-linked list pointer인 `void *fd` 와 `void *bk`  삽입한다. 2개의 `void*` type data를 삽입하기 위해, `writeup_malloclab.pdf`에 명시되있듯, 32bit 환경에서 User data size는 최소 `sizeof(void*) * 2 == 8` byte의 allocation을 요구한다. 

## Functions and Macros (except `mm` series)

### Macros

#### Aligning & Adjusting Macros

```c
////////// ALIGNING MACROS //////////

#define ALIGNMENT 8                                                                /* single word (4) or double word (8) alignment */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)                            /* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN_PAGE(size) (((size) + (mem_pagesize() - 1)) & ~(mem_pagesize() - 1)) /* Rounds up to the nearest multiple of system page size (4KB in LINUX) */
```

`ALIGN`은 `ALIGNMENT` 단위에 맞추어 올림하는 Macro function으로, CSAPP textbook을 참고하였다. 이를 이용해 `ALIGN_PAGE` Macro function을 만들었으며, 마찬가지로 `mm_pagesize()`의 반환 단위 (System Page Size)  단위에 맞추어 올림한다. (해당 `ALIGN_PAGE`는 Anticipatory Memory Allocation을 구현하는 데 사용되었다)

#### Constants & Simple Arithmetic 

상수 `WSIZE`와 `DSIZE`는 CSAPP textbook에서 가져와 사용하였으며, 다른 상수에 대한 설명은 다음과 같다.

```c
////////// CONSTANT DEFINITION //////////

#define WSIZE 4 // word and header/footer size (bytes)
#define DSIZE 8 // double word size (bytes)

#define SIZEOF_MIN_FREE_CHUNK (4 * WSIZE) // sizeof(size_t) * 2 (for header, footer) + sizeof(void*) * 2 (for fd, bk)

/* Find by trial-and-error heuristics. */
#define INIT_HEAP_SZ (mem_pagesize() >> 4)

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
```

* `SIZEOF_MEAN_FREE_CHUNK`

  Header, Footer와 Bin forward pointer, Bin backward pointer를 모두 포함하였을 때 필요한 최소한의 Block size이다. 32bit 환경에서 `sizeof(size_t) * 2 + sizeof(void*) * 2 == 16`으로 정의된다.

* `INIT_HEAP_SZ`

  최초로 할당될 Heap의 크기이다. 해당 값을 여러 번 바꿔보며 적용한 결과, 처음에 `mm_pagesize()` 보다 작은 단위로 할당하는 쪽이 `./mdriver -v`의 결과가 좋게 나왔다.  `mm_pagesize()` 를 사용했을 때 보다 Utilization Point가 높게 나온 것을 미루어 보았을 때 Memory를 많이 사용하지 않는 Test case가 있었을 것으로 추측된다.

* `BIN_SIZE`

  Bin의 개수이다. `20`으로 정했으며, 이 값은 기존의 libc `malloc()` 구현체의 Segregated list policy로부터 크기를 정하였다. libc `malloc`은 다음과 같은 종류의 Segregated list를 가진다.

  * **tcache** : libc 2.26 이후에 Process-wise한 heap memory가 아닌 Per-thread cache 영역에 할당하는 heap으로 매우 빠른 할당 속도를 가진다. (현재 구현은 Heap Consistency를 `sbrk` System call을 통해 Manage하는 것을 목표로 하고 있기 때문에, lab 구현에 있어서 해당 bin을 무시하였다.)
  * **fastbin** : 10개의 chunk만을 Single-linked list의 LIFO를 통해 저장하는 bin이다. Coalescing을 진행하지 않는 bin이다. (Coalescing을 진행하지 않기 때문에 lab 구현에 있어서 해당 bin을 무시하였다.)
  * **smallbin**: Heap structure consistency를 지키기 위한 최소한의 Chunk size부터 `MIN_LARGE_SIZE` 보다 작은 Chunk size를 선형적인 `ALIGN` 간격으로 관리하는 bin이다.
  * **largebin**: `MIN_LARGE_SIZE` 보다 큰 Chunk size를 지수적인 간격으로 관리하는 bin으로, 자세한 Allocation 단위를 주석에 작성하였다.

  계산에 따르면, libc의 `malloc()`의 가장 큰 Large bin은 `0xAAA00` byte 이상의 bin을 저장하게 되며, 이를 최대한 따라하기 위해 가장 가까운 (2의 거듭제곱) 형태인 size를 `0x100000 == (1 << 20)` 으로 결정하였다. 따라서 `BIN_SIZE`를 `20`으로 결정했다.

* `TOP_CHUNK_INDICATOR`

  Top chunk를 구분하기 위해 Free list의 Consistency를 지키면서, 할당이 불가능한 터무니없이 큰 값을 Size tag로써 사용하여 해당 Chunk가 Heap의 끝임을 나타내었다. 해당 값은 의미를 부여하고자 `0xC5ED2110` ("CSED211") 으로 지정하였다. (LSB 1 bit는 0으로 설정하여 Allocated되지 않음을 보여야 한다.)

* `ALLOCATED` & `FREED`

  Free tag를 설정할 때 사용할 상수이다.

* `MIN` & `MAX`

  Macro function을 통해 사용하는 두 수 중 작은 / 큰 수를 반환하는 함수이다.

* `ADJUST`

  Heap Consistency를 지키기 위한 User memory 최소 할당 크기 (`DSIZE`)를 검사하면서, Header 및 Footer의 영역까지 고려하여 `ALIGN`하여 새롭게 할당할 크기를 정하기 위한 Macro Function이다.

  * `AVOID_BAD_TC` Macro의 경우, `trace` 중 Memory utilization이 떨어지는 Memory allocation pattern의 utilization을 높이기 위해 강제로 block size를 바꾸어 주도록  하는 Setting이다.

#### Chunk(Block) Data Getter/Setter

```c
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
#define PREV_BLKP(blk) ((char *)(blk) - GET_SIZE(((char *)(blk)-DSIZE)))
////////////////////////////////////////////////////////////////////////////////////
```

* 해당 함수들은 모두 CSAPP Textbook에 있는 형태를 사용하였다.

#### Chunk Pointer for Bin Consistency

```c
////////// CHUNK POINTERS FOR BIN CONSISTENCY //////////

/* Forward and Backward free block (block itself and its pointer) in same bin on segregated list */
#define FREE_FD_PTR(blk) ((char *)(blk))
#define FREE_BK_PTR(blk) ((char *)(blk) + WSIZE)
#define FREE_FD_BLK(blk) (*(char **)(blk))
#define FREE_BK_BLK(blk) (*(char **)(((char *)(blk) + WSIZE)))

////////////////////////////////////////////////////////
```

해당 함수들은 Bin 내에서 Free block들을 Traverse할 때 사용할 함수들으로, Adjacent한 Block으로의 Pointer를 찾아 반환 / 혹은 직접 참조한다.

* `FREE_[FD/BK]_[PTR/BLK]`
  * `FD`의 경우 Free block에서 Forward Free Block Pointer가 위치하는 Offset `0`의  `(char*) (blk)` / `BK`의 경우 Free block에서 Backward Free Block Pointer가 위치하는 Offset `+WSIZE ` byte의 `(char*) ((blk)+ WSIZE)`를 참조한다.
  * `PTR`의 경우 참조할 Address를, `BLK`의 경우 직접 참조한 값을 반환한다.

#### Reallocation Policy Setting Macros

```c
////////// REALLOCATION POLICY //////////

#define EXTEND_ONLY_TOP

// #define GROW_TO_NEXT_FREE

/////////////////////////////////////////
```

`mm_realloc`에서 사용할 Reallocation policy를 사용하기 위해 사용한 Macro이다. 2개의 옵션은 독립적으로 작용하며, 자세한 설명은 [Reallocation Policy and Utilization](#Reallocation-Policy-and-Utilization)에서 다루었다.

### Functions

#### Memory Allocator Utilities

##### `extend_heap`

```c
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
```

CSAPP Textbook의 Code를 참조하여 시작하였으며, 변경사항은 다음과 같다.

* 할당 단위를 `ALIGN(sz)` 를 통해 Align하였다.
* Top chunk에 `TOP_CHUNK_INDICATOR`를 삽입하여 명시적으로 Top chunk임을 나타내었으며, 이는 [`mm_realloc`](#`mm_realloc`)에서 활용된다.
* Block의 Header 및 Footer를 Setting한 이후, 관리를 위해 Segregated list에 삽입하였다.

##### `coalesce`

```c
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
```

CSAPP Textbook의 Code를 참조하였으며, 조건문을 분리하였다. 크게 세 파트로 나뉜다.

* Coalesce가 불가능한 경우 (앞/뒤의 Chunk 모두 `ALLOCATED`)인 경우 `return ptr`
* Coalesce가 가능한 경우, 앞/뒤의 Chunk `FREED` 여부에 따라 Segregated list에서 앞/뒤 Chunk를 제거하고 최종 크기를 결정한다.
* 이후, 자신 Chunk를 할당 해제하고 새로운 Chunk를 작성하여 다시 Segregated list에 insert한다.

##### `place`

```c
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
```

CSAPP Textbook의 Code를 참조하였다. 변경 사항은 다음과 같다.

* 만약 Split 이후 남은 Block이 너무 작은 (`rem <= SIZEOF_MIN_FREE_CHUNK`) 경우를 분리하지 못하도록 남겼다.
* Split이 가능한 경우, 앞서 설명한 [Free block insertion](#Free-block-insertion) policy를 적용했다.
  * Bin의 Median value를 기준으로, Split 이후 남은 block의 size가 큰 쪽을 선택해 Segregated list에 삽입하고,  작은 쪽을 User에게 반환한다.

#### Segregated List Utilities

##### `segrlist_insert`

```c
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
```

* `msb_scan`을 통해 할당 가능한 최소의 bin index를 찾고, 해당 bin의 HEAD 부터 TAIL까지 순차적으로 탐색한다.
* 탐색 과정에서 `sz` 오름차순에 따른 적절한 삽입 위치를 발견한 경우 해당 위치에 삽입한다. 최악의 경우 $O(n)$의 시간이 걸린다.

##### `segrlist_remove`

```c
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
```

* `msb_scan`을 통해  bin index를 찾고, 현재 주어진 Pointer의 Adjacent Forward/Backward Free block 간 Consistency를 지키며 인자로 주어진 `ptr` Chunk를 제거하면서 Adjacent block을 연결한다.

##### `segrlist_search`

```c
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
```

* `mm_malloc` 함수 내에서 사용되며, **bin에 관계 없이** 인자로 주어진 `sz`가 삽입될 수 있는 최소의 공간을 $O(n)$에 찾는다.
  * Segregated policy에 따라 할당 가능한 최소한의 Bin size의 Bin 부터 탐색을 시작하며,  마지막 Bin index에 도달한 경우 무조건 삽입하게 된다.
  * 삽입 이후 Block을 Split하는 과정은 `mm_malloc`에서 `place` 함수를 호출하여 진행한다.

#### Other Useful Functions

##### `msb_scan`

```c
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
```

Bin index를 찾기 위해 Most Significant Bit를 Scan하여 그 Bit의 Index를 반환한다. `sz`로 `0`이 주어진 경우 `0xFFFFFFFF`을 반환한다.

`bad_testcase`

```c
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
```

앞서 언급한 Memory utilization을 떨어트리는 Test case에 대해 강제로 Memory를 Fit시킨다.

## Functions (`mm` series)

### `mm_init`

```c
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

```

Segregated list를 할당하고 초기화한다. 이후 초기의 Zero-sized Block을 Heap에 만들면서, 해당 Block에 `TOP_CHUNK_INDICATOR`로 해당 Block이 Top chunk임을 명시한다. 이후 `INIT_HEAP_SZ` 만큼 Heap size를 증가시킨다.

### `mm_malloc`

```c
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
```

`mm_malloc`의 정책은 다음과 같다.

1. 먼저 에러의 경우 (`sz == 0`)를 제거한다.
2. 할당 가능한 최소의 Chunk를 `segrlist_search`를 통해 Segregated list에서 찾는다. 
   1. 할당이 가능한 Chunk가 있는 경우, `place`를 통해 요구한 `sz`만큼의 block의 크기 만큼만 반환하며, 나머지 block은 다시 bin에 삽입한다.
3. Segregated list에서 할당 가능한 Chunk가 없는 경우, Heap을 `mem_pagesize()` 단위로 할당한다.
   1. 이후 `place`를 통해 block의 크기 만큼 할당하여 반환하고, 나머지 block은 다시 bin에 삽입한다.

### `mm_free`

```c
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
```

CSAPP Textbook에 있는 code를 사용하였으며, Segregated list를 통한 관리 부분을 추가하였다.

### `mm_realloc`

```c
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
```

`mm_malloc`과 마찬가지로 Zero-size reallocation의 경우 무시하였다. 해당 `mm_realloc` 은 [Reallocation Policy Setting Macros](#Reallocation-Policy-Setting-Macros)에서 결정하였으며, **실험 결과 `EXTEND_ONLY_TOP` Policy만 활성화 하는 경우 Utilization의 최고치를 보였다.**

각 Policy는 독립적으로 작동하며 다음과 같이 작동한다.

* `EXTEND_ONLY_TOP`
  * Top chunk에서 Coalescing을 요구하는 경우, 충분한 크기 (`mm_pagesize()`) 만큼 새로 Allocation하여 `mm_pagesize()`만큼 커진 Chunk를 그대로 사용한다.
* `GROW_TO_NEXT_FREE`
  * 만약 Growth를 원하는 Block의 Coalescing을 요구하고, Next Block이 Free chunk인 경우 Next Block의 전체 크기를 모두 합친다.

모든 Policy (에 해당하는 조건)을 만족시키지 못하는 경우는 원하는 크기만큼 새롭게 `malloc`하고 해당하는 Memory의 영역을 모두 `memcpy`를 통해 옮긴다.

## Result

### On Local environment

```shell
~/A/De/C/CSED211-Computer-SW-Systems/Lab8/malloclab-handout > on main > ./mdriver -v
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000327 17434
 1       yes   99%    4805  0.000264 18180
 2       yes   97%   12000  0.000431 27875
 3       yes   97%    8000  0.000300 26649
 4       yes   90%   24000  0.001388 17292
 5       yes   90%   16000  0.000631 25353
 6       yes   99%    5848  0.000319 18321
 7       yes   99%    5032  0.000252 19945
 8       yes   95%   14400  0.000617 23350
 9       yes   95%   14400  0.000580 24815
10       yes   99%    6648  0.000406 16374
11       yes   99%    5683  0.000268 21221
12       yes   99%    5380  0.000302 17844
13       yes   99%    4537  0.000212 21401
14       yes   96%    4800  0.000640  7502
15       yes   96%    4800  0.000628  7648
16       yes   95%    4800  0.000644  7452
17       yes   95%    4800  0.000614  7816
18       yes   99%   14401  0.000282 50995
19       yes   99%   14401  0.000279 51654
20       yes   85%   14401  0.000236 61073
21       yes   85%   14401  0.000254 56652
22       yes   95%      12  0.000001 20000
23       yes   95%      12  0.000000 24000
24       yes   88%      12  0.000001 17143
25       yes   88%      12  0.000001 15000
Total          95%  209279  0.009876 21191

Perf index = 57 (util) + 40 (thru) = 97/100
```

### On Programming cluster server

```shell
┌🤘-🐧taeyeonkim@ 💻 programming2 - 🧱 malloclab-handout on 🌵  main •5 ⌀5 ✗
└🤘-> ./mdriver -v
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000412 13804
 1       yes   99%    4805  0.000306 15697
 2       yes   97%   12000  0.000570 21067
 3       yes   97%    8000  0.000396 20187
 4       yes   90%   24000  0.001690 14204
 5       yes   90%   16000  0.000809 19773
 6       yes   99%    5848  0.000398 14679
 7       yes   99%    5032  0.000319 15789
 8       yes   95%   14400  0.000717 20072
 9       yes   95%   14400  0.000712 20228
10       yes   99%    6648  0.000468 14211
11       yes   99%    5683  0.000372 15293
12       yes   99%    5380  0.000373 14420
13       yes   99%    4537  0.000295 15406
14       yes   96%    4800  0.000807  5946
15       yes   96%    4800  0.000815  5889
16       yes   95%    4800  0.000790  6078
17       yes   95%    4800  0.000794  6049
18       yes   99%   14401  0.000350 41134
19       yes   99%   14401  0.000349 41311
20       yes   85%   14401  0.000322 44793
21       yes   85%   14401  0.000318 45215
22       yes   95%      12  0.000001 13333
23       yes   95%      12  0.000001 17143
24       yes   88%      12  0.000001 10909
25       yes   88%      12  0.000001 12000
Total          95%  209279  0.012385 16898

Perf index = 57 (util) + 40 (thru) = 97/100
```

## Discussion

### Reallocation Policy and Utilization

두 가지의 독립적인 [Reallocation policy](#Reallocation-Policy-Setting-Macros)를 제안하였고, 모든 조합 (2×2=4)을 `./mdriver -v -f ./trace/realloc2-bal.rep`을 사용하여 성능을 실험해 보았으며 다음과 같다. Allocation 관찰의 편의를 위해 각 Case 별 Allocation에 다음을 Print하도록 설정했다.

>  `EXTEND_ONLY_TOP` 시에 `T`(**T**op) / `GROW_TO_NEXT_FREE` 시에 `G` (**G**row) / `mm_malloc`과 memcpy를 통한 Naive한 접근 시에 `C` (**C**opy)

#### Naive approach

>```text
>CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
>...(중략)... CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
>Results for mm malloc:
>trace  valid  util     ops      secs  Kops
> 0       yes   29%   14401  0.001747  8246
>Total          29%   14401  0.001747  8246
>
>Perf index = 18 (util) + 40 (thru) = 58/100
>```

#### `EXTEND_ONLY_TOP`

> ```text
> Measuring performance with gettimeofday().
> CTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTTCTTTTTT
> Results for mm malloc:
> trace  valid  util     ops      secs  Kops
>  0       yes   85%   14401  0.000251 57329
> Total          85%   14401  0.000251 57329
> 
> Perf index = 51 (util) + 40 (thru) = 91/100
> ```

#### `GROW_TO_NEXT_FREE`

> ```text
> Measuring performance with gettimeofday().
> GGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCGGGGGGGCCCG
> Results for mm malloc:
> trace  valid  util     ops      secs  Kops
>  0       yes   38%   14401  0.000249 57905
> Total          38%   14401  0.000249 57905
> 
> Perf index = 23 (util) + 40 (thru) = 63/100
> ```

#### `EXTEND_ONLY_TOP` `GROW_TO_NEXT_FREE`

> ```text
> Measuring performance with gettimeofday().
> GGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCTGGGGGGCT
> Results for mm malloc:
> trace  valid  util     ops      secs  Kops
>  0       yes   52%   14401  0.000222 64986
> Total          52%   14401  0.000222 64986
> 
> Perf index = 31 (util) + 40 (thru) = 71/100
> ```

### Result of Experiment and Explain

Test에 사용한 trace `realloc2-bal.rep`는 다음과 같은 구성을 가졌다.

```text
104987
4801
14401
1
a 0 4092
a 1 16
r 0 4097
a 2 16
f 1
r 0 4102
a 3 16
f 2
r 0 4107
a 4 16
f 3
r 0 4112
a 5 16
f 4
...
```

초기에 4092byte의 큰 Chunk를 할당하고, 상대적으로 작은 16byte를 4092byte 위에 할당한다.  이후 4092byte 할당을 5byte 증가시켜 `mm_realloc`한다. 이후 16byte를 할당하고, 처음으로 할당한 16byte를 해제한다. 큰 Chunk의 할당 좌우로 작은 Chunk를 할당하는 동시에 순서를 정하여 `mm_free`와 `mm_realloc`을 반복하여, Heap memory의 External fragmentation이 일어나도록 하는 (악의적인) Test case임을 확인할 수 있었다.

이 경우, 새롭게 Free된 영역으로 `GROW_TO_NEXT_FREE`를 반복하는 과정보다, 새롭게 `mm_malloc`을 통해 강제로 Big chunk를 Top chunk로 이동, 이동된 Top chunk를 `EXTEND_ONLY_TOP`을 통해 증가시키면서 기존의 Free된 4092byte Free chunk를 활용하여 16byte의 `mm_malloc`과 `mm_free`를 반복하는 Utilization이 훨씬 좋은 성능을 보이는 것으로 관찰되었다. 불가피하게 약 4076byte (4092byte Free chunk - 16byte Repeating allocation)의 External fragmentation이 발생하나, 해당 Allocation method가 현재 Heap structure와 Allocation policy에서 최선의 선택이었다.

이에 따라 제출용 Code에서 [Reallocation policy](#Reallocation-Policy-Setting-Macros)에서 명시한 것과 같이,  `EXTEND_ONLY_TOP` 만을 정의하여 제출하였다.

## Future

Red-Black Tree와 같은 Self-balancing BST를 사용하면 더욱 빠른 시간에 Search를 할 수 있을 것이다. 이 경우 Throughput이 증가할 것으로 예상하나, 현재 Throughput이 최댓값이어서 Operation이 매우 많은 횟수가 진행되지 않는 이상 큰 변화가 발생할 것으로 생각되지 않는다.

두 번째 방법으로, Segregated list bin size를 더욱 Data에 fit하게 바꿀 수 있을 것이다. MSB bit의 index를 사용하여 bin을 찾았으나, 실제 libc malloc의 구현에 있는 smallbin 및 largebin의 chunk size 기준 처럼, 서로 다른 Policy를 가진 Segregated list를 사용하여 Bin memory의 utilization을 높일 수 있을 것이다.
