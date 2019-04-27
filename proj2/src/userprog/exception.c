#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include <stdlib.h>
#include "threads/malloc.h"
#include "devices/timer.h"
#include "vm/frame.h"
#include "devices/block.h"
/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
	 e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
	 we set DPL==3, meaning that user programs are allowed to
	 invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
					 "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
	 invoking them via the INT instruction.  They can still be
	 caused indirectly, e.g. #DE can be caused by dividing by
	 0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
					 "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
					 "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
	 We need to disable interrupts for page faults because the
	 fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
  sema_init(&exception_lock, 1);
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
	 For example, the process might have tried to access unmapped
	 virtual memory (a page fault).  For now, we simply kill the
	 user process.  Later, we'll want to handle page faults in
	 the kernel.  Real Unix-like operating systems pass most
	 exceptions back to the process via signals, but we don't
	 implement them. */
	 
  /* The interrupt frame's code segment value tells us where the
	 exception originated. */
  switch (f->cs)
	{
	case SEL_UCSEG:
	  /* User's code segment, so it's a user exception, as we
		 expected.  Kill the user process.  */
	  // printf ("%s: dying due to interrupt %#04x (%s).\n",
	  //         thread_name (), f->vec_no, intr_name (f->vec_no));
	  intr_dump_frame (f);
	  f->eax = -1;
	  printf("%s: exit(-1)\n", thread_current()->name);
	  thread_exit (); 

	case SEL_KCSEG:
	  /* Kernel's code segment, which indicates a kernel bug.
		 Kernel code shouldn't throw exceptions.  (Page faults
		 may cause kernel exceptions--but they shouldn't arrive
		 here.)  Panic the kernel to make the point.  */
	  intr_dump_frame (f);
	  PANIC ("Kernel bug - unexpected interrupt in kernel"); 

	default:
	  /* Some other code segment?  Shouldn't happen.  Panic the
		 kernel. */
	  printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
			 f->vec_no, intr_name (f->vec_no), f->cs);
	  thread_exit ();
	}
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
//printf("page faulting!!\n");
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
	 accessed to cause the fault.  It may point to code or to
	 data.  It is not necessarily the address of the instruction
	 that caused the fault (that's f->eip).
	 See [IA32-v2a] "MOV--Move to/from Control Registers" and
	 [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
	 (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
	 be assured of reading CR2 before it changed). */
  intr_enable ();
  sema_down(&exception_lock);
  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  void* esp = f->esp;
  if (!user) {
  	esp = thread_current()->saved_esp;
  }
  //fault_addr = pg_round_down(fault_addr);
  bool quit = false;
  int status = 0;
  //check if addr was null
  //int curr_addr = thread_current()->saved_esp;
  //printf("\n1\n\n%x, %x, %u, %x\n\n\n", fault_addr, thread_current()->saved_esp, (unsigned int)(thread_current()->saved_esp - fault_addr), f->esp);
  if (fault_addr == NULL) {
	quit = true;
	status = 1;
  }
  //check if user tried to access kernel address
  else if (is_kernel_vaddr(fault_addr) && user) {
	quit = true;
	status = 2;
  }
  //check if the address is below the user stack
  else if ((unsigned int)fault_addr < 0x08048000) {
	quit = true;
	status = 3;
  }
  //check if the address is a reasonable distance from the current user stack pointer
  
  else if (fault_addr < (esp - 32) && user) {
	quit = true;
	//printf("\n2\n\n%x, %x, %u, %x\n\n\n", fault_addr, thread_current()->saved_esp, (unsigned int)(thread_current()->saved_esp - fault_addr), f->esp);
	status = 4;
  }
  else if (!not_present && !user) {
  	quit = true;
  	status = 5;
  	printf("%s: exit(-1)\n", thread_current()->name);
	thread_exit (); 
  } else if (!not_present){
  	quit = true;
  	status = 6;
  }


	if(quit) {
		printf ("Page fault at %p: %s error %s page in %s context.\n",
			  fault_addr,
			  not_present ? "not present" : "rights violation",
			  write ? "writing" : "reading",
			  user ? "user" : "kernel");
		//printf("\n\n%d", status);
		kill (f);
	} else {

		//case 1: memory isn't full, page isnt in page table
		//grab new page, create a pagetable entry, and create a frametable entry
		//case 2: memory isn't full, page is in page table
		//Can this happen? if it can, then swap the page back into memory
		//case 3: memory is full, page is in page table 
		//this means the page was swapped out of memory but already allocated before
		//need to take a page from memory and put it on the disk
		//and then find this page in disk and load it into memory
		//then update the frame table to show new page is now in physical memory
		//case 4: memory is full, page isnt in the page table
		//evict some page in memory to hard drive. Create new page table entry
		//update frame table try
		
		void* upage = pg_round_down(fault_addr);
		bool found_page = false;
		struct sup_page_table_entry* curr_page;
		//check pagetable of current thread to see if the page is in the table already
		for (struct list_elem* iter = list_begin(&thread_current()->page_table);
		iter != list_end(&thread_current()->page_table);
		iter = list_next(iter))
		{
			struct sup_page_table_entry * page_table_elem = list_entry(iter, struct sup_page_table_entry, elem);
			if (page_table_elem->uservaddr == upage) {
				found_page = true;
				curr_page = page_table_elem;
			}
		}
		//if it is, that means it was evicted and needs to be brought back in
		if (found_page) {
			//TODO figure this out once swap table is set up
			//find page in memory
			//swap out page based on lru
			//swap page into recently freed spot
			//update frametable
		} else {
			//printf("Extending stack\n");
			//printf("\n%x\n", pg_round_up(upage));
			void* kpage = (void*)palloc_get_page(PAL_USER | PAL_ZERO);
			if(kpage == NULL) {
				intr_dump_frame (f);
		 		PANIC ("Ran out of memory - fix this by implementing swaps"); 
		 		//pick page to swap out of memory based on lru
		 		//swap it out
		 		//grab the piece of memory where that page was, and put in new page
		 		//update page table
		 		//update frame table
			}
			bool writable = true;
			//if this returns false, it means we ran out of physical memory
			//for now panic the kernal
			//later, swap memory out, and swap this memory in
			bool success = install_page(upage, kpage, writable);
			if (!success) {
				//not sure what we need to do about this
				intr_dump_frame (f);
		 		PANIC ("There's already a page at this virtual address"); 
			} else {
				//if it's not:
				//get a new page, allocate new frametable entry, create pagetable enty to track this
				//create pagetable entry
				struct sup_page_table_entry* new_elem = sup_page_table_insert(upage, timer_ticks(), -1, false, true);
				

				//create frametable entry
				struct frame_table_entry* ftable_entry = frame_table_insert(kpage, thread_current(), new_elem);
			}
				
		}
		
		

	    //printf("%d", success);
	}
	sema_up(&exception_lock);
	
	intr_enable ();
}

