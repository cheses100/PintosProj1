#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/init.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
int write(int fd, const void* buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool addressCheck(void* adr)
{
	bool invalidAddress = (
		(adr == NULL) ||
		!is_user_vaddr(adr) ||
		(pagedir_get_page(active_pd(), adr) == NULL)
		);
		
	return !invalidAddress;
}


static void
syscall_handler (struct intr_frame *f) 
{
	//cast f->esp into an int*, then dereference it for the SYS_CODE
	int * sysCodeStar = (int*)f->esp;
  
	if (!addressCheck(f->esp))
	{
		printf("%s: exit(-1)\n", thread_current()->name);
		
		thread_exit();
	}
	
	switch(*sysCodeStar)
	{
		case SYS_HALT:
		{
			shutdown_power_off();
			
			break;
		}
		case SYS_EXIT:
		{
			printf("%s: exit(0)\n", thread_current()->name);
			thread_exit();
			break;
		}
		case SYS_EXEC:
		{
			
			break;
		}
		case SYS_WAIT:
		{
			
			break;
		}
		case SYS_CREATE:
		{
			char* fileName = (char*)(*(sysCodeStar + 1));
			
			if(!addressCheck(fileName))
			{
				printf("%s: exit(-1)\n", thread_current()->name);
			}
			unsigned size = *((unsigned*)sysCodeStar + 2);
			//run the syscall, a function of your own making
			//since this syscall returns a value, the return value should be
			
			bool fileCreated = filesys_create(fileName, size);
			
			f->eax = fileCreated;
			break;
		}
		case SYS_REMOVE:
		{
			
			break;
		}
		case SYS_OPEN:
		{
			
			break;
		}
		case SYS_FILESIZE:
		{
			
			break;
		}
		case SYS_READ:
		{
			
			break;
		}
		case SYS_WRITE:
		{
			int fd = *(sysCodeStar + 1);
			void* buffer = (void*)(*(sysCodeStar + 2));
			
			if(!addressCheck(buffer))
			{
				printf("%s: exit(-1)\n", thread_current()->name);
			}
			unsigned size = *((unsigned*)sysCodeStar + 3);
			//run the syscall, a function of your own making
			//since this syscall returns a value, the return value should be
			//stored in f->eax
			f->eax = write(fd, buffer, size);
			
			break;
		}
		case SYS_SEEK:
		{
			
			break;
		}
		case SYS_TELL:
		{
			
			break;
		}
		case SYS_CLOSE:
		{
			
			break;
		}
	}
	
	//*/
	
	
	
	
	
	/*
	
	}*/
}

int write(int fd, const void* buffer, unsigned size)
{
	
	
	if (fd == STDOUT_FILENO) putbuf(buffer, size);
	
	
	
	return size;
}