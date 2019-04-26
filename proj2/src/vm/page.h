#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <list.h>

struct sup_page_table_entry {
	uint32_t* uservaddr;
	uint64_t access_time;

	bool dirty;
	bool accessed;
	struct list_elem elem;
};