#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_SYS_WRITE(struct intr_frame *f UNUSED)
{
  int fd;
  const void *buffer;
  unsigned size;

  fd = (uint32_t *)(f->esp+4);
  printf("Sys_write");
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("system call!\n");
  // thread_exit ();
  

  uint32_t * esp = (uint32_t *)(f->esp);
  char * buffer = (char *)*(esp+2);
  printf("%s\n", buffer);
}
