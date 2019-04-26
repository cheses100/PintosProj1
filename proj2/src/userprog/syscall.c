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

static void syscall_handler (struct intr_frame *);
int write(int fd, const void* buffer, unsigned size, struct fileListElem* elem);
bool addressCheck(void * adr);



int argCounts[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 3, 3, 2, 1, 1
};



void
syscall_init (void) 
{
  sema_init(&IOLock, 1);
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
			sema_down(&IOLock);
			int fd = *arg1;
			void* buffer = (void*)(*arg2);
			unsigned size = *(unsigned*)arg3;
			if(!addressCheck(buffer)) doBadExit();
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