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
#include <string.h>

/***********/
#include "devices/shutdown.h" /* For use in the halt syscall */
/***********/

static void syscall_handler (struct intr_frame *);
void halt(void);
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
bool remove (const char *file_name);
void seek (int fd, unsigned position);
unsigned tell (int fd);

static int allocate_fd(void);
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
  uint32_t *esp;
  esp = f->esp;

  if (!check_ptr (esp) || !check_ptr (esp + 1) ||
      !check_ptr (esp + 2) || !check_ptr (esp + 3))
    {
      exit (-1);
    }

  int syscall = *esp;
  switch(syscall)
  {
    case SYS_HALT:
      halt();
      break;
    
    case SYS_EXIT:
      exit (*(esp + 1));
      break;

    case SYS_EXEC:
      f->eax = exec ((char *) *(esp + 1));
      break;

    case SYS_WAIT:
      f->eax = wait (*(esp + 1));
      break;

    case SYS_CREATE:
      f->eax = create ((char *) *(esp + 1), *(esp + 2));
      break;

    case SYS_REMOVE:
      f->eax = remove ((char *) *(esp + 1));
      break;

    case SYS_OPEN:
      f->eax = open ((char *) *(esp + 1));
      break;

    case SYS_FILESIZE:
      f->eax = filesize (*(esp + 1));
      break;

    case SYS_READ:
      f->eax = read (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
      break;

    case SYS_WRITE:
      f->eax = write (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
      break;

    case SYS_SEEK:
      seek (*(esp + 1), *(esp + 2));
      break;

    case SYS_TELL:
      f->eax = tell (*(esp + 1));
      break;

    case SYS_CLOSE:
      close (*(esp + 1));
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
  struct child_status *child;
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  struct thread *parent = thread_get_by_id (cur->parent_id);
  if (parent != NULL) 
    {
      struct list_elem *e = list_tail(&parent->children);
      while ((e = list_prev (e)) != list_head (&parent->children))
        {
          child = list_entry (e, struct child_status, elem_status);
          if (child->child_id == cur->tid)
          {
            lock_acquire (&parent->lock_child);
            child->exit_call= true;
            child->status = status;
            lock_release (&parent->lock_child);
          }
        }
    }
   thread_exit ();
}

void
halt(void)
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

  if (!check_ptr(file_name) || strcmp(file_name, "") == 0)
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

bool 
remove (const char *file_name)
{
  bool status;
  if (!check_ptr (file_name))
    exit (-1);
 
  lock_acquire (&fs_lock);  
  status = filesys_remove (file_name);
  lock_release (&fs_lock);
  return status;
}

void 
seek (int fd, unsigned position)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    file_seek (fd_struct->file_struct, position);
  lock_release (&fs_lock);
  return ;
}
 
unsigned 
tell (int fd)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_tell (fd_struct->file_struct);
 
  lock_release (&fs_lock);
  return status;
}
