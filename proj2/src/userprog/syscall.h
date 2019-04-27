#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
typedef int mapid_t;
void syscall_init (void);
struct semaphore IOLock;
#endif /* userprog/syscall.h */
