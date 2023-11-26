#include "cachelab.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

__uint8_t VERBOSE = 0U;
__uint32_t E, S, B;
__uint32_t HIT_COUNT, MISS_COUNT, EVICT_COUNT;
int CacheAssignedTick = 0;

char *Trace;

typedef struct CacheStructure
{
    int valid;
    int clock;
    __uint64_t tag;
} CacheStructure;

CacheStructure **Cache;

CacheStructure **CacheCreate(__uint32_t, __uint32_t);
void CacheRun(char *);
void CacheAccess(__uint64_t);
void CacheFree(CacheStructure **, __uint32_t);

void printHelp(const char **);

int main(int argc, const char **argv, const char **envp)
{
    char arg;
    while (1)
    {
        arg = getopt(argc, (char *const *)argv, "vhs:E:b:t:");
        if (arg == -1)
        {
            break;
        }
        switch (arg)
        {
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            B = atoi(optarg);
            break;
        case 'h':
            printHelp(argv);
            return 0;
        case 's':
            S = atoi(optarg);
            break;
        case 't':
            Trace = optarg;
            break;
        case 'v':
            VERBOSE = 1U;
            break;
        default:
            printHelp(argv);
            return 0;
        }
    }

    if (!S || !E || !B || !Trace)
    {
        printf("%s: Missing required command line arguments\n", *argv);
        printHelp(argv);
        return 0;
    }

    Cache = CacheCreate(1 << S, 1 << E);
    CacheRun(Trace);
    CacheFree(Cache, 1 << S);

    printSummary(HIT_COUNT, MISS_COUNT, EVICT_COUNT);
    return 0;
}

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

void CacheRun(char *TraceFileName)
{
    char Buffer[0x400];

    FILE *TraceFile = fopen(TraceFileName, "r");
    assert(TraceFile != NULL);

    long long unsigned int Address;
    __uint32_t Length;

    while (fgets(Buffer, 0x400, TraceFile))
    {
        if (Buffer[1] == 'S' || Buffer[1] == 'L' || Buffer[1] == 'M')
        {
            sscanf(&Buffer[3], "%llx,%u", &Address, &Length);

            if (VERBOSE)
            {
                printf("%c %llx,%u ", Buffer[1], Address, Length);
            }

            CacheAccess(Address);

            if (Buffer[1] == 'M')
            {
                CacheAccess(Address);
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

void CacheFree(CacheStructure **CacheArray, __uint32_t Set)
{
    for (size_t set_id = 0U; set_id < Set; set_id++)
    {
        free(CacheArray[set_id]);
    }
    free(CacheArray);
    return;
}

void printHelp(const char **argv)
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", *argv);
    printf("Options:\n");
    printf("    -h          Print this help message.\n");
    printf("    -v          Turn on verbosity flag.\n");
    printf("    -s <num>    # of Set index bits.\n");
    printf("    -E <num>    # of Lines per set.\n");
    printf("    -b <num>    # of Block offset bits\n");
    printf("    -t <file>   Path to Trace file\n");
    return;
}