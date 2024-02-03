/**
 * @file csim.c
 * @brief Contains implementation of a cache simulator
 *
 * This implementation is only simulating a cache behaviour by addressing
 * hits, misses and evictions. Although memory is allocated for cache metadata
 * no memory is allocated for corresponding data in the cache blocks. Simulator
 * is merely checking for presence of address requested for and takes action
 * accordingly
 *
 * Tracefile is read line by line and corresponding hits, misses, evictions,
 * number of dirty bytes in the cache and number of dirty evictions are tracked.
 *
 * @author Abhishek Basrithaya <abasrith@andrew.cmu.edu>
 */

/* Importing header files */
#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Defines */
#define NUM_ELEMENTS_IN_UNIT 5
#define ADDRESS_BITS_LEN 64
#define CACHE_INVALID_FLAG 0
#define CACHE_VALID_FLAG 1
#define EVICTION_RANK_VAL 1

/* Cache line structure members */
typedef struct {
    unsigned long iTag;      /* Tag value */
    unsigned int bDirtyFlag; /* Dirty Flag */
    unsigned int bValidFlag; /* Valid Flag */
    unsigned int iRank; /* Rank value that keeps track of recent access to this
                           cache line */
    unsigned int dirtyEvictionCount; /* Dirty eviction count */
} cacheLine_t;

/* Function prototyping */
void checkSimulatorCache(cacheLine_t *cacheAddr, unsigned int accFlag,
                         unsigned long memAddr, csim_stats_t *stats);
void cacheMissHandler(cacheLine_t *cacheAddr, unsigned long addrSVal,
                      unsigned long addrTagVal, unsigned int memAccessType,
                      csim_stats_t *stats);
void cacheEvictionHandler(cacheLine_t *cacheAddr, unsigned long addrSVal,
                          unsigned long addrTagVal, unsigned int memAccessType,
                          csim_stats_t *stats);
void cacheLineRankUpdate(cacheLine_t *cacheAddr, unsigned long addrSVal,
                         unsigned int recentAccessIndex);
void printHelpVerbose(void);

/* Global variables */
unsigned int iSetBitCount = 0;
unsigned int iSetCount = 0;
unsigned int iCacheLinesPerSet = 0;
unsigned int iBlockBitCount = 0;
unsigned int iBlocksPerLine = 0;
unsigned int iRecentAccessRankValue = 0;
bool bVerbose = false;

int main(int argc, char **argv) {
    /* Trace file pointer */
    FILE *inputTraceFile = NULL;

    /* Trace file specifics */
    char memAccessType;
    unsigned long memAddress;
    int byteSize;

    /* Simulator cache pointer and statistics*/
    cacheLine_t *cacheImage = NULL;
    csim_stats_t inputTraceStats;
    memset((void *)&inputTraceStats, 0, sizeof(inputTraceStats));

    int options = 0;
    bool bHelpevoked = false;
    unsigned int iMemAccessTypeFlag = 0;
    unsigned int dirtyCacheLineCount = 0;
    unsigned int dirtyEvictedCacheLineCount = 0;
    int iSignedSetBitCount = -1;
    int iSignedCacheLinesPerSet = -1;
    int iSignedBlockBitCount = -1;

    /* Trace file parsing */
    while ((options = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (options) {
        case 'h':
            bHelpevoked = true;
            printHelpVerbose();
            break;
        case 'v':
            // verbose
            bVerbose = true;
            break;
        case 's':
            iSignedSetBitCount = atoi(optarg);
            iSetBitCount = (unsigned int)iSignedSetBitCount;
            iSetCount = 1 << iSetBitCount;
            break;
        case 'E':
            iSignedCacheLinesPerSet = atoi(optarg);
            iCacheLinesPerSet = (unsigned int)iSignedCacheLinesPerSet;
            iRecentAccessRankValue = iCacheLinesPerSet;
            break;
        case 'b':
            iSignedBlockBitCount = atoi(optarg);
            iBlockBitCount = (unsigned int)iSignedBlockBitCount;
            iBlocksPerLine = 1 << iBlockBitCount;
            break;
        case 't':
            inputTraceFile = fopen(optarg, "r");
            break;
        default:
            printf("Invalid command flag, type -h for valid command flags "
                   "list\n");
            break;
        }
    }
    /* General error checks */
    if ((iSetBitCount < 0) || (iSignedCacheLinesPerSet < 1) ||
        (inputTraceFile == NULL) || (iBlockBitCount < 0) ||
        (iSignedSetBitCount < 0) || (iSignedBlockBitCount < 0)) {
        if (!bHelpevoked)
            printf("Invalid cache parameters encountered!\nProgram "
                   "Terminating...\n");
        return 1; /* Invalid condition evoked */
    }
    /* Check cache for an input line of the trace file */
    /* Initialise simulator cache by allocating memory in heap */
    cacheImage = calloc((iSetCount * iCacheLinesPerSet), sizeof(cacheLine_t));
    if (cacheImage != NULL) {
        while (fscanf(inputTraceFile, "%c %lx,%d\n", &memAccessType,
                      &memAddress, &byteSize) > 0) {
            if (memAccessType == 'L') /* Load memory address */
            {
                iMemAccessTypeFlag = 0;
            } else if (memAccessType == 'S') /* Store memory address */
            {
                iMemAccessTypeFlag = 1;
            }
            if (bVerbose)
                printf("%c %lx,%d", memAccessType, memAddress, byteSize);
            /* Checking simulator cache for memory hits and misses */
            checkSimulatorCache(cacheImage, iMemAccessTypeFlag, memAddress,
                                &inputTraceStats);
            if (bVerbose)
                printf("\n");
        }
    } else /* return if calloc retuned a NULL, unable to allocate space for
                  simulator cache */
    {
        printf("Heap allocation for cache simulator failed!\n");
        return 1;
    }
    for (unsigned int i = 0; i < iSetCount; i++) {
        for (unsigned int j = 0; j < iCacheLinesPerSet; j++) {
            /* compute number of dirty cache lines */
            if (cacheImage[(i * iCacheLinesPerSet) + j].bDirtyFlag == 1) {
                dirtyCacheLineCount++;
            }
            /* compute number of dirty eviction from the cache */
            dirtyEvictedCacheLineCount +=
                cacheImage[(i * iCacheLinesPerSet) + j].dirtyEvictionCount;
        }
    }
    /* Update dirty bytes in the cache and dirty cache evictions */
    inputTraceStats.dirty_bytes = dirtyCacheLineCount * iBlocksPerLine;
    inputTraceStats.dirty_evictions =
        dirtyEvictedCacheLineCount * iBlocksPerLine;

    /* free allocated cache space and closing trace file */
    free(cacheImage);
    fclose(inputTraceFile);
    /* submitting final summary for the trace file */
    printSummary(&inputTraceStats);
    return 0;
}

/**
 * @brief Checks simulator cache of hits, misses and evictions.
 *
 *
 * @param[in]       cacheLine_t *cacheAddr          Pointer to the cache
 * simulator
 * @param[in]       unsigned int memAccessType      Type of memory access
 * requested
 * @param[in]       unsigned long memAddr           Memory address to be
 * accessed
 * @param[in,out]   csim_stats_t *stats             Cache statistics
 *
 * @return void.
 */
void checkSimulatorCache(cacheLine_t *cacheAddr, unsigned int memAccessType,
                         unsigned long memAddr, csim_stats_t *stats) {

    /* Local variables */
    unsigned long addrSVal = 0;
    unsigned long addrTagVal = 0;
    unsigned long setMask = 0;
    unsigned long tagMask = 0;
    bool hitFlag = false;

    /* Compute cache set to access */
    for (unsigned int i = 0; i < iSetBitCount; i++) {
        setMask = (setMask << 1) + 1;
    }
    addrSVal = (memAddr >> iBlockBitCount) & setMask;

    /* Compute Tag corresponding to the memory address */
    for (unsigned int i = 0;
         i < (ADDRESS_BITS_LEN - (iSetBitCount + iBlockBitCount)); i++) {
        tagMask = (tagMask << 1) + 1;
    }
    addrTagVal = (memAddr >> (iBlockBitCount + iSetBitCount)) & tagMask;

    /* Cache handler section */
    for (unsigned int j = 0; j < iCacheLinesPerSet; j++) {
        /* Check if address tag matches with cache line tag and update rank for
         the cache line, if not call the miss routine */
        if ((cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iTag ==
             addrTagVal) &&
            (cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bValidFlag !=
             CACHE_INVALID_FLAG)) {
            stats->hits++;
            hitFlag = true;
            /* Updating cache line rank upon access*/
            if (cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iRank !=
                iRecentAccessRankValue) {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iRank =
                    iRecentAccessRankValue;
                cacheLineRankUpdate(cacheAddr, addrSVal, j);
            }
            /* Updating dirty memory access */
            if (memAccessType == 1) {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag = 1;
            }
            if (bVerbose) {
                printf("\thit");
            }
            break;
        }
    }
    if (!hitFlag)
        cacheMissHandler(cacheAddr, addrSVal, addrTagVal, memAccessType, stats);
}

/**
 * @brief Cache misses are handled in this function.
 *
 *
 * @param[in]       cacheLine_t *cacheAddr          Pointer to the cache
 * simulator
 * @param[in]       unsigned long addrSVal          Cache set to be accessed
 * @param[in]       unsigned long addrTagVal        Tag value of the memory
 * address to be accessed
 * @param[in]       unsigned int memAccessType      Memory access type
 * @param[in,out]   csim_stats_t *stats             Cache statistics
 *
 * @return void.
 */
void cacheMissHandler(cacheLine_t *cacheAddr, unsigned long addrSVal,
                      unsigned long addrTagVal, unsigned int memAccessType,
                      csim_stats_t *stats) {
    /* local variable */
    bool evictFlag = false;

    /* Cache miss handler */
    stats->misses++;
    if (bVerbose)
        printf("\tmiss");

    for (unsigned int j = 0; j < iCacheLinesPerSet; j++) {
        /* Check if cache lines are empty and update the tag and rank for the
         cache line, if not call the eviction routine */
        if (cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bValidFlag ==
            CACHE_INVALID_FLAG) {
            cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bValidFlag =
                CACHE_VALID_FLAG;
            cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iTag = addrTagVal;
            cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iRank =
                iRecentAccessRankValue;
            /* Updating dirty memory access */
            if (memAccessType == 1) {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag = 1;
            } else {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag = 0;
            }
            /* Updating cache line rank upon access*/
            cacheLineRankUpdate(cacheAddr, addrSVal, j);
            evictFlag = false;
            break;
        } else {
            evictFlag = true;
        }
    }
    if (evictFlag)
        cacheEvictionHandler(cacheAddr, addrSVal, addrTagVal, memAccessType,
                             stats);
}

/**
 * @brief Cache evictions are handled in this function.
 *
 *
 * @param[in]       cacheLine_t *cacheAddr          Pointer to the cache
 * simulator
 * @param[in]       unsigned long addrSVal          Cache set to be accessed
 * @param[in]       unsigned long addrTagVal        Tag value of the memory
 * address to be accessed
 * @param[in]       unsigned int memAccessType      Memory access type
 * @param[in,out]   csim_stats_t *stats             Cache statistics
 *
 * @return void.
 */
void cacheEvictionHandler(cacheLine_t *cacheAddr, unsigned long addrSVal,
                          unsigned long addrTagVal, unsigned int memAccessType,
                          csim_stats_t *stats) {
    /* Cache eviction handler */
    stats->evictions++;
    if (bVerbose)
        printf("\teviction");
    for (unsigned int j = 0; j < iCacheLinesPerSet; j++) {
        /* If cache line rank == "EVICTION_RANK_VAL" then evict line
         and update tag and rank for cache line */
        if (cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iRank ==
            EVICTION_RANK_VAL) {
            cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iTag = addrTagVal;
            cacheAddr[(addrSVal * iCacheLinesPerSet) + j].iRank =
                iRecentAccessRankValue;
            /* If eviction was dirty then update dirty eviction count */
            if (cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag == 1) {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j]
                    .dirtyEvictionCount++;
            }
            /* Updating dirty memory access flag */
            if (memAccessType == 0) {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag = 0;
            } else {
                cacheAddr[(addrSVal * iCacheLinesPerSet) + j].bDirtyFlag = 1;
            }
            /* Updating cache line rank upon access */
            cacheLineRankUpdate(cacheAddr, addrSVal, j);
            break;
        }
    }
}

/**
 * @brief Cache line rank updates are handled in this function.
 *
 *
 * @param[in]       cacheLine_t *cacheAddr              Pointer to the cache
 * simulator
 * @param[in]       unsigned long addrSVal              Cache set to be accessed
 * @param[in]       unsigned int recentAccessIndex      Most recently accessed
 * cache line index
 *
 * @return void.
 */
void cacheLineRankUpdate(cacheLine_t *cacheAddr, unsigned long addrSVal,
                         unsigned int recentAccessIndex) {
    for (unsigned int i = 0; i < iCacheLinesPerSet; i++) {
        if ((i != recentAccessIndex) &&
            (cacheAddr[(addrSVal * iCacheLinesPerSet) + i].bValidFlag ==
             CACHE_VALID_FLAG)) {
            cacheAddr[(addrSVal * iCacheLinesPerSet) + i].iRank =
                cacheAddr[(addrSVal * iCacheLinesPerSet) + i].iRank - 1;
        }
    }
}
/**
 * @brief Print verbose for help.
 *
 * @return void.
 */
void printHelpVerbose(void) {
    printf("Valid command flags:-\n-h -> help\n-v -> print verbose\n"
           "-s -> number of set bits\n"
           "-E -> number of lines per cache set\n"
           "-b -> number of block bits per cache line\n"
           "-t -> input trace file\n");
}
