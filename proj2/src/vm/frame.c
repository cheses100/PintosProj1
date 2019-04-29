#include "frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"



struct frame_table_entry* frame_table_insert(uint8_t* frame, struct thread* owner, struct sup_page_table_entry* aux) {


	struct frame_table_entry* newFrameElem = malloc(sizeof(struct frame_table_entry));
	newFrameElem->frame = frame;
	newFrameElem->owner = owner;
	newFrameElem->aux = aux;
	list_push_back (&frame_table, &newFrameElem->elem);
	return newFrameElem;
}