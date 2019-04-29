#include "page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/timer.h"


struct sup_page_table_entry* sup_page_table_insert(uint8_t* uservaddr, uint64_t access_time, int block_index, bool dirty, bool accessed) {
	struct sup_page_table_entry* newElem = malloc(sizeof(struct sup_page_table_entry));
	newElem->uservaddr = uservaddr;
	newElem->dirty = false;
	newElem->block_index = block_index;
	newElem->access_time = timer_ticks();
	newElem->accessed = true;
	list_push_back (&(thread_current()->page_table), &newElem->elem);
	return newElem;
}