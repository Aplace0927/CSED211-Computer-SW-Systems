# CSED211 - Lab8 & 9 (cachelab) Report

**20220140 Taeyeon Kim**

[toc]

## Cache Simulator

### Design Plan

#### Structures & Variables

##### `int E`, `int S`, `int B`

Cache specification을 저장하고 있는 값들이다. 각각 (# of log(Line)), (# of log(Set)), (# of Block bytes)을 의미하며, 프로그램의 `argv`로 전달되어 `main` 내의 `getopt`를 통해 Parse되고, 값을 저장한다. Cache simulator에서 전역적으로 관리되는 값이다.

##### `typedef struct CacheStructure`

Cache의 한 블럭을 시뮬레이션하기 위한 구조체로,  다음과 같은 멤버를 두고 있다.

* `int valid`: 현재 Cache의 Valid bit이다.
* `int clock`: 현재 Cache의 LRU를 구현하기 위해 둔 변수이다. 최근에 바뀔수록 값이 커지도록 구현했다.
* `__uint64_t tag`: Tag bit로, 주소를 나타내기 위해 64bit 자료형을 사용하고 있다.

해당 구조체를 2차원으로, Cache simulator에 주어진 Set과 Line의 개수에 맞게 `malloc`하여 전역 변수로써 `CacheStructure **Cache`를 사용하고 있다.

##### `int CacheAssignedTick`

`CacheStructure`의 `clock` 멤버에 들어가며, `CacheStructure`에 접근(Cache Hit, Cache Modify)이 일어날 때  이 값을 increment하고,  `clock`에 assign한다.  Cache simulator에서 전역적으로 관리되는 값이다.

##### `int HIT_COUNT`, `int MISS_COUNT`, `int EVICT_COUNT`

Cache simulation에서 Cache의 Hit, Miss, Eviction 횟수를 저장하는 값이다. Cache simluator에서 전역적으로 관리되는 값이다.

#### Algorithms

##### Cache Operation Handler (`void CacheRun`)

`valgrind` tool을 이용해 나온 메모리 Access는 `I` ,`L`, `S`, `M`이 있으며, 이는 각각 **I**nstruction load, Data **L**oad, Data **S**tore, Data **M**odify를 의미하며, 우리가 관심 있는 메모리의 접근은 Data의 Load, Store, Modify이다. 각각 Load, Store은 해당 주소의 메모리에 한 번 접근이 일어나며, Modify는 두번 접근이 일어난다. (해당 주소 접근 + 해당 주소 덮어쓰기)

따라서 다음과 같은 형식으로 구현했다.

```c
void CacheRun(char *TraceFileName)
{
    char Buffer[0x400];

    FILE *TraceFile = fopen(TraceFileName, "r");
    assert(TraceFile != NULL);

    long long unsigned int Address;
    __uint32_t Length;

    while (fgets(Buffer, 0x400, TraceFile))
    {
        if (Buffer[1] == 'S' || Buffer[1] == 'L' || Buffer[1] == 'M')	// S, L, M은 공백이 앞에 한 칸
        {
            sscanf(&Buffer[3], "%llx,%u", &Address, &Length);	// Address, Size

            if (VERBOSE)
            {
                printf("%c %llx,%u ", Buffer[1], Address, Length);
            }

            CacheAccess(Address);	// 해당 Address를 가지고 Cache에 Access.

            if (Buffer[1] == 'M')
            {
                CacheAccess(Address);	// Data modify instruction인 경우 한번 더 Access.
            }

            if (VERBOSE)
            {
                printf("\n");
            }
        }
    }

    fclose(TraceFile);
    return;
}
```

##### Cache Access Handler (`void CacheAccess`)

Cache에 접근 시, 해당 Cache simulator 프로그램의 `argv`로 주어진 Cache의 Specification (# of Set, # of Line, # of Bytes/Block)의 값을 확인한다. 다음과 같은 순서로 실행된다.

* 함수가 호출되고, 인자로 넘어간 `__uint64_t Address`의 Bitmasking을 통해 다음을 구한다.
  * **Set index**: `__uint64_t SetID = (Address >> B) & ((1U << S) - 1);`
  * **Tag**: `(Address >> (B + S));`
* 해당 Set에 있는 모든 Line에 대해 **Cache Hit 여부를 확인하기 위해** 다음을 수행한다.
  * Valid하지 않다면, 다음 Line을 검사한다.
  * Tag가 일치하지 않는 경우에도 다음 Line을 검사한다.
  * 그 이외의 경우는 **Cache Hit**의 경우이다.
    * 해당 Cache의 `clock` field의 값을 `++CacheAssignedTick`으로 갱신하여, 최근에 접근했음을 기록한다.
    * Global variable `HIT_COUNT`를 increment하고, 바로 반환하여 함수를 빠져나온다.
  * 반복문에서 빠져나온 경우 **(= Tag에 맞는 Cache Block이 없는 경우)는 Cache Miss**가 되는 경우이다.
    * Global variable `MISS_COUNT`를 Increment한다.
  * 이 경우, Temporal locality에 따라 Miss되었던 Tag를 저장해야 한다. 저장될 Line은 다음과 같이 선정하였다.
    * 모든 Cache block 중 `valid == 0`인 block이 있다면 (invalid) 해당 Cache block을 선정한다.
      * 이 경우 Cache Eviction이 일어나지 않는다.
    * 그 외의 경우, LRU Policy에 따라 `clock` 값이 가장 낮은 (가장 최근에 접근된 적 없는) Cache block을 선정한다.
      * 이 경우 Valid한 Cache block을 덮어썼기 때문에 Cache Eviction이 발생하게 된다. Global variable `EVICT_COUNT`를 Increment한다.
    * 선정된 Cache block에 값을 작성한다. `tag` 를 수정하고, `clock` 을 `++CacheAssignedTick`으로, `valid`를 `1`로 바꾸어 Cache가 가장 최근에 바뀌었음을 나타냈다.

다음과 같이 구현했다.

```c
void CacheAccess(__uint64_t Address)
{
    __uint64_t SetID = (Address >> B) & ((1U << S) - 1);
    __uint64_t Tag = (Address >> (B + S));
    __uint64_t line_idx = 0;

    while (line_idx < E)
    {
        if (!Cache[SetID][line_idx].valid)
        {
            line_idx++;
            continue;
        }

        if (Cache[SetID][line_idx].tag != Tag)
        {
            line_idx++;
            continue;
        }

        Cache[SetID][line_idx].clock = ++CacheAssignedTick;
        line_idx++;

        // Cache hit case
        if (VERBOSE)
        {
            printf("hit ");
        }

        HIT_COUNT++;
        return;
    }

    // Cache miss case
    if (VERBOSE)
    {
        printf("miss ");
    }

    MISS_COUNT++;

    line_idx = 0;
    int EvictionPolicy = 0x7FFFFFFF;
    int ReplaceLine = 0;

    while (line_idx < E)
    {
        if (!Cache[SetID][line_idx].valid)
        {
            ReplaceLine = line_idx;
            break;
        }
        if (Cache[SetID][line_idx].clock < EvictionPolicy)
        {
            EvictionPolicy = Cache[SetID][line_idx].clock;
            ReplaceLine = line_idx;
        }
        line_idx++;
    }
    if (Cache[SetID][ReplaceLine].valid)
    {
        // Cache eviction case: valid but remove!
        EVICT_COUNT++;
        if (VERBOSE)
        {
            printf("eviction ");
        }
    }
    Cache[SetID][ReplaceLine].tag = Tag;
    Cache[SetID][ReplaceLine].clock = ++CacheAssignedTick;
    Cache[SetID][ReplaceLine].valid = 1;

    return;
}
```

#### Cache Memory Manager (`CacheStructure **CacheCreate`, `void CacheFree`)

해당 함수들의 인자는 `CacheStructure **CacheCreate(__uint32_t Set, __uint32_t Line)`, `void CacheFree(CacheStructure **CacheArray, __uint32_t Set)`이며, 각각 `__uint32_t` 값들로는 `S`와 `E`의 값이 아닌, `1 << S`와 `1 << E`의 값이 전달된다.

2D array를 `malloc`을 통해 할당했으며, Error handling은 `assert`를 사용해 처리했다. 전체 코드는 다음과 같다.

```c
CacheStructure **CacheCreate(__uint32_t Set, __uint32_t Line)
{
    CacheStructure **CacheArray = (CacheStructure **)malloc(sizeof(CacheStructure *) * Set);
    assert(CacheArray != NULL);

    for (size_t set_idx = 0; set_idx < Set; set_idx++)
    {
        CacheArray[set_idx] = malloc(sizeof(CacheStructure) * Line);
        assert(CacheArray[set_idx] != NULL);

        for (size_t line_idx = 0; line_idx < Line; line_idx++)
        {
            CacheArray[set_idx][line_idx].valid = 0;
            CacheArray[set_idx][line_idx].clock = 0;
            CacheArray[set_idx][line_idx].tag = 0;
        }
    }

    return CacheArray;
}
```

```c
void CacheFree(CacheStructure **CacheArray, __uint32_t Set)
{
    for (size_t set_id = 0; set_id < Set; set_id++)
    {
        free(CacheArray[set_id]);
    }
    free(CacheArray);
    return;
}
```

## Optimized Matrix Transpose

먼저, 주어진 채점 상황은 `-s 5 -E 1 -b 5`로 주어진다. Set은 32개이고, 한 Block당 32byte이며, 한 Set당 1 Line이 있다.(Direct-mapped), 따라서 Set이 겹치는 Element가 있다면 항상 Cache Miss와 Eviction이 일어나게 된다.

모든 함수 내에서 생성 가능한 변수는 최대 12개로 제한된다. 32×32와 64×64의 크기와, 생성 가능한 변수의 개수를 조합하여 고려해 보았을 때 최대 한 번에 8개의 Index에 접근할 수 있으며 (Spatial locality), 나머지를 Bitmasking을 통해 반복 변수를 최대한 줄이는 방향으로 설계하였다.

따라서, 다음과 같은 12개의 지역변수로 전치를 수행하고 있다.

```c
int R, C, rT, cT;
int T0, T1, T2, T3, T4, T5, T6, T7;
```

* `R`과 `C`는 외부 반복문에서 사용하는 변수이다.
* `rT`와 `cT`는 내부 반복문에서 사용하는 변수이다.
* `T0` ~ `T7`은 한 번에 접근 가능한(Spatial Locality) Matrix의 연속된 8개의 메모리에 접근하기 위해 사용하는 변수들이다.

### 32×32

#### Analysis

Cache의 1 Block은 32byte이다. 따라서 한 번 Miss (혹은 Eviction까지) 일어나, Submatrix의 첫 원소를 Caching한다면 그 뒤의 8개 원소 (32byte / `sizeof(int)`) 를 Hit할 수 있는 것이다. 따라서 Block size를 8로 지정하였다 (`#define BLOCK_SIZE 8`)

Eviction을 고려하지 않고, Miss의 개수만 Count하는 경우 다음과 같이 Miss 횟수를 분석할 수 있다.

* **Source matrix: 가로를 Caching**하는 경우: 32byte씩 Caching하기 때문에 **연속적인 8개의 Entry마다 최대 1번의 (Eviction + Miss)**가 발생한다. (단, 첫 번째 반복에선 Miss만 발생)

  * 따라서 전체 32×32 Matrix에서 최대 (32 * 32) / 8 = **128회의 (Eviction + Miss)**가 발생할 수 있다.

* **Destination matrix: 세로를 Caching**하는 경우: 8줄 단위로 다음과 같은 일이 일어난다.

  * $8n\ (n = 0,1,2...)$ 번째 줄에서 **8번의 Cache (Eviction + Miss)**가 일어난다. (단, 첫 번째 반복에선 Miss만 발생)
    * Caching당할 element들은 세로줄이 다르기 때문에 (가로 * sizeof(int))의 Set을 구하면 `(32 * 4) >> 5 & ((1 << 5) - 1) == 4`이기 때문에 8개의 세로줄 시작 index가 4개의 Set을 간격으로 Caching당하는 것을 확인할 수 있다. 따라서 (8개의 세로줄 시작 index) * (4 set interval) == 32로 서로 겹침으로 인해 Miss가 발생하지 않는다.
    * 8n 번째 Entry에서 서로 Caching당하는 Set index가 같기 때문에 - (8개의 세로줄 시작 index) * (4 set interval) == 32인데 (# of Set) == 32이기 때문 - **Eviction과 Miss가 동시에 일어난다.**
  * $8n + k\ (k = 1,2,3,4,5,6,7)$ 번째 줄에서 **Cache Hit**이 일어난다.
    * 해당 Element들은 $8n$ 번째 줄에서 Caching이 Miss가 일어났을 때 32bytes씩 Caching되어 이미 Hit될 수 있다.
  * 따라서 전체 32×32 Matrix를 Transpose하기 위해: (8 Cache Miss per 8 * 8 Block) * (32 / 8 Block per Row) * (32 / 8 Block per Column) = 8 * 4 * 4 = **128회의 Eviction + Miss**가 발생할 수 있다.

* **Cache Set Index**가 겹치는 경우

  * 배열이 생성된 Offset에 의해 Set이 우연히 겹치는 경우, 다음과 같은 Caching miss가 일어날 수 있다. 실제로 이 현상이 Transpose 과정에서 일어났다.

    * 가로줄의 경우, 아래 코드의 가장 내부 Loop에서 사용하는 Set index는 고정되어있다.
    * 세로줄의 경우, 아래 코드의 가장 내부 Loop에서 사용하는 Set index는 $4n + k\ (k = 0,1,2,3)$ 꼴이다.

    여기서 **(가로줄의 Set index % 4)  == $k$** 인 경우 내부 Loop를 한 번 돌 때 마다 **2번의 Miss와 Eviction이 발생한다.** (가로줄 Caching에 대해 + 세로줄 Caching에 대해 각각 1번씩)

  * 따라서 전체 32×32 Matrix를 Transpose하기 위해 (32 / 8 Block per Row) * (32 / 8 Block per Column) * (2 Miss + Eviction per Block) = 4 * 4 * 2 = **32회의 Miss와 Eviction**이 발생할 수 있다.

따라서 위 논리에 따라 계산해 보았을 때 다음과 같은 Naive한 계산 결과를 얻는다.

* 최선의 경우 대략 **256**회의 Cache miss가 발생할 수 있다, 우연히 Cache set index가 겹치는 경우 **256 + 32 = 288**회의 Cache miss가 발생할 수 있다.
* Cache Eviction은 Cache miss보다 적게 발생하며, 이는 처음 반복문을 시작할 때 Eviction이 일어나지 않고 단지 Cache miss만 일어나기 때문이다.

32×32의 Matrix와, 한 번에 8개의 Index에 접근할 수 있는 Locality를 지니고 있다. 따라서 32×32 Matrix를 8×8 Matrix로 쪼개었고, Sub 8×8 Matrix에서 Transpose를 수행할 때 8개의 변수를 통해 한 Row를 한번에 transpose하는 방향을 적용하였다. 

#### Solution

수행 결과는 다음과 같다.

```c
if (M == 32)
    {
        for (R = 0; R < N; R += BLOCK_SIZE)
        {
            for (C = 0; C < M; C += BLOCK_SIZE)
            {
                for (rT = 0; rT < BLOCK_SIZE; rT++)
                {

                    T0 = A[R + rT][C];
                    T1 = A[R + rT][C + 1];
                    T2 = A[R + rT][C + 2];
                    T3 = A[R + rT][C + 3];
                    T4 = A[R + rT][C + 4];
                    T5 = A[R + rT][C + 5];
                    T6 = A[R + rT][C + 6];
                    T7 = A[R + rT][C + 7];

                    B[C][R + rT] = T0;
                    B[C + 1][R + rT] = T1;
                    B[C + 2][R + rT] = T2;
                    B[C + 3][R + rT] = T3;
                    B[C + 4][R + rT] = T4;
                    B[C + 5][R + rT] = T5;
                    B[C + 6][R + rT] = T6;
                    B[C + 7][R + rT] = T7;
                }
            }
        }
    }
```

* 8×8 Sub-matrix마다 (`R`, `C` 변수를 사용하는 `for` loop)
* 8개 의 행에 대해 (`for (rT = 0; rt < BLOCK_SIZE; rT++)`)
* 연속된 8개의 Matrix element에 접근하고, Temporary variable `T0` ~ `T7`을 통해 기록하고 Transpose한다.

수행 결과는 다음과 같았다.

![](images\32x32.png)

예상과 동일하게 총 **1765** Cache Hits, **288** Cache Misses, **256** Cache Eviction이 일어났다.

### 64×64

#### Analysis

앞서 사용한 전략을 사용할 경우 단순히 Matrix size의 차이만큼 **(288 * (64 * 64) / (32 * 32)) == 1152** Cache miss가 **일어나지 않을 것이다.** 이유는 다음과 같다.

* 32×32에서는 32개의 Cache set에서 서로 겹치지 않게 하기 위해 다음과 같이 계산했다.
  * 세로줄 간격이 `sizeof(int)` * 32 = 128 byte였고, 이를 Set index의 차이를 계산하면 **4**가 나오기 때문에 32개의 Cache set에서 최대 (32 Cache set) / (4 Set index diff) = **8**개의 세로줄 시작 Index를 한번에 Caching하였다.
  * 그러나, 64×64의 한 줄은 `sizeof(int)` * 64 = 256 byte이고, 이를 Set index의 차이를 계산했을 때 **8**이 나오기 때문에 같은 논리를 적용하였을 때 (32 Cache set) / (8 Set index diff) = **4개의 세로줄 시작 Index를 Caching하는 것이 바람직할 것이다.** **이를 코드 내에서 `#define SUBBLOCK_SIZE 4` 로 정의하였다.**
* 하지만, 32×32에서 적용한 8×8 Subblock transpose를 4×4 Subblock transpose에 적용한다면 가용되는 Cache Set index의 개수는 반으로 줄어들게 된다. 또한, 하지만 `SUBBLOCK_SIZE 4`라면 16byte를 Caching하기 때문에 Cache가 한번에 32byte를 Caching하는 것과 모순이 된다. 
* 따라서 다음과 같이 적용해야 한다.
  * 세로줄 index를 Caching 시 Set index를 최대로 사용하기 위해 **4개의 세로줄을 Caching한다.** / 가로줄은 한번에 **8개의 `int`가 Caching된다.**
  * 따라서 **(가로 8 × 세로 4)**와 **(가로 4 × 세로 4)** 사용해 Matrix를 Transpose하는것이 바람직하다.


(각각 4번의 Loop를 합쳐서 생각했을 때) 다음과 같은 과정을 이용해 Transpose를 진행한다.

* $A = \begin{bmatrix} T_1 & T_2 \\ T_3 & T_4 \end{bmatrix}$ , $B = \begin{bmatrix} - & - \\ - & - \end{bmatrix}$에서 시작한다.
* 첫번째로, **(가로 8 × 세로 4)**의 $\begin{bmatrix} T_1 & T_2 \end{bmatrix}$를  옮긴다. **이 과정에서 $T_1$ 과 $T_2$가 한 번에 Caching 된다.** 
  * $A = \begin{bmatrix} T_1 & T_2 \\ T_3 & T_4 \end{bmatrix}$ , $B = \begin{bmatrix} T_1^T & - \\ T_2 & - \end{bmatrix}$
* 두번째로,  Caching된 $T_2$를 $B$에서 $B$로 Transpose하여 옮기고, $T_3$을 $A$에서 $B$로 Caching하여 Transpose한다. **이 과정에서 $T_3$과 $T_4$가 한번에 Caching된다.**
  * $A = \begin{bmatrix} T_1 & T_2 \\ T_3 & T_4 \end{bmatrix}$ , $B = \begin{bmatrix} T_1^T & T_3^T \\ T_2^T & - \end{bmatrix}$
* 세 번째로, Caching된 $T_4$를 $A$에서 $B$로 Transpose하여 옮긴다. 이 경우 앞 단계에서 8개의 Spatial Locality Cache와 다르게, 4개의 Spatial Locality Cache만을 사용하여 Transpose한다.
  * $A = \begin{bmatrix} T_1 & T_2 \\ T_3 & T_4 \end{bmatrix}$ , $B = \begin{bmatrix} T_1^T & T_3^T \\ T_2^T & T_4^T \end{bmatrix}$

이 경우 $(8n + k)$ 형태의 Cache Set index를 두 세트 사용하게 되고, Cache 가용률을 32×32의 수준과 동일하게 맞출 수 있다. 이때, Cache miss rate는 동일한 비율로 끌어올려 약 **(288 * (64 * 64) / (32 * 32)) == 1152 Cache Miss** 가 발생할 수 있다. (마찬가지로 Cache에 처음 접근하는 32개를 뺀 경우 **1120 Cache Eviction**이 일어날 것이다.) 

#### Solution

```c
else if (M == 64)
    {
        for (R = 0; R < N; R += BLOCK_SIZE)
        {
            for (C = 0; C < M; C += BLOCK_SIZE)
            {
                /* SubBlockTranspose_SubQuad2 */
                for (rT = R; rT < R + BLOCK_SIZE / 2; rT++)
                {
                    T0 = A[rT][C + 0];
                    T1 = A[rT][C + 1];
                    T2 = A[rT][C + 2];
                    T3 = A[rT][C + 3];
                    T4 = A[rT][C + 4];
                    T5 = A[rT][C + 5];
                    T6 = A[rT][C + 6];
                    T7 = A[rT][C + 7];

                    B[C + 0][rT + 0] = T0;
                    B[C + 1][rT + 0] = T1;
                    B[C + 2][rT + 0] = T2;
                    B[C + 3][rT + 0] = T3;
                    B[C + 0][rT + 4] = T4;
                    B[C + 1][rT + 4] = T5;
                    B[C + 2][rT + 4] = T6;
                    B[C + 3][rT + 4] = T7;
                }

                /* SubBlockTranspose_SubQuad13 */
                for (cT = C; cT < C + BLOCK_SIZE / 2; cT++)
                {
                    T0 = A[R + SUBBLOCK_SIZE + 0][cT];
                    T1 = A[R + SUBBLOCK_SIZE + 1][cT];
                    T2 = A[R + SUBBLOCK_SIZE + 2][cT];
                    T3 = A[R + SUBBLOCK_SIZE + 3][cT];
                    T4 = B[cT][R + 4];
                    T5 = B[cT][R + 5];
                    T6 = B[cT][R + 6];
                    T7 = B[cT][R + 7];

                    B[cT][R + 4] = T0;
                    B[cT][R + 5] = T1;
                    B[cT][R + 6] = T2;
                    B[cT][R + 7] = T3;
                    B[cT + 4][R + 0] = T4;
                    B[cT + 4][R + 1] = T5;
                    B[cT + 4][R + 2] = T6;
                    B[cT + 4][R + 3] = T7;
                }

                /* SubBlockTranspose_SubQuad4 */
                for (rT = R + SUBBLOCK_SIZE; rT < R + BLOCK_SIZE; rT++)
                {
                    T4 = A[rT][C + 4];
                    T5 = A[rT][C + 5];
                    T6 = A[rT][C + 6];
                    T7 = A[rT][C + 7];

                    B[C + 4][rT] = T4;
                    B[C + 5][rT] = T5;
                    B[C + 6][rT] = T6;
                    B[C + 7][rT] = T7;
                }
            }
        }
    }
```

앞서 설명한 단계별로 다음을 진행한다. $M$을 나누었을 때 각각 1, 2, 3, 4분면 위치의 이름으로 Comment를 작성했다.

* `/* SubBlockTranspose_SubQuad2 */`
  * 앞서 서술한 첫 단계 ($T_1$을  Transpose, $T_2$를 옮기는 부분이다.)
* `/* SubBlockTranspose_SubQuad13 */`
  * 앞서 서술한 두 번째 단계 ($T_2$와 $T_3$을  Transpose하는 부분이다.)
* `/* SubBlockTranspose_SubQuad4 */`
  * 앞서 서술한 마지막 단계 ($T_4$을  Transpose하는 부분이다.)

수행 결과는 다음과 같았다.

![](images\64x64.png)

예상과 유사하게 총 **9065** Cache Hits, **1180** Cache Misses (Estimated Cache Misses + 38), **1148** Cache Eviction (Estimated Cache Eviction + 38)이 일어났다.

### 61×67

#### Analysis

앞서의 경우에서 2^n^ × 2^n^ 형태의 Matrix를 Transpose했던 것과 다르게, 뒤집힐 때 Cache Locality를 고려하기 어렵다. 따라서 경험과 Trial & Error에 의존하여 Matrix transpose를 구현하고자 했다.

이를 자동화하기 위해 `trans.c` 및 `cachelab.c`, `cachelab.h`와 `tracegen.c`를 임시로 수정해 Program argument로 해당 Block size를 넘기도록 수정하였다.

* 수정 부분은 다음과 같다. (**최종 제출용 tarball에는 이러한 변경사항의 적용 없이 최종 결과만을 적용했다.**)

  * `cachelab.h`

    ```diff
    typedef struct trans_func
    {
    -   void (*func_ptr)(int M, int N, int[N][M], int[M][N]);
    +   void (*func_ptr)(int M, int N, int[N][M], int[M][N], int R, int C);
    ...
    void registerTransFunction(
    -    void (*trans)(int M, int N, int[N][M], int[M][N]), char *desc);
    +    void (*trans)(int M, int N, int[N][M], int[M][N], int R, int C), char *desc);
    ```

  * `cachelab.c`

    ```diff
    - void registerTransFunction(void (*trans)(int M, int N, int[N][M], int[M][N]), char *desc)
    + void registerTransFunction(void (*trans)(int M, int N, int[N][M], int[M][N], int R, int C), char *desc)
    ```

  * `test-trans.c`

    ```diff
    /* Globals set on the command line */
    static int M = 0;
    static int N = 0;
    
    + /* Static Row block and Column block for test 61*67 */
    + static int ROW_BLK = 0;
    + static int COL_BLK = 0;
    
    ...
            if (strcmp(func_list[i].description, SUBMIT_DESCRIPTION) == 0)
                results.funcid = i; /* remember which function is the submission */
    
            printf("\nFunction %d (%d total)\nStep 1: Validating and generating memory traces\n", i, func_counter);
            /* Use valgrind to generate the trace */
    
    -        sprintf(cmd, "valgrind --tool=lackey --trace-mem=yes --log-fd=1 -v ./tracegen -M %d -N %d -F %d > trace.tmp", M, N, i);
    +        sprintf(cmd, "valgrind --tool=lackey --trace-mem=yes --log-fd=1 -v ./tracegen -M %d -N %d -F %d -R %d -C %d > trace.tmp", M, N, i, ROW_BLK, COL_BLK);
    ...
    - while ((c = getopt(argc, argv, "M:N:h")) != -1)
    + while ((c = getopt(argc, argv, "M:N:hR:C:")) != -1)
        {
            switch (c)
            {
            case 'M':
                M = atoi(optarg);
                break;
            case 'N':
                N = atoi(optarg);
                break;
            case 'h':
                usage(argv);
                exit(0);
    +       case 'R':
    +           ROW_BLK = atoi(optarg);
    +           break;
    +       case 'C':
    +          COL_BLK = atoi(optarg);
    +           break;
            default:
                usage(argv);
                exit(1);
            }
        }
    ```

  * `tracegen.c`

    ```diff
    static int M;
    static int N;
    + static int R;
    + static int C;
    
    - while ((c = getopt(argc, argv, "M:N:F:")) != -1)
    + while ((c = getopt(argc, argv, "M:N:F:R:C:")) != -1)
        {
            switch (c)
            {
            case 'M':
                M = atoi(optarg);
                break;
            case 'N':
                N = atoi(optarg);
                break;
            case 'F':
                selectedFunc = atoi(optarg);
                break;
    +       case 'R':
    +           R = atoi(optarg);
    +           break;
    +       case 'C':
    +           C = atoi(optarg);
    +           break;
            case '?':
            default:
                printf("./tracegen failed to parse its options.\n");
                exit(1);
            }
        }
    
    ...
    
    if (-1 == selectedFunc)
        {
            /* Invoke registered transpose functions */
            for (i = 0; i < func_counter; i++)
            {
                MARKER_START = 33;
    -           (*func_list[i].func_ptr)(M, N, A, B);
    +           (*func_list[i].func_ptr)(M, N, A, B, R, C);
                MARKER_END = 34;
                if (!validate(i, M, N, A, B))
                    return i + 1;
            }
        }
        else
        {
            MARKER_START = 33;
    -       (*func_list[selectedFunc].func_ptr)(M, N, A, B);
    +       (*func_list[selectedFunc].func_ptr)(M, N, A, B, R, C);
            MARKER_END = 34;
            if (!validate(selectedFunc, M, N, A, B))
                return selectedFunc + 1;
        }
    ```

  * `trans.c`

    ```diff
    - void transpose_submit(int M, int N, int A[N][M], int B[M][N])
    + void transpose_submit(int M, int N, int A[N][M], int B[M][N], int ROW_BLK, int COL_BLK)
    ```

    

이를 Python에서 자동화 하기 위해 `os.system`이나 `subprocess.run`을 사용하여 Program argument를 달리하여 결과를 관찰하려 했으나, Python이 같이 CPU를 사용하기 때문에 `./test-trans`를 직접 실행했을 떄와 결과가 달랐다. 이를 위해 Shell script를 작성하여 Local에서 실험하였다. 다음과 같은 Shell script를 작성했다. **(Block size의 각 Row와 Column은 2^N^  형태로 고정했다.)**

```shell
#!/bin/bash
rm log.txt
touch log.txt
for ((R=1; R<=64; R=R*2))
do
    for((C=1; C<=64; C=C*2))
    do
        echo "Testing ($R, $C) Block combination."
        echo -n "$R $C | " >> log.txt
        ./test-trans -M 61 -N 67 -R $R -C $C | sed -n "/misses:*[0-9]/p" >> log.txt
    done
done
```

결과를 Python을 통해 분석했을 때 다음과 같은 그래프를 얻을 수 있었다.

![](images\statics.png)

Row size에 영향을 받지 않고, Column size에만 영향을 받는 것을 확인할 수 있었다. 이 때 최적의 Column 개수는 16개가 (2^4^) 가장 적당함을 확인하였다. **따라서 원래 코드에서 각 가로와 세로를 모두 2^4^, 2^4^ 로 고정**하여 61×67 Matrix transpose를 완성하였다.

####  Solution

```c
else if (M == 61)
    {
        for (R = 0; R < N; R += 16)
        {
            for (C = 0; C < M; C += 16)
            {
                for (rT = C; rT < C + (((M - C) > 16) ? 16 : M - C); rT++)
                {
                    for (cT = R; cT < R + (((N - R) > 16) ? 16 : N - R); cT++)
                    {
                        B[rT][cT] = A[cT][rT];
                    }
                }
            }
        }
    }
```

![](images\61x67_16x16.png)

최종적으로 총 **6333** Cache Hits, **1848** Cache Misses, **1816** Cache Eviction이 일어났다.

## Final Result

![](images\result.png)
