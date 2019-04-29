#include "threads/vaddr.h"
#include "swap.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "devices/block.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static struct block * global_swap_block;
static int curr_index = 0;

static struct lock swap_IO_lock;

void swap_init(void) {
	global_swap_block = block_get_role(BLOCK_SWAP);
	curr_index = 0;
	lock_init(&swap_IO_lock);
}

void write_frame_to_block(uint8_t* frame, int index) {
	//lock_acquire(&swap_IO_lock);
	//printf("writing frame to block");
	for (int i = 0; i < 8; i++) {
  		//printf("checked\n");
		block_write(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
		//printf("asdfasdfasdf\n\n");
	}
	curr_index += 8;
	//printf("done writing\n\n");
	//lock_release(&swap_IO_lock);
}

void load_frame_from_block(uint8_t* frame, int index) {
	lock_acquire(&swap_IO_lock);
	for (int i = 0; i < 8; i++) {
		block_read(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	
	}

//printf("\nloaded black from dsik\n");

	lock_release(&swap_IO_lock);
}


bool evict_frame()
{
	lock_acquire(&swap_IO_lock);
	struct thread * cur = thread_current();
	uint64_t maxTime = ~(uint64_t)0;
	struct frame_table_entry * maxFrame = NULL;
    for (struct list_elem* iter = list_begin(&frame_table);
		iter != list_end(&frame_table);
		iter = list_next(iter))
	{
		struct frame_table_entry * frame_table_elem = list_entry(iter, struct frame_table_entry, elem);
		//if (frame_table_elem->owner == cur) continue;
		if (frame_table_elem->aux->access_time < maxTime)
		{
			maxTime =  frame_table_elem->aux->access_time;
			maxFrame = frame_table_elem;
		}
	}
	if (maxFrame == NULL) {

		//printf("evicitng frame failed\n");
		return false;
	}
		
	pagedir_clear_page(cur->pagedir, maxFrame->aux->uservaddr);

	
	maxFrame->aux->block_index = (maxFrame->aux->block_index == -1) ? curr_index : maxFrame->aux->block_index;
	write_frame_to_block(maxFrame->frame, maxFrame->aux->block_index);
	palloc_free_page(maxFrame->frame);
	list_remove(&maxFrame->elem);
	free(maxFrame);
	lock_release(&swap_IO_lock);

	//printf("evicitng frame succeded\n");
	return true;
}

