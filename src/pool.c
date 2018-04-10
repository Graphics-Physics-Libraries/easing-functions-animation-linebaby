#include "pool.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct pool* pool_init(size_t poolSize, size_t poolCount) {
	assert(poolSize);
	assert(poolCount);
	assert(poolSize % 16 == 0); // ensure 16-byte alignment amongst elements
	assert(poolCount % 16 == 0); // ensure 16-byte alignment after the pool struct and inUse array
	
	void* a = malloc(sizeof(struct pool) + sizeof(bool)*poolCount + poolSize*poolCount);
	struct pool* p = (struct pool*)a;
	p->poolSize = poolSize;
	p->poolCount = poolCount;
	p->poolsUsed = 0;
	p->pools = a + sizeof(struct pool) + sizeof(bool)*poolCount;
	memset(p->inUse, 0, sizeof(bool)*poolCount);
	
	return p;
}

void* pool_alloc(struct pool* p) {
	assert(p);
	
	void* data = NULL;
	
	// TODO: Optimize the search to start where left off last time
	for(size_t i = 0; i < p->poolCount; i++) {
		if(!p->inUse[i]) {
			data = p->pools + (i * p->poolSize);
			p->inUse[i] = true;
			break;
		}
	}
	
	assert(data);
	p->poolsUsed++;
	
	return data;
}

void pool_free(struct pool* p, void* data) {
	assert(p);
	assert(data >= p->pools && data < p->pools + (p->poolSize*p->poolCount));
	size_t idx = (data - p->pools) / p->poolSize;
	p->inUse[idx] = false;
	p->poolsUsed--;
}

void pool_reset(struct pool* p) {
	assert(p);
	memset(p->inUse, 0, p->poolCount);
	p->poolsUsed = 0;
}

void pool_destroy(struct pool* p) {
	free(p);
}