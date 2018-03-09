#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

/***********/
#include "devices/shutdown.h" /* For use in the halt syscall */
/***********/

static void syscall_handler (struct intr_frame *);
void syscall_SYS_HALT();
void syscall_SYS_EXIT(int status);
void syscall_SYS_WRITE(int fd, const void *buffer, unsigned size);
bool check_ptr (const void *usr_ptr);

bool
check_ptr (const void *usr_ptr)
{
  if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
    {
      return (pagedir_get_page (thread_current()->pagedir, usr_ptr)) != NULL;
    }
  return false;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{

  if (!check_ptr (f->esp) || !check_ptr (f->esp + 1) ||
      !check_ptr (f->esp + 2) || !check_ptr (f->esp + 3))
  {
    syscall_SYS_EXIT(-1);
  }

  switch(*(int*)f->esp)
  {
    case SYS_HALT:
      syscall_SYS_HALT();
      break;
    
    case SYS_EXIT:
      syscall_SYS_EXIT(*(int *)(f->esp+4));
      break;

    case SYS_EXEC:
      //printf("NOT IMPLEMENTED YET - SYS_EXEC\n");
      break;

    case SYS_WAIT:
      //printf("NOT IMPLEMENTED YET - SYS_WAIT\n");
      break;

    case SYS_CREATE:
      //printf("NOT IMPLEMENTED YET - SYS_CREATE\n");
      break;

    case SYS_REMOVE:
      //printf("NOT IMPLEMENTED YET - SYS_REMOVE\n");
      break;

    case SYS_OPEN:
      //printf("NOT IMPLEMENTED YET - SYS_OPEN\n");
      break;

    case SYS_FILESIZE:
      //printf("NOT IMPLEMENTED YET - SYS_FILESIZE\n");
      break;

    case SYS_READ:
      //printf("NOT IMPLEMENTED YET - SYS_READ\n");
      break;

    case SYS_WRITE:
      syscall_SYS_WRITE(*(int *)(f->esp+4), (void *)*(uint32_t *)(f->esp+8), *(unsigned *)(f->esp+12));
      break;

    case SYS_SEEK:
      //printf("NOT IMPLEMENTED YET - SYS_SEEK\n");
      break;

    case SYS_TELL:
      //printf("NOT IMPLEMENTED YET - SYS_TELL\n");
      break;

    case SYS_CLOSE:
      printf("NOT IMPLEMENTED YET - SYS_CLOSE\n");
      break;

    default:
      //printf("Default SYSCALL Catcher");
      break;
  }
}

void
syscall_SYS_WRITE(int fd, const void *buffer, unsigned size)
{
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
syscall_SYS_EXIT(int status)
{
  struct thread *cur_thread = thread_current();

  if(cur_thread->parent->waiting_for_child == true)
  {
    cur_thread->parent->child_exit_status = status;
    sema_up(&cur_thread->parent->wait_child_sema);
  }
  printf("%s: exit(%d)\n", cur_thread->name, status);
  thread_exit();
}

void
syscall_SYS_HALT()
{
  shutdown_power_off();
}
