#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/init.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "devices/timer.h"



static void syscall_handler (struct intr_frame *);
int write(int fd, const void* buffer, unsigned size, struct fileListElem* elem);
bool addressCheck(const void * adr);
struct fileListElem* getFileWithFd(int fd);
void doBadExit(void);

int argCounts[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 3, 3, 2, 1, 1, 2, 1
};



void
syscall_init (void) 
{
  sema_init(&IOLock, 1);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool addressCheck(const void* adr)
{
	bool validAddress = (
		(adr != NULL) &&
		is_user_vaddr(adr) &&
		(pagedir_get_page(thread_current()->pagedir, adr) != NULL)
		);
	// if(pagedir_get_page(thread_current()->pagedir, adr) == NULL) {
	// 	//printf("pagedir get page failed for: %x", adr);
	// }
		
	return validAddress;
	
	
}

bool addressCheckWithEsp(const void* adr, void* esp)
{
	bool validAddress = (
		adr != NULL &&
		((is_user_vaddr(adr) &&	pagedir_get_page(thread_current()->pagedir, adr != NULL))
		|| adr >= (esp - 32))
		);
	// if(pagedir_get_page(thread_current()->pagedir, adr) == NULL) {
	// 	//printf("pagedir get page failed for: %x", adr);
	// }
		
	return validAddress;
	
	
}

void doBadExit()
{
	printf("%s: exit(-1)\n", thread_current()->name);
	thread_exit();
}

//I added this
struct fileListElem * getFileWithFd(int fd) 
{
	for (struct list_elem* iter = list_begin(&thread_current()->fileList);
	 iter != list_end(&thread_current()->fileList);iter = list_next(iter))
	{
	  	struct fileListElem* fileEl = list_entry(iter, struct fileListElem, elem);
	  	 //printf("________\n\n\nfileEl->fd: %d\nfd: %d\n\n________\n", fileEl->fd, fd);
	  	if (fileEl->fd == fd) {
	  		return fileEl;
	  	}
	  	
  	}
  	return NULL;
}

static void
syscall_handler (struct intr_frame *f) 
{
	//cast f->esp into an int*, then dereference it for the SYS_CODE
	int * sysCodeStar = (int*)f->esp;
	int * arg1 = sysCodeStar + 1;
	int * arg2 = sysCodeStar + 2;
	int * arg3 = sysCodeStar + 3;
  
	if (!addressCheck(f->esp)) doBadExit();
	
	if (*sysCodeStar < 0 || *sysCodeStar > 14) doBadExit();
	
	int * tempArg = arg1;
	for (int i = 0; i < argCounts[*sysCodeStar]; i++) 
	{
		if (!addressCheck(tempArg)) doBadExit();
		tempArg++;
	}
	thread_current()->saved_esp = f->esp;
	switch(*sysCodeStar)
	{
		case SYS_HALT:
		{
			shutdown_power_off();
			
			break;
		}
		case SYS_EXIT:
		{
			int status = *arg1;
			for (struct list_elem* iter = list_begin(&thread_current()->fileList);
			 iter != list_end(&thread_current()->fileList);)
			{
				struct fileListElem* fileEl = list_entry(iter, struct fileListElem, elem);
			  	file_close (fileEl->mFile);
				iter = list_next(iter);
			  	list_remove (&fileEl->elem);
			  	free(fileEl);
			}
			printf("%s: exit(%d)\n", thread_current()->name, status);
			thread_current()->exitStatus = status;
            f->eax = status;
			thread_exit();
			
			break;
		}
		case SYS_EXEC:
		{
			const char * execArgs = (const char *)(*arg1);
			
			if (!addressCheck(execArgs)) doBadExit();
			int pid = process_execute(execArgs);
			
			// wait for exec to load
			sema_down(&thread_current()->waiting2);
			if (thread_current()->childLoadStatus) {
				f->eax = pid;
			} else {
				f->eax = -1;
			}
			
			break;
		}
		case SYS_WAIT:
		{
			tid_t pid = *arg1;
			int waitStatus = process_wait(pid);
			
			f->eax = waitStatus;
			
			break;
		}
		case SYS_CREATE:
		{
			//printf("creating file\n");
			sema_down(&IOLock);
			char * fileName= (char*)(*arg1);
			int size = *arg2;
			if(!addressCheck(fileName)) {
				sema_up(&IOLock);
				doBadExit();
			}
			
			bool fileCreated = filesys_create(fileName, size);
			
			f->eax = fileCreated;
			sema_up(&IOLock);
			break;
		}
		case SYS_REMOVE:
		{
			sema_down(&IOLock);
			char * fileName= (char*)(*arg1);
			if(!addressCheck(fileName)){
				sema_up(&IOLock);
				doBadExit();
			
			} 
			bool fileRemoved = filesys_remove(fileName);
			
			f->eax = fileRemoved;
			sema_up(&IOLock);
			break;
		}
		case SYS_OPEN:
		{
			//printf("openning file\n");
			sema_down(&IOLock);
			//I aded this
			char * fileName= (char*)(*arg1);
			if(!addressCheck(fileName)){
				sema_up(&IOLock);
				doBadExit();
			
			} 
			
			void * newfile = filesys_open(fileName);
			if (newfile != NULL) {
				//TODO remember to free this later
				struct fileListElem* newElem = malloc(sizeof(struct fileListElem));
				newElem->mFile = (struct file*)newfile;
				newElem->fd = thread_current()->fdCounter;
				list_push_back (&(thread_current()->fileList), &newElem->elem);
			}
			sema_up(&IOLock);
			
			f->eax = (newfile == NULL) ? -1 : thread_current()->fdCounter++;
			break;
		}
		case SYS_FILESIZE:
		{
			//I added this
			sema_down(&IOLock);
			int fd = *arg1;
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();
			}
			f->eax = file_length(curElem->mFile);
			sema_up(&IOLock);
			break;
		}
		case SYS_READ:
		{
			//I added this
			//printf("reading \n");
			sema_down(&IOLock);
			int fd = *arg1;
			void* buffer = (void*)(*arg2);
			unsigned size = *(unsigned*)arg3;
			if (!((buffer != NULL) && is_user_vaddr(buffer))) {
				doBadExit();
			}
			if(!addressCheck(buffer))  {
				//printf("buffer: %x", buffer);
				void* upage = pg_round_down(buffer);
				if (buffer >= (f->esp - 32)) {

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
				} else{
					doBadExit();	
				}
				//printf("this is what fails");
				
			}
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();

			}
			
			if (fd == 0) {
				f->eax = input_getc();
			} else {
				f->eax = file_read(curElem->mFile, buffer, size);
			}
			
			sema_up(&IOLock);
			break;
		}
		case SYS_WRITE:
		{
			//printf("writing file\n");
			int fd = *arg1;
			void* buffer = (void*)(*arg2);
			unsigned size = *(unsigned*)arg3;
			
			if(!addressCheck(buffer)) {

				doBadExit();
			}
			//I added/modified this
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL && fd != STDOUT_FILENO) {
				doBadExit();
			}
			f->eax = write(fd, buffer, size, curElem);
			
			break;
		}
		case SYS_SEEK:
		{
			//I added this
			sema_down(&IOLock);
			int fd = *arg1;
			unsigned position = *(unsigned*)arg2;
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();

			}
			file_seek(curElem->mFile, position);
			sema_up(&IOLock);
			break;
		}
		case SYS_TELL:
		{
			//I added this
			sema_down(&IOLock);
			int fd = *arg1;
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();
			}
			f->eax = file_tell(curElem->mFile);
			sema_up(&IOLock);
			break;
		}
		case SYS_CLOSE:
		{
			//i added this
			sema_down(&IOLock);
			int fd = *arg1;
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();
			}
			file_close (curElem->mFile);
			list_remove (&curElem->elem);
			free(curElem);
			sema_up(&IOLock);
			break;
		}
		case SYS_MMAP:
		{
			thread_current()->saved_esp = f->esp;
			sema_down(&IOLock);
			//int fd, void *addr
			int fd = *arg1;
			uint8_t* upage = (uint8_t*) arg2;
			struct fileListElem* curElem = getFileWithFd(fd);
			if (curElem == NULL) {
				sema_up(&IOLock);
				doBadExit();
			}
			struct file* fresh_file = file_reopen(curElem->mFile);
			if (fresh_file == NULL) {
				sema_up(&IOLock);
				doBadExit();
			}

			uint32_t read_bytes = file_length(fresh_file);
			uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
			while (read_bytes > 0 || zero_bytes > 0) 
		    {
		      /* Calculate how to fill this page.
		         We will read PAGE_READ_BYTES bytes from FILE
		         and zero the final PAGE_ZERO_BYTES bytes. */
		      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		      size_t page_zero_bytes = PGSIZE - page_read_bytes;

		      /* Get a page of memory. */
		      uint8_t *kpage = palloc_get_page (PAL_USER);
		      
		      
		      if (kpage == NULL)
		      {
		      	evict_frame();
		      	kpage = palloc_get_page (PAL_USER);
		      	if (kpage == NULL)
		      	{
			        sema_up(&IOLock);
			    	//printf("kpage is null");
					doBadExit();
				}
		      }

		      /* Load this page. */
		      if (file_read (fresh_file, kpage, page_read_bytes) != (int) page_read_bytes)
		        {
		          palloc_free_page (kpage);
		          sema_up(&IOLock);
		          //printf("file read failed");
				  doBadExit();
		        }
		      memset (kpage + page_read_bytes, 0, page_zero_bytes);


		      /* Add the page to the process's address space. */
		      if (!install_page (upage, kpage, true)) 
		        {
		          palloc_free_page (kpage);
		          sema_up(&IOLock);
		          //printf("failed to install page");
					doBadExit();
		        }


		        //create pagetable entry
				struct sup_page_table_entry* newElem = malloc(sizeof(struct sup_page_table_entry));
				newElem->uservaddr = (uint32_t*)upage;
				newElem->dirty = false;
				newElem->access_time = timer_ticks();
				newElem->accessed = true;
				list_push_back (&(thread_current()->page_table), &newElem->elem);

				//create frametable entry
				struct frame_table_entry* newFrameElem = malloc(sizeof(struct frame_table_entry));
				newFrameElem->frame = (uint32_t*)kpage;
				newFrameElem->owner = thread_current();
				newFrameElem->aux = newElem;
				list_push_back (&(frame_table), &newFrameElem->elem);

		        read_bytes -= page_read_bytes;
				zero_bytes -= page_zero_bytes;
				upage += PGSIZE;
		    }
			sema_up(&IOLock);
			break;
		}
		case SYS_MUNMAP:
		{
			sema_down(&IOLock);
			//mapid_t mapid
			mapid_t mapid = *arg1;
			sema_up(&IOLock);
			break;
		}
	}
	

}



int write(int fd, const void* buffer, unsigned size, struct fileListElem* elem )
{
	sema_down(&IOLock);
	//I added/modified this
	if (fd == STDOUT_FILENO) {
		putbuf(buffer, size);
	} else if (fd == 0){
		doBadExit();
	} else {
		struct file * file = elem->mFile;
		if (file->deny_write) doBadExit();
		size = file_write(elem->mFile, buffer, size);
	}
	sema_up(&IOLock);
	
	return size;
}