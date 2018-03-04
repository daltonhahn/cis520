#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/***********/
#include "devices/shutdown.h" /* For use in the halt syscall */
/***********/

static void syscall_handler (struct intr_frame *);

void
syscall_SYS_WRITE(struct intr_frame *f)
{
  int fd;
  const void *buffer;
  unsigned size;

  fd = *(int *)(f->esp+4);
  buffer = (void *)*(uint32_t *)(f->esp+8);
  size = *(unsigned *)(f->esp+12);

  //printf("File Descriptor: <%d> Pointer: <%x> Size: <%d>\n", fd, (unsigned)buffer, size);

  switch(fd)
  {
    case STDOUT_FILENO:
      //Option 1
      while(size--){
	      printf("%c", *(char *)buffer++);
      }
      //Option 2 
      /*
      if((* (char *)buffer == 0x0a) && (size == 1) )
      {
        printf("%c", '\n');
      }
      else
      {
        printf("%s", buffer);
      }
      */
      
      
      break;

    default:
      printf("Write Syscall");
      break;
  }
}

void 
syscall_SYS_EXIT(struct intr_frame *f)
{
  printf("Called SYS_EXIT\n");
  thread_exit();
}

void 
syscall_SYS_HALT(struct intr_frame *f)
{
  printf("Called SYS_HALT\n");
  shutdown_power_off();
}



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // printf ("system call!\n");
  // thread_exit ();
  /*
  uint32_t * esp = (uint32_t *)(f->esp);
  char * buffer = (char *)*(esp+2);

  printf("%s\n", buffer);
  */

  uint32_t syscall_num = *(uint32_t *)(f->esp);
  switch(syscall_num)
  {
    case SYS_WRITE:
      syscall_SYS_WRITE(f);
      break;
    
    case SYS_EXIT:
      syscall_SYS_EXIT(f);
      break;

    case SYS_HALT:
      syscall_SYS_HALT(f);
      break;

    default:
      printf("Default syscall");
      break;
  }
 
  
    

  
}
