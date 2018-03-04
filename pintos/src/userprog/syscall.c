#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/***********/
#include "devices/shutdown.h" /* For use in the halt syscall */
/***********/

static void syscall_handler (struct intr_frame *);
void syscall_SYS_HALT(struct intr_frame *);
void syscall_SYS_EXIT(struct intr_frame *);
void syscall_SYS_WRITE(struct intr_frame *);


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
    case SYS_HALT:
     syscall_SYS_HALT(f);
     break;
     
    case SYS_EXIT:
      syscall_SYS_EXIT(f);
      break;

    case SYS_EXEC:
      printf("NOT IMPLEMENTED YET - SYS_EXEC\n");
      break;

    case SYS_WAIT:
      // CREATE WAIT FUNCTION AND CALL PROCESS_WAIT FROM FUNCTION
      printf("NOT IMPLEMENTED YET - SYS_WAIT\n");
      break;

    case SYS_CREATE:
      printf("NOT IMPLEMENTED YET - SYS_CREATE\n");
      break;

    case SYS_REMOVE:
      printf("NOT IMPLEMENTED YET - SYS_REMOVE\n");
      break;

    case SYS_OPEN:
      printf("NOT IMPLEMENTED YET - SYS_OPEN\n");
      break;

    case SYS_FILESIZE:
      printf("NOT IMPLEMENTED YET - SYS_FILESIZE\n");
      break;

    case SYS_READ:
      printf("NOT IMPLEMENTED YET - SYS_READ\n");
      break;

    case SYS_WRITE:
      syscall_SYS_WRITE(f);
      break;

    case SYS_SEEK:
      printf("NOT IMPLEMENTED YET - SYS_SEEK\n");
      break;

    case SYS_TELL:
      printf("NOT IMPLEMENTED YET - SYS_TELL\n");
      break;

    case SYS_CLOSE:
      printf("NOT IMPLEMENTED YET - SYS_CLOSE\n");
      break;

    default:
      printf("Default SYSCALL Catcher");
      break;
  }
}

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
  struct thread *cur_thread = thread_current();
  int status = *(int *)(f->esp+4);

  if(cur_thread->parent->waiting_for_child == true)
  {
    cur_thread->parent->child_exit_status = status;
    sema_up(&cur_thread->parent->wait_child_sema);
  }

  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

void
syscall_SYS_HALT(struct intr_frame *f)
{
  printf("Called SYS_HALT\n");
  shutdown_power_off();
}




