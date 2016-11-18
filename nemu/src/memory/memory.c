#include "common.h"
#include "cache.h"

uint32_t dram_read(hwaddr_t, size_t);

void dram_write(hwaddr_t, size_t, uint32_t);

double cache_miss_time=0;

double cache_visit_time=0;


/* Calculate cache performance parameter */
double calculate_hit_rate()
{
	return (cache_visit_time-cache_miss_time)/cache_visit_time;
}

double calculate_visit_time()
{
	return cache_miss_time*200+cache_visit_time*2;
}

/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
	//return dram_read(addr, len) & (~0u >> ((4 - len) << 3));
	return cachel1_read(addr,len);
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
	//dram_write(addr, len, data);
	cachel1_write(addr,len,data);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	hwaddr_write(addr, len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	return lnaddr_read(addr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_write(addr, len, data);
}
