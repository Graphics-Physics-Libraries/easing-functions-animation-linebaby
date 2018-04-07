#pragma once

#include <stddef.h>
#include <stdbool.h>

struct pool {
	size_t poolSize;
	size_t poolCount;
	size_t poolsUsed;
	void* pools;
	bool inUse[];
};

struct pool* pool_init(size_t poolSize, size_t poolCount);
void* pool_alloc(struct pool* p);
void pool_free(struct pool* p, void* data);
void pool_destroy(struct pool* p);
