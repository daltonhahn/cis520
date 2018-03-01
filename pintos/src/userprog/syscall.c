#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"


static void syscall_handler (struct intr_frame *);
bool userVirtualAddressIsValid(void *vaddr);
void halt(void);


// Necessary data structures
struct lock fs_lock; // lock for the filesystem


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
  lock_init(&fs_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t esp = f->esp;
  if(userVirtualAddressIsValid(esp) && userVirtualAddressIsValid(esp+1) &&
  userVirtualAddressIsValid(esp+2) && userVirtualAddressIsValid(esp+3))
  {

    switch(esp)
    {
      case SYS_HALT:
        halt();
        break;
      case SYS_EXIT:
        exit(esp + 1);
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

// Called in SYS_EXIT
void
exit(int status)
{
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

// Called in SYS_WRITE
int
write(int fd, const void *buffer, unsigned size)
{
  if (!userVirtualAddressIsValid (buffer)) 
    exit (-1);

  lock_acquire(&fs_lock);
    if (fd == STDIN_FILENO)
    {
      
    }
  lock_release(&fs_lock);
}