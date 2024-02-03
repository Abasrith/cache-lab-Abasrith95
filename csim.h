/**
 * @file csim.h
 * @brief Header file for csim.c
 *
 *
 * @author Abhishek Basrithaya <abasrith@andrew.cmu.edu>
 */
#include "cachelab.h"
/* Defines */
#define NUM_ELEMENTS_IN_UNIT            5
#define ADDRESS_BITS_LEN                64
#define CACHE_INVALID_FLAG              0
#define CACHE_VALID_FLAG                1
#define EVICTION_RANK_VAL               1

/* Cache line structure members */
typedef struct {
    unsigned long iTag;                             /* Tag value */
    unsigned int bDirtyFlag;                        /* Dirty Flag */
    unsigned int bValidFlag;                        /* Valid Flag */
    unsigned int iRank;                             /* Rank value that keeps track of recent access to this memory */
    unsigned int dirtyEvictionCount;                /* Dirty eviction count */
} cacheLine_t;

/* Function prototyping */
void checkSimulatorCache(cacheLine_t *cacheAddr,unsigned int accFlag, unsigned long memAddr, csim_stats_t *stats);
void cacheMissHandler(cacheLine_t *cacheAddr, unsigned long addrSVal, unsigned long addrTagVal, unsigned int memAccessType, csim_stats_t *stats);
void cacheEvictionHandler(cacheLine_t *cacheAddr, unsigned long addrSVal, unsigned long addrTagVal, unsigned int memAccessType, csim_stats_t *stats);

