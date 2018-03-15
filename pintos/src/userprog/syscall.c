#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "devices/input.h"

/***********/
#include "devices/shutdown.h" /* For use in the halt syscall */
/***********/

static void syscall_handler (struct intr_frame *);
void halt();
void exit(int status);
int write(int fd, const void *buffer, unsigned size);
bool check_ptr (const void *usr_ptr);
bool create (const char *file_name, unsigned size);
int open (const char *file_name);
struct file_descriptor *get_open_file (int);
int read (int, void *, unsigned);
void close (int fd);
pid_t exec (const char *);
int filesize (int);
int wait (pid_t pid);

static void close_open_file (int);
void close_file_by_owner (tid_t);

/***********
Structs
***********/
struct lock fs_lock;
struct list open_files;
struct file_descriptor
{
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

int
allocate_fd ()
{
  static int fd_current = 1;
  return ++fd_current;
}

bool
check_ptr (const void *usr_ptr)
{
  if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
    {
      return (pagedir_get_page (thread_current()->pagedir, usr_ptr)) != NULL;
    }
  return false;
}

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

void
close_open_file (int fd)
{
  struct list_elem *e;
  struct list_elem *prev;
  struct file_descriptor *fd_struct; 
  e = list_end (&open_files);
  while (e != list_head (&open_files)) 
    {
      prev = list_prev (e);
      fd_struct = list_entry (e, struct file_descriptor, elem);
      if (fd_struct->fd_num == fd)
	{
	  list_remove (e);
          file_close (fd_struct->file_struct);
	  free (fd_struct);
	  return ;
	}
      e = prev;
    }
  return ;
}

void
close_file_by_owner (tid_t tid)
{
  struct list_elem *e;
  struct list_elem *next;
  struct file_descriptor *fd_struct; 
  e = list_begin (&open_files);
  while (e != list_tail (&open_files)) 
    {
      next = list_next (e);
      fd_struct = list_entry (e, struct file_descriptor, elem);
      if (fd_struct->owner == tid)
	{
	  list_remove (e);
	  file_close (fd_struct->file_struct);
          free (fd_struct);
	}
      e = next;
    }
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
      f->eax = exec((char *)(f->esp+4));
      break;

    case SYS_WAIT:
      //printf("NOT IMPLEMENTED YET - SYS_WAIT\n");
      f->eax = wait((unsigned *)(f->esp+4));
      break;

    case SYS_CREATE:
      f->eax = create ((char *)(f->esp+4), (unsigned *)(f->esp+8));
      break;

    case SYS_REMOVE:
      //printf("NOT IMPLEMENTED YET - SYS_REMOVE\n");
      break;

    case SYS_OPEN:
      //printf("NOT IMPLEMENTED YET - SYS_OPEN\n");
      break;

    case SYS_FILESIZE:
      //printf("NOT IMPLEMENTED YET - SYS_FILESIZE\n");
      f->eax = filesize(*(int *)(f->esp+4));
      break;

    case SYS_READ:
      //printf("NOT IMPLEMENTED YET - SYS_READ\n");
      f->eax = read(*(int *)(f->esp+4), (void *)*(uint32_t *)(f->esp+8), *(unsigned *)(f->esp+12));
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
      close(*(int *)(f->esp+4));
      break;

    default:
      //printf("Default SYSCALL Catcher");
      break;
  }
}

int
write(int fd, const void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0;

  if (!check_ptr(buffer) || !check_ptr(buffer + size - 1))
    exit (-1);

  switch(fd)
  {
    case STDIN_FILENO:
      return -1;

    case STDOUT_FILENO:
      putbuf(buffer, size);
      return size;
    default:
      //need to write to file.
      lock_acquire (&fs_lock); 
      fd_struct = get_open_file (fd);
      if (fd_struct != NULL)
        status = file_write (fd_struct->file_struct, buffer, size);
      lock_release (&fs_lock);
      return status;
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

int
open (const char *file_name)
{
  struct file *f;
  struct file_descriptor *fd;
  int status = -1;
  
  if (!check_ptr(file_name))
    exit (-1);

  lock_acquire (&fs_lock); 
 
  f = filesys_open (file_name);
  if (f != NULL)
    {
      fd = calloc (1, sizeof *fd);
      fd->fd_num = allocate_fd ();
      fd->owner = thread_current ()->tid;
      fd->file_struct = f;
      list_push_back (&open_files, &fd->elem);
      status = fd->fd_num;
    }
  lock_release (&fs_lock);
  return status;
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

int
read (int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0; 

  if (!check_ptr (buffer) || !check_ptr (buffer + size - 1))
    exit (-1);

  
  if (fd == STDOUT_FILENO)
    {
      return -1;
    }

  if (fd == STDIN_FILENO)
    {
      lock_acquire(&fs_lock);
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
  lock_acquire(&fs_lock);
  
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_read (fd_struct->file_struct, buffer, size);

  lock_release (&fs_lock);
  return status;
}

void 
close (int fd)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
    close_open_file (fd);
  lock_release (&fs_lock);
  return ; 
}

pid_t
exec (const char *cmd_line)
{
  /* a thread's id. When there is a user process within a kernel thread, we
   * use one-to-one mapping from tid to pid, which means pid = tid
   */
  tid_t tid;
  struct thread *cur;
  /* check if the user pinter is valid */
  if (!check_ptr (cmd_line))
    {
      exit (-1);
    }

  cur = thread_current ();

  cur->child_load_status = 0;
  tid = process_execute (cmd_line);
  lock_acquire(&cur->lock_child);
  while (cur->child_load_status == 0)
    cond_wait(&cur->cond_child, &cur->lock_child);
  if (cur->child_load_status == -1)
    tid = -1;
  lock_release(&cur->lock_child);
  return tid;
}

int
filesize (int fd)
{
  struct file_descriptor *fd_struct;
  int status = -1;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_length (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

int 
wait (pid_t pid)
{ 
  return process_wait(pid);
}