#include "Cache.h"

/* Constants */
const unsigned block_size = 16; // Size of a cache line (in Bytes)
// TODO, you should try different size of cache, for example, 128KB, 256KB, 512KB, 1MB, 2MB
const unsigned cache_size = 2048; // Size of a cache (in KB)
// TODO, you should try different association configurations, for example 4, 8, 16
const unsigned assoc = 16;

Cache *initCache()
{
	Cache *cache = (Cache *)malloc(sizeof(Cache));

	cache->blk_mask = block_size - 1;

	unsigned num_blocks = cache_size * 1024 / block_size;
	cache->num_blocks = num_blocks;
//    printf("Num of blocks: %u\n", cache->num_blocks);

	// Initialize all cache blocks
	cache->blocks = (Cache_Block *)malloc(num_blocks * sizeof(Cache_Block));
	
	int i;
	for (i = 0; i < num_blocks; i++)
	{
		cache->blocks[i].tag = UINTMAX_MAX; 
		cache->blocks[i].valid = false;
		cache->blocks[i].dirty = false;
		cache->blocks[i].when_touched = 0;
		cache->blocks[i].frequency = 0;
	}

	// Initialize Set-way variables
	unsigned num_sets = cache_size * 1024 / (block_size * assoc);
	cache->num_sets = num_sets;
	cache->num_ways = assoc;
//    printf("Num of sets: %u\n", cache->num_sets);

	unsigned set_shift = log2(block_size);
	cache->set_shift = set_shift;
//    printf("Set shift: %u\n", cache->set_shift);

	unsigned set_mask = num_sets - 1;
	cache->set_mask = set_mask;
//    printf("Set mask: %u\n", cache->set_mask);

	unsigned tag_shift = set_shift + log2(num_sets);
	cache->tag_shift = tag_shift;
//    printf("Tag shift: %u\n", cache->tag_shift);

	// Initialize Sets
	cache->sets = (Set *)malloc(num_sets * sizeof(Set));
	for (i = 0; i < num_sets; i++)
	{
		cache->sets[i].ways = (Cache_Block **)malloc(assoc * sizeof(Cache_Block *));
	}

	// Combine sets and blocks
	for (i = 0; i < num_blocks; i++)
	{
		Cache_Block *blk = &(cache->blocks[i]);
		
		uint32_t set = i / assoc;
		uint32_t way = i % assoc;

		blk->set = set;
		blk->way = way;

		cache->sets[set].ways[way] = blk;
		cache->sets[set].L1 = NULL;
		cache->sets[set].L2 = NULL;
	}

	return cache;
}

bool accessBlock(Cache *cache, Request *req, uint64_t access_time)
{
	bool hit = false;

	uint64_t blk_aligned_addr = blkAlign(req->load_or_store_addr, cache->blk_mask);

	Cache_Block *blk = findBlock(cache, blk_aligned_addr);
   
	if (blk != NULL) 
	{
		hit = true;

		// Update access time   
		blk->when_touched = access_time;
		// Increment frequency counter
		++blk->frequency;

		if (req->req_type == STORE)
		{
			blk->dirty = true;
		}
	}

	return hit;
}

bool insertBlock(Cache *cache, Request *req, uint64_t access_time, uint64_t *wb_addr)
{
	// Step one, find a victim block
	uint64_t blk_aligned_addr = blkAlign(req->load_or_store_addr, cache->blk_mask);

	Cache_Block *victim = NULL;
	#ifdef LRU
		bool wb_required = lru(cache, blk_aligned_addr, &victim, wb_addr);
	#endif
	#ifdef LFU
		bool wb_required = lfu(cache, blk_aligned_addr, &victim, wb_addr);
	#endif
	#ifdef ARC
		bool wb_required = arc(cache, blk_aligned_addr, &victim, wb_addr);
	#endif
	assert(victim != NULL);

	// Step two, insert the new block
	uint64_t tag = req->load_or_store_addr >> cache->tag_shift;
	victim->tag = tag;
	victim->valid = true;

	victim->when_touched = access_time;
	++victim->frequency;

	if (req->req_type == STORE)
	{
		victim->dirty = true;
	}

	return wb_required;
//    printf("Inserted: %"PRIu64"\n", req->load_or_store_addr);
}

// Helper Functions	
inline uint64_t blkAlign(uint64_t addr, uint64_t mask)
{
	return addr & ~mask;
}

Cache_Block *findBlock(Cache *cache, uint64_t addr)
{
//    printf("Addr: %"PRIu64"\n", addr);

	// Extract tag
	uint64_t tag = addr >> cache->tag_shift;
//    printf("Tag: %"PRIu64"\n", tag);

	// Extract set index
	uint64_t set_idx = (addr >> cache->set_shift) & cache->set_mask;
//    printf("Set: %"PRIu64"\n", set_idx);

	Cache_Block **ways = cache->sets[set_idx].ways;
	int i;
	for (i = 0; i < cache->num_ways; i++)
	{
		if (tag == ways[i]->tag && ways[i]->valid == true)
		{
			deleteNode(&(cache->sets[set_idx].L1), addr);
			insertNodeOrTop(&(cache->sets[set_idx].L2), addr);
			return ways[i];
		}
	}

	return NULL;
}

bool lru(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr)
{
	uint64_t set_idx = (addr >> cache->set_shift) & cache->set_mask;
	//    printf("Set: %"PRIu64"\n", set_idx);
	Cache_Block **ways = cache->sets[set_idx].ways;

	// Step one, try to find an invalid block.
	int i;
	for (i = 0; i < cache->num_ways; i++)
	{
		if (ways[i]->valid == false)
		{
			*victim_blk = ways[i];
			return false; // No need to write-back
		}
	}

	// Step two, if there is no invalid block. Locate the LRU block
	Cache_Block *victim = ways[0];
	for (i = 1; i < cache->num_ways; i++)
	{
		if (ways[i]->when_touched < victim->when_touched)
		{
			victim = ways[i];
		}
	}

	// Step three, need to write-back the victim block
	*wb_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    uint64_t ori_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    printf("Evicted: %"PRIu64"\n", ori_addr);

	// Step three, invalidate victim
	victim->tag = UINTMAX_MAX;
	victim->valid = false;
	victim->dirty = false;
	victim->frequency = 0;
	victim->when_touched = 0;

	*victim_blk = victim;

	return true; // Need to write-back
}

bool lfu(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr)
{
	uint64_t set_idx = (addr >> cache->set_shift) & cache->set_mask;
	//    printf("Set: %"PRIu64"\n", set_idx);
	Cache_Block **ways = cache->sets[set_idx].ways;

	// Step one, try to find an invalid block.
	int i;
	for (i = 0; i < cache->num_ways; i++)
	{
		if (ways[i]->valid == false)
		{
			*victim_blk = ways[i];
			return false; // No need to write-back
		}
	}

	// Step two, if there is no invalid block. Locate the LRU block
	Cache_Block *victim = ways[0];
	for (i = 1; i < cache->num_ways; i++)
	{
		if (ways[i]->frequency < victim->frequency)
		{

			victim = ways[i];
		}
	}

	// Step three, need to write-back the victim block
	*wb_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    uint64_t ori_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    printf("Evicted: %"PRIu64"\n", ori_addr);

	// Step three, invalidate victim
	victim->tag = UINTMAX_MAX;
	victim->valid = false;
	victim->dirty = false;
	victim->frequency = 0;
	victim->when_touched = 0;

	*victim_blk = victim;

	return true; // Need to write-back
}

bool arc(Cache *cache, uint64_t addr, Cache_Block **victim_blk, uint64_t *wb_addr)
{
	uint64_t set_idx = (addr >> cache->set_shift) & cache->set_mask;
	//    printf("Set: %"PRIu64"\n", set_idx);
	Cache_Block **ways = cache->sets[set_idx].ways;

	insertFirstNode(&(cache->sets[set_idx].L1), addr);
	// Step one, try to find an invalid block.
	int i;
	for (i = 0; i < cache->num_ways; i++)
	{
		if (ways[i]->valid == false)
		{
			*victim_blk = ways[i];
			return false; // No need to write-back
		}
	}

	Node *removed = NULL;
	if(lengthNode(cache->sets[set_idx].L1) == cache->num_ways) {
		removed = deleteLastNode(&(cache->sets[set_idx].L1));
	} else if(lengthNode(cache->sets[set_idx].L1) + lengthNode(cache->sets[set_idx].L2) == 2 * cache->num_ways) {
		removed = deleteLastNode(&(cache->sets[set_idx].L2));
	}

	// Step two, if there is no invalid block. Locate the LRU block
	Cache_Block *victim = ways[0];
	for (i = 1; i < cache->num_ways; i++)
	{
		if (removed != NULL && ways[i]->tag == (removed->addr >> cache->tag_shift)) {
			victim = ways[i];
			break;
		} else if (ways[i]->when_touched < victim->when_touched) {
			victim = ways[i];
		}
	}

	// Step three, need to write-back the victim block
	*wb_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    uint64_t ori_addr = (victim->tag << cache->tag_shift) | (victim->set << cache->set_shift);
//    printf("Evicted: %"PRIu64"\n", ori_addr);

	// Step three, invalidate victim
	victim->tag = UINTMAX_MAX;
	victim->valid = false;
	victim->dirty = false;
	victim->frequency = 0;
	victim->when_touched = 0;

	*victim_blk = victim;

	return true; // Need to write-back
}

int lengthNode(Node *head) {
	int length = 0;
	Node *current;
	
	for(current = head; current != NULL; current = current->next) {
		length++;
	}
	
	return length;
}  

bool findNode(Node *head, uint64_t block) {
	Node* current = head;

	if(current == NULL) {
		return NULL;
	}

 	while(current->addr != block) {
	 	if(current->next == NULL) {
			return NULL;
	  	} else {
			current = current->next;
	  	}
   	}      
	return current;
} 

void insertFirstNode(Node **head, uint64_t block) {
   	Node *link = (Node*) malloc(sizeof(Node));
	
   	link->addr = block;
   	link->next = NULL;
	
	if(*head != NULL) {
	   	link->next = *head;
	}

   	*head = link;
}

Node* deleteLastNode(Node **head) {
	Node *current;
	Node *previous = NULL;
	
   	for(current = *head; current != NULL && current->next != NULL; current = current->next) {
		previous = current;
   	}
   	if(previous != NULL){
	   	previous->next = NULL;
	} else {
		*head = NULL;
	}
    return current;
}

Node* deleteNode(Node **head, uint64_t block) {
	Node *current;
	Node *previous = NULL;
	
   	for(current = *head; current != NULL && current->addr != block; current = current->next) {
		previous = current;
   	}
   	if(current == NULL) {
   		return NULL;
   	}
   	if(previous != NULL){
	   	previous->next = current->next;
	} else {
		*head = NULL;
	}
    return current;
}

void insertNodeOrTop(Node **head, uint64_t block) {
	Node *current = *head;
	Node *previous = NULL;

	if(current == NULL) {
		insertFirstNode(head, block);
		return;
	}

 	while(current->addr != block) {
	 	if(current->next == NULL) {
	 		insertFirstNode(head, block);
			return;
	  	} else {
	  		previous = current;
			current = current->next;
	  	}
   	}      
   	if(previous != NULL){
   		previous->next = current->next;
   		current->next = *head;
   	}
   	*head = current;
} 