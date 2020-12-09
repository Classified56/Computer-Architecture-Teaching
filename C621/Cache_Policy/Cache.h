#ifndef __CACHE_H__
#define __CACHE_H__

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include <stdint.h>

#include "Cache_Blk.h"
#include "Request.h"

//#define LRU
//#define LFU
#define ARC

extern const unsigned cache_size;
extern const unsigned assoc;

typedef struct Node{
    uint64_t addr;
    struct Node *next;
}Node;

/* Cache */
typedef struct Set
{
    Cache_Block **ways; // Block ways within a set
    Node *L1;
    Node *L2;
    uint64_t p;
}Set;

typedef struct Cache
{
    uint64_t blk_mask;
    unsigned num_blocks;
    
    Cache_Block *blocks; // All cache blocks

    /* Set-Associative Information */
    unsigned num_sets; // Number of sets
    unsigned num_ways; // Number of ways within a set

    unsigned set_shift;
    unsigned set_mask; // To extract set index
    unsigned tag_shift; // To extract tag

    Set *sets; // All the sets of a cache
    
}Cache;

// Function Definitions
Cache *initCache();
bool accessBlock(Cache *cache, Request *req, uint64_t access_time);
bool insertBlock(Cache *cache, Request *req, uint64_t access_time, uint64_t *wb_addr);

// Helper Function
uint64_t blkAlign(uint64_t addr, uint64_t mask);
Cache_Block *findBlock(Cache *cache, uint64_t addr);

// Replacement Policies
bool lru(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr);
bool lfu(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr);
bool arc(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr);

int lengthNode(Node *head);
bool findNode(Node *head, uint64_t block);
void insertFirstNode(Node **head, uint64_t block);
Node* deleteLastNode(Node **head);
Node* deleteNode(Node **head, uint64_t block);
void insertNodeOrTop(Node **head, uint64_t block);

#endif
