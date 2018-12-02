#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

unsigned frame_hash_hash_helper(const struct hash_elem * element, void * aux);
bool frame_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void frame_init(void)
{
	sema_init(&frame_sema, 1);

	sema_down(&frame_sema);
	hash_init(&frame_table, frame_hash_hash_helper, frame_hash_less_helper, NULL);
	list_init(&fte_list);
	sema_up(&frame_sema);
}

uint8_t* frame_get_fte(uint32_t *upage, enum palloc_flags flag)
{
	if(upage == NULL) return NULL;

	uint32_t *kpage = palloc_get_page(flag);
	// if(kpage==NULL) printf("palloc failed\n");
	// printf("KPAGE : %p\n", kpage);
	
	// ASSERT(frame_fte_lookup(kpage) != NULL);
	// printf("FRAME SEMA BEOFRE %d\n", frame_sema.value);
	sema_down(&frame_sema);

	if(kpage == NULL)
	{
		sema_up(&frame_sema); 
		return NULL;
	}
	frame_set_fte(upage, kpage);
	sema_up(&frame_sema);
	// printf("FRAME SEMA AFTER %d\n", frame_sema.value);

	return kpage;
}

bool frame_set_fte(uint32_t *upage, uint32_t *kpage)
{
	FTE* new_fte = (FTE*)malloc(sizeof(FTE));
	new_fte->paddr = kpage;
	new_fte->uaddr = upage;
	// new_fte->is_swapped_out = false;
	new_fte->usertid = thread_current()->tid;

	hash_insert(&frame_table, &new_fte->helem);
	list_push_back(&fte_list, &new_fte->elem);
	return true;
}

void frame_remove_fte(uint32_t* kpage)
{
	if(kpage==NULL) return;
	
	sema_down(&frame_sema);
	// printf("REMOVE KPAGE : %p\n", kpage);
	FTE* fte = frame_fte_lookup(kpage);
	if(fte == NULL){
		sema_up(&frame_sema);
		return;
	}
	ASSERT(fte->paddr == kpage);
	// ASSERT(fte->usertid == thread_current()->tid);

	hash_delete(&frame_table, &fte->helem);
	list_remove(&fte->elem);
	
	palloc_free_page(kpage);
	pagedir_clear_page(thread_current()->pagedir, fte->uaddr);

	free(fte);

	sema_up(&frame_sema);
	// printf("FRAME SEMA AFTER\n");
}



FTE* frame_fifo_fte(void)
{
	int currtid = thread_current()->tid;
	struct list_elem *e = list_begin(&fte_list);
	int partid = thread_current()->parent->tid;
	FTE *fte = NULL;
	
	int num = 0;
	for(; e != list_end(&fte_list); e = list_next(e)){
		FTE *fte = list_entry(e, FTE, elem);
		// printf("fifo fte->tid : %d , curr tid : %d ", fte->usertid, currtid);
		if(fte->usertid == currtid){
			num++;
		}

	}
	// print("NUM : %d\n", num);
	if(num < 5){
		goto PARENT_SWAP;
	}
	e = list_begin(&fte_list);
	for(; e != list_end(&fte_list); e = list_next(e))
	{
		FTE *fte = list_entry(e, FTE, elem);
		// printf("fifo fte->tid : %d , curr tid : %d ", fte->usertid, currtid);
		if(fte->usertid == currtid){
			return fte;
		}
	}
PARENT_SWAP:
	// NOT_REACHED();
	

	
	e = list_begin(&fte_list);
	for(; e != list_end(&fte_list); e = list_next(e))
	{
		fte = list_entry(e, FTE, elem);
		if(fte->usertid == partid)
			break;
	}	
	// printf("FTE UADDR : %p\n", fte->uaddr);
	
	swap_parent(fte->uaddr);
	return NULL;
}

void parent_remove_fte(uint32_t* kpage, struct thread* parent)
{
	ASSERT(kpage!=NULL);
	
	sema_down(&frame_sema);

	FTE* fte = frame_fte_lookup(kpage);
	if(fte == NULL){
		sema_up(&frame_sema);
		return;
	}
	ASSERT(fte->paddr == kpage);
	ASSERT(fte->usertid == parent->tid);

	hash_delete(&frame_table, &fte->helem);
	list_remove(&fte->elem);
	
	palloc_free_page(kpage);
	pagedir_clear_page(parent->pagedir, fte->uaddr);

	free(fte);

	sema_up(&frame_sema);
	// printf("FRAME SEMA AFTER\n");
}



FTE* frame_fte_lookup(uint32_t *addr)
{
	FTE fte;
	struct hash_elem *helem;

	fte.paddr = addr;
	helem = hash_find(&frame_table, &fte.helem);

	// ASSERT(helem != NULL);
	// printf("lookup result : %p, %p\n", hash_entry(helem, FTE, helem)->uaddr, addr);
	// printf("USERTID : %d, tid : %d\n",hash_entry(helem, FTE, helem)->usertid, thread_current()->tid);

	// ASSERT(hash_entry(helem, FTE, helem)->usertid == thread_current()->tid);

	return helem!=NULL ? hash_entry(helem, FTE, helem) : NULL;
}

unsigned frame_hash_hash_helper(const struct hash_elem * element, void * aux)
{
	FTE *fte = hash_entry(element, FTE, helem);
	return hash_bytes(&fte->paddr, sizeof(uint32_t));
}

bool frame_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	uint32_t* a_key = hash_entry(a, FTE, helem) -> paddr;
	uint32_t* b_key = hash_entry(b, FTE, helem) -> paddr;
	if(a_key < b_key) return true;
	else return false;
}


