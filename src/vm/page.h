#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <list.h>
#include <hash.h>
#include <stddef.h>
#include "threads/init.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "filesys/off_t.h"

typedef struct page_table_entry
{
	uint32_t *uaddr;          /* virtual address of the page */
	uint32_t *paddr;          /* physical address of the page */

	int usertid;              /* process tid that use this frame */
	bool is_swapped_out;      /* flag for swapped out */
	bool dirty;               /* dirty bit flag UNUSED */
	bool writable;            /* writable flag */
	bool load;				  /* for load segment page UNUSED*/

	/* Lazy Loading */
	struct file *file;        /* loading file */
	off_t ofs;                /* file position offset */
	size_t page_read_bytes;   /* read bytes */
	size_t page_zero_bytes;   /* zero bytes */
	bool load_result;         /* actually loaded */

	struct hash_elem helem;   /* hash element */
	struct list_elem elem;    /* list element */
} PTE;

// struct hash page_table;       /* page table */

struct semaphore page_sema;   /* page semaphore */


void page_init(void);
void page_clear_all(void);
bool lazy_load(uint32_t *upage, struct file *file, off_t ofs, size_t page_read_bytes, size_t page_zero_bytes, bool writable);
bool actual_load(uint32_t *upage);
void page_map(uint32_t *upage, uint32_t *kpage, bool writable);
void page_remove_pte(uint32_t *upage);
PTE* page_pte_lookup(uint32_t *addr);
void page_table_init(struct hash* h);
PTE* parent_page_lookup(uint32_t* uaddr, struct thread* parent);
unsigned page_hash_hash_helper(const struct hash_elem * element, void * aux);
bool page_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux);


#endif /* vm/page.h */