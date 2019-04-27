#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <list.h>


struct sup_page_table_entry {
	uint32_t* uservaddr;
	uint64_t access_time;
	int block_index;
	bool dirty;
	bool accessed;
	struct list_elem elem;
};

struct sup_page_table_entry* sup_page_table_insert(uint32_t* uservaddr, uint64_t access_time, int block_index, bool dirty, bool accessed);