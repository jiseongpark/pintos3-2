#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/thread.h"

unsigned swap_hash_hash_helper(const struct hash_elem * element, void * aux);
bool swap_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void swap_init(void)
{
	sema_init(&swap_sema, 1);
	// printf("SEMA VALUE : %d\n", swap_sema.value);

	// sema_down(&swap_sema);
	// hash_init(&swap_table, swap_hash_hash_helper, swap_hash_less_helper, NULL);
	// sema_up(&swap_sema);
}

void swap_table_init(struct hash* h)
{
	if(h == NULL) return;
	// sema_down(&swap_sema);
  	hash_init(h, swap_hash_hash_helper, swap_hash_less_helper, NULL);
  	// sema_up(&swap_sema);
}

void swapdisk_bitmap_init(void)
{
	swapdisk_bitmap = bitmap_create(8064);
}

STE* swap_set_ste(uint32_t *upage)
{
	struct thread *t = thread_current();
	// sema_down(&swap_sema);
	STE* new_ste = (STE*)malloc(sizeof(STE));
	new_ste->uaddr = upage;
	int i=0;
	for(; i<8; i++) new_ste->sec_no[i] = -1;

	hash_insert(&t->st, &new_ste->helem);
	
	// sema_up(&swap_sema);



	return new_ste;
}


STE* parent_swap_set(uint32_t* upage, struct thread * parent)
{

	// sema_down(&swap_sema);
	STE* ste = (STE*)malloc(sizeof(STE));
	ste->uaddr = upage;
	int i=0;
	for(; i<8; i++) ste->sec_no[i] = -1;
	hash_insert(&parent->st, &ste->helem);
	// sema_up(&swap_sema);

	return ste;
}



void swap_remove_ste(uint32_t* upage)
{
	// sema_down(&swap_sema);
	struct thread *t = thread_current();

	if(upage==NULL) return;

	
	// printf("UPAGE : %p\n", upage);
	STE* ste = swap_ste_lookup(upage);
	if(ste != NULL) {
		hash_delete(&t->st, &ste->helem);
		free(ste);
	}

	// sema_up(&swap_sema);
}


STE* swap_ste_lookup(uint32_t *addr)
{
	STE ste;
	struct hash_elem *helem;
	struct thread *t = thread_current();

	ste.uaddr = addr;
	helem = hash_find(&t->st, &ste.helem);

	return helem!=NULL ? hash_entry(helem, STE, helem) : NULL;
}

bool swap_in(uint32_t *uaddr)
{
	PTE *pte = page_pte_lookup(uaddr);
	STE *ste = swap_ste_lookup(uaddr);
	uint8_t *paddr = NULL;
	// printf("SWAP IN COME\n");
	ASSERT(pte->uaddr == uaddr);

	/* add to frame table */
	paddr = frame_get_fte(uaddr, PAL_USER | PAL_ZERO);

	if(paddr == NULL) return false;

	/* put data to physical memory */
	// printf("SWAP IN BEFORE : %d\n", swap_sema.value);
	// sema_down(&swap_sema);
	int i=0;
	for(; i<8; i++)
	{
		ASSERT(ste->sec_no[i] != -1);
		disk_read(disk_get(1,1), ste->sec_no[i], paddr + i*512);
		bitmap_set(swapdisk_bitmap, ste->sec_no[i], false);
	}
	

	/* install page */
	ASSERT(pagedir_get_page(thread_current()->pagedir, uaddr) == NULL);
	ASSERT(pagedir_set_page(thread_current()->pagedir, uaddr, paddr, true));

	/* update mapping info in page table*/
	pte->paddr = paddr;
	pte->is_swapped_out = false;

	/* erase info from swap table */
	// sema_up(&swap_sema);
	// printf("SWAP IN AFTER : %d\n", swap_sema.value);
	swap_remove_ste(uaddr);
	

	return true;
}

bool swap_out(uint32_t *uaddr)
{	
	
	PTE *pte = page_pte_lookup(uaddr);
	
	/* Put to swap table */
	STE *ste = swap_set_ste(uaddr);
	// printf("SWAP OUT BEFORE : %d\n", swap_sema.value);
	// sema_down(&swap_sema);
	/* disk get */
	int i = 0, cnt = 0;
	while(cnt<8)
	{
		if(!bitmap_test(swapdisk_bitmap, i))
			ste->sec_no[cnt++] = i;
		i++;
	}

	/* pte->is_swapped_out = true */
	pte->is_swapped_out = true;

	/* store file data to disk */
	// sema_down(&swap_sema);
	i=0;
	for(; i<8; i++)
	{
		ASSERT(ste->sec_no[i] != -1);
		disk_write(disk_get(1,1), ste->sec_no[i], pte->paddr + i*512/4);
		bitmap_set(swapdisk_bitmap, ste->sec_no[i], true);
	}
	// sema_up(&swap_sema);

	// sema_up(&swap_sema);
	// printf("SWAP OUT AFTER : %d\n", swap_sema.value);
	/* remvoe from frame table */
	frame_remove_fte(pte->paddr);
	
	return true;	
}

void swap_parent(uint32_t* uaddr){
	/* page_pte_lookup*/
	// printf("swap parent entered\n");
	
	struct thread* parent = thread_current()->parent;
	PTE* pte = parent_page_lookup(uaddr,parent);
	STE* ste = parent_swap_set(uaddr, parent);
	/* swap_ste_lookup*/
	// printf("pass 1\n");
	/* disk get */
	// printf("PARENT VALUE BEFORE : %d\n", swap_sema.value);
	// sema_down(&swap_sema);
	int i = 0, cnt = 0;

	while(cnt<8)
	{
		if(!bitmap_test(swapdisk_bitmap, i)){
			ste->sec_no[cnt++] = i;
		}
		i++;
	}

	// printf("pass 2\n");
	pte->is_swapped_out = true;

	/* store file data to disk */

	i=0;
	for(; i<8; i++)
	{
		
		ASSERT(ste->sec_no[i] != -1);
		disk_write(disk_get(1,1), ste->sec_no[i], pte->paddr + i*512/4);
		bitmap_set(swapdisk_bitmap, ste->sec_no[i], true);
	}

	// sema_up(&swap_sema);
	// printf("PARENT VALUE AFTER : %d\n", swap_sema.value);

	/* remvoe from frame table */
	parent_remove_fte(pte->paddr, parent);
	
	// printf("swap parent exited\n");
}

unsigned swap_hash_hash_helper(const struct hash_elem * element, void * aux)
{
	STE *ste = hash_entry(element, STE, helem);
	return hash_bytes(&ste->uaddr, sizeof(uint32_t));
}

bool swap_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	uint32_t* a_key = hash_entry(a, STE, helem) -> uaddr;
	uint32_t* b_key = hash_entry(b, STE, helem) -> uaddr;
	if(a_key < b_key) return true;
	else return false;
}
