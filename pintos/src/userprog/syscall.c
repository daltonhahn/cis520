#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"


static void syscall_handler (struct intr_frame *);
bool userVirtualAddressIsValid(void *vaddr);
void halt(void);
void exit(int status);
int write(int fd, const void *buffer, unsigned size);
int read (int fd, void *buffer, unsigned size);


// Necessary data structures
struct lock fs_lock; // lock for the filesystem
struct list open_files;
struct file_descriptor
{
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

struct file_descriptor *
get_open_file (int fd)
{
  struct list_elem *e;
  struct file_descriptor *fd_struct; 
  e = list_tail (&open_files);
  while ((e = list_prev (e)) != list_head (&open_files)) 
    {
      fd_struct = list_entry (e, struct file_descriptor, elem);
      if (fd_struct->fd_num == fd)
	      return fd_struct;
    }
  return NULL;
}


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
  list_init(&open_files);
}

static void
syscall_handler (struct intr_frame *f) 
{
  putchar(26);
  uint32_t *esp;
  esp = f->esp;
  if(!userVirtualAddressIsValid(esp) || !userVirtualAddressIsValid(esp+1) ||
  !userVirtualAddressIsValid(esp+2) || !userVirtualAddressIsValid(esp+3))
  {
    exit (-1);
  }
  else
  {
    //printf("all pointers are good\n");
    int syscall = *esp;
    switch(syscall)
    {
      case SYS_HALT:
        
        halt();
        break;
      case SYS_EXIT:
        exit(*(esp + 1));
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
      //printf("Recieved SYS_WRITE\n");
        //f->eax = write(esp+1, esp+2, esp+3);
        f->eax = write (esp + 1, (void *) esp + 2, esp + 3);
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
      printf("default has been reached");
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
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

// Called in SYS_WRITE
int
write(int fd, const void *buffer, unsigned size)
{
  int status = 0;
  struct file_descriptor *fd_struct;

  if (!userVirtualAddressIsValid (buffer) || !userVirtualAddressIsValid(buffer + size - 1)) 
  {
    exit (-1);
  }

  lock_acquire(&fs_lock);

  if (fd == STDIN_FILENO)
  {
    status = -1;
  }
  else if (fd == STDOUT_FILENO)
  {
    putbuf(buffer, size);
    status = size;
  }
  else
  {
    fd_struct = get_open_file(fd);
    if (fd_struct != NULL)
      status = file_write(fd_struct->file_struct, buffer, size);
  }
  lock_release(&fs_lock);

  return status;
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0; 

  if (!userVirtualAddressIsValid (buffer) || !userVirtualAddressIsValid (buffer + size - 1))
    exit (-1);

  lock_acquire (&fs_lock); 
  
  if (fd == STDOUT_FILENO)
    {
      lock_release (&fs_lock);
      return -1;
    }

  if (fd == STDIN_FILENO)
    {
      uint8_t c;
      unsigned counter = size;
      uint8_t *buf = buffer;
      while (counter > 1 && (c = input_getc()) != 0)
        {
          *buf = c;
          buffer++;
          counter--; 
        }
      *buf = 0;
      lock_release (&fs_lock);
      return (size - counter);
    } 
  
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_read (fd_struct->file_struct, buffer, size);

  lock_release (&fs_lock);
  return status;
}

// int
// read(int fd, void *buffer, unsigned size)
// {
//   lock_acquire (&fs_lock);   
//   if (fd == STDOUT_FILENO)
//       status = -1;
//   else if (fd == STDIN_FILENO)
//     {
//       uint8_t c;
//       unsigned counter = size;
//       uint8_t *buf = buffer;
//       while (counter > 1 && (c = input_getc()) != 0)
//         {
//           *buf = c;
//           buffer++;
//           counter--; 
//         }
//       *buf = 0;
//       status = size - counter;
//     }
//   else 
//     {
//       fd_struct = get_open_file (fd);
//       if (fd_struct != NULL)
// 	status = file_read (fd_struct->file_struct, buffer, size);
//     }
//   lock_release (&fs_lock);
//   return status;
// }