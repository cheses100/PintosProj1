#include "frame.h"



struct frame_table_entry* frame_table_insert(uint32_t* frame, struct thread* owner, struct sup_page_table_entry* aux);




struct frame_table_entry* frame_table_insert(uint32_t* frame, struct thread* owner, struct sup_page_table_entry* aux) {


	struct frame_table_entry* newFrameElem = malloc(sizeof(struct frame_table_entry));
	newFrameElem->frame = frame;
	newFrameElem->owner = thread_current();
	newFrameElem->aux = aux;
	list_push_back (&frame_table, &newFrameElem->elem);
}