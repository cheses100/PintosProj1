#include "page.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <list.h>

struct list frame_table;

struct frame_table_entry {
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* aux;
	struct list_elem elem;
};