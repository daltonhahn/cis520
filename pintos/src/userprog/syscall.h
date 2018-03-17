#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void exit(int);

// Added to provide pid_t to syscalls
typedef int pid_t;

#endif /* userprog/syscall.h */
