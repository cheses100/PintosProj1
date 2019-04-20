struct list frame_table;

struct frame_table_entry {
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* aux;
}