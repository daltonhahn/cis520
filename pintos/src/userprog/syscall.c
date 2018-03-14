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
void halt();
void exit(int status);
int write(int fd, const void *buffer, unsigned size);
bool check_ptr (const void *usr_ptr);
bool create (const char *file_name, unsigned size);

/***********
Structs
***********/
struct lock fs_lock;

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
  lock_init(&fs_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{

  if (!check_ptr (f->esp) || !check_ptr (f->esp + 1) ||
      !check_ptr (f->esp + 2) || !check_ptr (f->esp + 3))
  {
    exit(-1);
  }

  switch(*(int*)f->esp)
  {
    case SYS_HALT:
      halt();
      break;
    
    case SYS_EXIT:
      exit(*(int *)(f->esp+4));
      break;

    case SYS_EXEC:
      //printf("NOT IMPLEMENTED YET - SYS_EXEC\n");
      break;

    case SYS_WAIT:
      //printf("NOT IMPLEMENTED YET - SYS_WAIT\n");
      break;

    case SYS_CREATE:
      create ((char *)(f->esp+4), (unsigned *)(f->esp+8));
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
      f->eax = write(*(int *)(f->esp+4), (void *)*(uint32_t *)(f->esp+8), *(unsigned *)(f->esp+12));
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

int
write(int fd, const void *buffer, unsigned size)
{

  if (!check_ptr(buffer) || !check_ptr(buffer + size - 1))
    exit (-1);

  switch(fd)
  {
    case STDIN_FILENO:
      return -1;

    case STDOUT_FILENO:
      putbuf(buffer, size);
      return size;
      break;

    default:
      printf("Write Syscall");
      return -1;
      break;
  }
}

void
exit (int status)
{
  /* later on, we need to determine if there is process waiting for it */
  /* process_exit (); */
  struct child_status *child;
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  struct thread *parent = thread_get_by_id (cur->parent_id);
  if (parent != NULL) 
    {
      struct list_elem *e = list_tail(&parent->children);
      while ((e = list_prev (e)) != list_head (&parent->children))
        {
          child = list_entry (e, struct child_status, elem_child_status);
          if (child->child_id == cur->tid)
          {
            lock_acquire (&parent->lock_child);
            child->is_exit_called = true;
            child->child_exit_status = status;
            lock_release (&parent->lock_child);
          }
        }
    }
  thread_exit ();
}

void
halt()
{
  shutdown_power_off();
}

bool
create (const char *file_name, unsigned size)
{
  bool status;

  if (!check_ptr(file_name))
    exit (-1);

  lock_acquire (&fs_lock);
  status = filesys_create(file_name, size);  
  lock_release (&fs_lock);
  return status;
}