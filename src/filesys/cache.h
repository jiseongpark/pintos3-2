#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include <stdbool.h>
#include "threads/synch.h"

typedef struct buffer_cache_entry{
	bool dirty;                          /* Dirty bit flag */
	bool used;                           /* check whether the entry is used */
	disk_sector_t sec_num;               /* disk sector mapped to the entry */
	uint8_t buffer[DISK_SECTOR_SIZE];    /* sector contents */
	int64_t access_time;                 /* recently accessed time (used for LRU) */
}BCE;

struct semaphore cache_sema;
BCE buffer_cache[64];


void buffer_cache_init(void);
int buffer_cache_lru_eviction(void);
void buffer_cache_flush(int index);
void buffer_cache_flush_all(void);
int buffer_cache_find(disk_sector_t sector);
void buffer_cache_write(disk_sector_t sector, void *buffer);
void buffer_cache_read(disk_sector_t sector, void *buffer);
void periodic_flush_all(void);
void read_ahead(disk_sector_t sector);
void read_ahead_func(void);


#endif /* filesys/cache.h */