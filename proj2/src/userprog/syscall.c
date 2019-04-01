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
bool addressCheck(void * adr);

int argCounts[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 3, 3, 2, 1, 1
};

int fdCounter = 5;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool addressCheck(void* adr)
{
	bool validAddress = (
		(adr != NULL) &&
		is_user_vaddr(adr) &&
		(pagedir_get_page(thread_current()->pagedir, adr) != NULL)
		);
		
	return validAddress;
}

void doBadExit()
{
	printf("%s: exit(-1)\n", thread_current()->name);
	thread_exit();
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
	
	if (*sysCodeStar < 0 || *sysCodeStar > 12) doBadExit();
	
	int * tempArg = arg1;
	for (int i = 0; i < argCounts[*sysCodeStar]; i++) 
	{
		if (!addressCheck(tempArg)) doBadExit();
		tempArg++;
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
			int status = *arg1;
			
			printf("%s: exit(%d)\n", thread_current()->name, status);
			thread_current()->exitStatus = status;
			thread_exit();
			f->eax = status;
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
			char * fileName= (char*)(*arg1);
			int size = *arg2;
			if(!addressCheck(fileName)) doBadExit();
			
			bool fileCreated = filesys_create(fileName, size);
			
			f->eax = fileCreated;
			break;
		}
		case SYS_REMOVE:
		{
			char * fileName= (char*)(*arg1);
			if(!addressCheck(fileName)) doBadExit();
			
			bool fileRemoved = filesys_remove(fileName);
			
			f->eax = fileRemoved;
			break;
		}
		case SYS_OPEN:
		{
			char * fileName= (char*)(*arg1);
			if(!addressCheck(fileName)) doBadExit();
			
			void * file = filesys_open(fileName);
			
			f->eax = (file == NULL) ? -1 : fdCounter++;
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
			int fd = *arg1;
			void* buffer = (void*)(*arg2);
			unsigned size = *(unsigned*)arg3;
			
			if(!addressCheck(buffer)) doBadExit();
			
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
	

}

int write(int fd, const void* buffer, unsigned size)
{
	
	
	if (fd == STDOUT_FILENO) putbuf(buffer, size);
	
	
	
	return size;
}