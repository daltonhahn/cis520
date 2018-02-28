#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"


static void syscall_handler (struct intr_frame *);
bool userVirtualAddressIsValid(void *vaddr);
void halt(void);


// Call to check if user program virtual address is valid
bool
userVirtualAddressIsValid(void *vaddr)
{
  if (vaddr == NULL) return false;
  if (!is_user_vaddr(vaddr)) return false;
  if (pagedir_get_page (thread_current()->pagedir, vaddr) == NULL) return false;
  return true;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t sys_call = f->esp;
  if(userVirtualAddressIsValid(sys_call))
  {

    switch(sys_call)
    {
      case SYS_HALT:
        halt();
        break;
      case SYS_EXIT:
        printf("%s: exit(%d)\n", "Name?", f->error_code);
        break;
      case SYS_EXEC:
      printf("Recieved SYS_EXEC\n");
        break;
      case SYS_WAIT:
      printf("Recieved SYS_WAIT\n");
        break;
      case SYS_CREATE:
      printf("Recieved SYS_CREATE\n");
        break;
      case SYS_REMOVE:
      printf("Recieved SYS_REMOVE\n");
        break;
      case SYS_OPEN:
      printf("Recieved SYS_OPEN\n");
        break;
      case SYS_FILESIZE:
      printf("Recieved SYS_FILESIZE\n");
        break;
      case SYS_READ:
      printf("Recieved SYS_READ\n");
        break;
      case SYS_WRITE:
      printf("Recieved SYS_WRITE\n");
        break;
      case SYS_SEEK:
      printf("Recieved SYS_SEEK\n");
        break;
      case SYS_TELL:
      printf("Recieved SYS_TELL\n");
        break;
      case SYS_CLOSE:
      printf("Recieved SYS_CLOSE\n");
        break;
      default:
        break;
    }
  }
}

// Called in SYS_HALT
void
halt(void)
{
  shutdown_power_off();
}
