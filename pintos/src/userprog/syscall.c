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
/*****************************/
void halt();
void exit(int status);
pid_t exec(const char* command_line);
int wait(pid_t);
int write(int fd, const void *buffer, unsigned size);
bool check_ptr (const void *usr_ptr);
bool create (const char *file_name, unsigned size);
/*****************************/

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
      f->eax = exec(*(char *)(f->esp+4));
      break;

    case SYS_WAIT:
      f->eax = wait(*(f->esp+4));
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
exit(int status)
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
halt()
{
  shutdown_power_off();
}

/*************************************/
pid_t
exec(const char* command_line)
{
  tid_t tid;
  struct thread *curThread;

  if(!check_ptr(command_line))
  {
    exit(-1);
  }

  curThread = thread_current();

  curThread->child_load_status = 0;
  tid = process_execute(command_line);
  lock_acquire(&curThread->lock_child);

  while(curThread->child_load_status == 0)
  {
    cond_wait(&curThread->cond_child, &curThread->lock_child);
  }
  if(curThread->child_load_status == -1)
  {
    tid = -1;
  }
  lock_release(&curThread->lock_child);
  return tid;
}

int
wait(pid_t process)
{
  return process_wait(process);
}


/*****************************************/

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
