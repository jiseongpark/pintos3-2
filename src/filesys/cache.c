#include "filesys/cache.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include <string.h>
#include <debug.h>
#include <stdlib.h>

#define FLUSH_PERIOD 10

void * read_ahead_buffer;
extern struct thread* read_ahead_thread;
disk_sector_t read_ahead_sec = 0;

int read_ahead_flag = 1;

void buffer_cache_init(void)
{
	sema_init(&cache_sema, 1);

	sema_down(&cache_sema);
	read_ahead_buffer = calloc(1, 512);
	

	int i = 0;
	for(; i < 64; i++){
		buffer_cache[i].used = false;
		buffer_cache[i].dirty = false;
		buffer_cache[i].access_time = 0;
	}

	thread_create("read_ahead", PRI_DEFAULT, read_ahead_func, NULL);
	sema_up(&cache_sema);
}

int buffer_cache_lru_eviction(void)
{
	int i=0, lru = 0;
	for(; i<64; i++)
	{
		if(buffer_cache[i].used == false)
			return i;
	}
	// printf("CHILD NUM : %d\n", thread_current()->child_num);
	for(i=0; i<64; i++)
	{
	 // if(thread_current()->child_num == 1){	
		// if(buffer_cache[lru].access_time > buffer_cache[i].access_time){
		// 	lru = i;
		// 	continue;
		// }
	 //  }
	 	if(buffer_cache[lru].access_time > buffer_cache[i].access_time){
			lru = i;
		}
	   
		// if(buffer_cache[lru].access_time < buffer_cache[i].access_time && !buffer_cache[i].dirty){
		// 	lru = i;
		// }
	}
	if(buffer_cache[lru].dirty)
		buffer_cache_flush(lru);

	return lru;
}

void buffer_cache_flush(int index)
{
	ASSERT(index >= 0 && index <= 63);
	ASSERT(buffer_cache[index].used == true);

	if(buffer_cache[index].dirty == true)
	{
		disk_write(disk_get(0,1), buffer_cache[index].sec_num, buffer_cache[index].buffer);
		buffer_cache[index].dirty = false;
	}
}

void buffer_cache_flush_all(void)
{
	int i=0;
	sema_down(&cache_sema);
	for (; i<64; i++)
	{
		if(buffer_cache[i].dirty)
			buffer_cache_flush(i);
	}
	sema_up(&cache_sema);
}

int buffer_cache_find(disk_sector_t sector)
{
	int i=0;
	for (; i<64; i++)
	{	

		if(buffer_cache[i].sec_num == sector && buffer_cache[i].used)
			return i;
	}
	// read_ahead(sector);
	return -1;
}

void buffer_cache_write(disk_sector_t sector, void *buffer)
{
	
	sema_down(&cache_sema);
	int buffer_cache_idx = buffer_cache_find(sector);
	
	if(buffer_cache_idx == -1)
	{
		int evict_idx = buffer_cache_lru_eviction();

		
		
		buffer_cache[evict_idx].used = true;
		buffer_cache[evict_idx].dirty = false;
		buffer_cache[evict_idx].sec_num = sector;
		disk_read(disk_get(0,1), sector, buffer_cache[evict_idx].buffer);
		buffer_cache_idx = evict_idx;
		
		// sema_up(&cache_sema);
	}

	// sema_down(&cache_sema);

	buffer_cache[buffer_cache_idx].dirty = true;
	buffer_cache[buffer_cache_idx].access_time = timer_ticks();
	memmove(buffer_cache[buffer_cache_idx].buffer, buffer, DISK_SECTOR_SIZE);
	sema_up(&cache_sema);
}

void buffer_cache_read(disk_sector_t sector, void *buffer)
{
	
	sema_down(&cache_sema);
	int buffer_cache_idx = buffer_cache_find(sector);
	
	if(buffer_cache_idx == -1)
	{
		int evict_idx = buffer_cache_lru_eviction();

		

		buffer_cache[evict_idx].used = true;
		buffer_cache[evict_idx].dirty = false;
		buffer_cache[evict_idx].sec_num = sector;
		disk_read(disk_get(0,1), sector, buffer_cache[evict_idx].buffer);
		buffer_cache_idx = evict_idx;

		// sema_up(&cache_sema);
	}

	// sema_down(&cache_sema);
	// buffer_cache[buffer_cache_idx].access_time = timer_ticks();
	memmove(buffer, buffer_cache[buffer_cache_idx].buffer, DISK_SECTOR_SIZE);
	
	sema_up(&cache_sema);

	if(strcmp(thread_current()->name, "child-qsort"))
		read_ahead(sector);

	
}

void periodic_flush_all(void)
{
	
	while(true){
		buffer_cache_flush_all();
		timer_sleep(FLUSH_PERIOD);
	}
}

void read_ahead(disk_sector_t sector)
{	
	read_ahead_sec = sector + 1;
	// printf("THREAD CREATE\n");
	// sema_down(&cache_sema);
	disk_read(disk_get(0,1), sector, read_ahead_buffer);
	read_ahead_flag = 2;
	// printf("나와라 씨발새끼야 %p\n", read_ahead_thread);
	// printf("magic : 0xcd6abf4b, tmagic : %p\n", read_ahead_thread->magic);
	// printf("%s\n", read_ahead_thread->magic == 0xcd6abf4b ? "맞아 씨발" : "ㄴㄴ");
	// printf("TID : %s\n", read_ahead_thread->status);
	
	// free(read_ahead_buffer);
	// sema_up(&cache_sema);
	// printf("THREAD PASS\n");
}

void read_ahead_func(void)
{
	// sema_down(&cache_sema);
	while(true){

		while(read_ahead_flag = 1)
		{
			thread_yield();
		}

		disk_sector_t sector = read_ahead_sec;
		int buffer_idx = buffer_cache_find(sector);
		// timer_sleep(1000);
		if(buffer_idx == -1){
			buffer_idx = buffer_cache_lru_eviction();
			buffer_cache[buffer_idx].used = true;
			buffer_cache[buffer_idx].dirty = false;
			buffer_cache[buffer_idx].sec_num = sector;
			memmove(buffer_cache[buffer_idx].buffer, read_ahead_buffer, DISK_SECTOR_SIZE);
		}
		read_ahead_flag = 1;
	}
	// sema_up(&cache_sema);
	// timer_sleep(10);
	// thread_block();	
	// thread_exit();
}