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
#include "devices/shutdown.h" /* For use in the halt syscall */

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

// Structs added 
struct lock fs_lock;
struct list open_files;
struct file_descriptor
{
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

// Initializes register, filesystem lock and open_file list
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&fs_lock);
  list_init(&open_files);
}

// Handles all of the system calls
static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t *esp = f->esp;

  // Checks all potential arguments
  if (!check_ptr (esp) || !check_ptr (esp + 1) ||
      !check_ptr (esp + 2) || !check_ptr (esp + 3))
    {
      exit (-1);
    }

  // Switches on the syscall number esp is pointing to
  switch(*esp)
  {
    case SYS_HALT:
      halt();
      break;
    
    case SYS_EXIT:
      exit (*(esp + 1));
      break;

    case SYS_EXEC:
      f->eax = exec((char *) *(esp + 1));
      break;

    case SYS_WAIT:
      f->eax = wait(*(esp + 1));
      break;

    case SYS_CREATE:
      f->eax = create((char *) *(esp + 1), *(esp + 2));
      break;

    case SYS_REMOVE:
      f->eax = remove((char *) *(esp + 1));
      break;

    case SYS_OPEN:
      f->eax = open((char *) *(esp + 1));
      break;

    case SYS_FILESIZE:
      f->eax = filesize(*(esp + 1));
      break;

    case SYS_READ:
      f->eax = read(*(esp + 1), (void *) *(esp + 2), *(esp + 3));
      break;

    case SYS_WRITE:
      f->eax = write(*(esp + 1), (void *) *(esp + 2), *(esp + 3));
      break;

    case SYS_SEEK:
      seek(*(esp + 1), *(esp + 2));
      break;

    case SYS_TELL:
      f->eax = tell(*(esp + 1));
      break;

    case SYS_CLOSE:
      close(*(esp + 1));
      break;
  }
}

void
halt(void)
{
  shutdown_power_off();
}

// From https://github.com/codyjack/OS-pintos/
void 
exit(int status)
{
  struct child_status *child;
  struct thread *cur = thread_current ();
  // Prints exit status
  printf ("%s: exit(%d)\n", cur->name, status);
  struct thread *parent = thread_get_by_id (cur->parent_id);

  // Notify parent we are exiting
  if (parent != NULL) 
    {
      struct list_elem *e = list_tail(&parent->children);
      // Loop through parent's children
      while ((e = list_prev (e)) != list_head (&parent->children))
        {
          child = list_entry (e, struct child_status, elem_child_status);
          // If we find current thread. Set exit status
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

// From https://github.com/codyjack/OS-pintos/
pid_t
exec (const char *cmd_line)
{
  tid_t tid;
  struct thread *cur;
  
  // Validate pointer
  if (!check_ptr (cmd_line))
    {
      exit (-1);
    }

  cur = thread_current ();

  // Executes process
  cur->child_load_status = 0;
  tid = process_execute (cmd_line);
  lock_acquire(&cur->lock_child);
  // Wait for process to finish
  while (cur->child_load_status == 0)
    cond_wait(&cur->cond_child, &cur->lock_child);
  // Check status of executed process
  if (cur->child_load_status == -1)
    tid = -1;
  lock_release(&cur->lock_child);
  return tid;
}

// Waits for a thread to finish
int 
wait (pid_t pid)
{ 
  // process_wait(int) is in process.c
  return process_wait(pid);
}

// Creates file
bool
create (const char *file_name, unsigned size)
{
  bool status;
  // Check pointer
  if (!check_ptr(file_name))
    exit (-1);

  // Create file 
  lock_acquire (&fs_lock);
  status = filesys_create(file_name, size);  
  lock_release (&fs_lock);
  return status;
}

// Removes file
bool 
remove (const char *file_name)
{
  bool status;

  // Check pointer
  if (!check_ptr (file_name))
    exit (-1);
 
  // remove file
  lock_acquire (&fs_lock);  
  status = filesys_remove (file_name);
  lock_release (&fs_lock);
  return status;
}

// From https://github.com/codyjack/OS-pintos/
int
open (const char *file_name)
{
  struct file *f;
  struct file_descriptor *fd;
  int status = -1;
  
  // Check pointer
  if (!check_ptr(file_name))
    exit (-1);

  lock_acquire (&fs_lock); 
  f = filesys_open (file_name);
  // If we can open file, set up file_descriptor struct
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

// From https://github.com/codyjack/OS-pintos/
// Gets size of a file
int
filesize (int fd)
{
  struct file_descriptor *fd_struct;
  int status = -1;
  lock_acquire (&fs_lock); 
  // If file exists get length
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_length (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

// From https://github.com/codyjack/OS-pintos/
// Reads file into buffer
int
read (int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0; 

  // check pointer and check buffer boundary
  if (!check_ptr (buffer) || !check_ptr (buffer + size - 1))
    exit (-1);
  
  if (fd == STDOUT_FILENO)
      return -1;

  if (fd == STDIN_FILENO)
    {
      lock_acquire(&fs_lock);
      uint8_t c;
      unsigned counter = size;
      uint8_t *buf = buffer;
      // loop and read from standard input
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

  // if we make it to here we need to read from a file
  lock_acquire(&fs_lock);
  fd_struct = get_open_file (fd);
  // Reads file if file exists
  if (fd_struct != NULL)
    status = file_read (fd_struct->file_struct, buffer, size);

  lock_release (&fs_lock);
  return status;
}

// From https://github.com/codyjack/OS-pintos/
// Writes to a file from buffer
int
write(int fd, const void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0;

  // Check pointer and buffer
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
      // writes to file if file exists
      if (fd_struct != NULL)
        status = file_write (fd_struct->file_struct, buffer, size);
      lock_release (&fs_lock);
      return status;
  }
}

// Changes next byte to position
void 
seek (int fd, unsigned position)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);

  if (fd_struct != NULL)
    file_seek (fd_struct->file_struct, position);
  lock_release (&fs_lock);
}

// Gets position of next byte
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

// From https://github.com/codyjack/OS-pintos/
// Closes open files
void 
close (int fd)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock); 
  fd_struct = get_open_file (fd);
  // If file exists and this thread owns it, close the file
  if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
    close_open_file (fd);
  lock_release (&fs_lock);
  return ; 
}

/****************
HELPER FUNCTIONS
*****************/

// Allocates file descriptor
// From https://github.com/codyjack/OS-pintos/
int
allocate_fd ()
{
  static int fd_current = 1;
  return ++fd_current;
}

// Verifies that user pointers are valid
bool
check_ptr (const void *usr_ptr)
{
  if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
    {
      return (pagedir_get_page (thread_current()->pagedir, usr_ptr)) != NULL;
    }
  return false;
}

// Gets file_descriptor struct for open files
// From https://github.com/codyjack/OS-pintos/
struct file_descriptor *
get_open_file (int fd)
{
  struct list_elem *e;
  struct file_descriptor *fd_struct; 

  // Loops through open files and returns the match
  e = list_tail (&open_files);
  while ((e = list_prev (e)) != list_head (&open_files)) 
    {
      fd_struct = list_entry (e, struct file_descriptor, elem);
      if (fd_struct->fd_num == fd)
	      return fd_struct;
    }
  return NULL;
}


// Closes open files
// From https://github.com/codyjack/OS-pintos/
void
close_open_file (int fd)
{
  struct list_elem *e;
  struct list_elem *prev;
  struct file_descriptor *fd_struct; 
  e = list_end (&open_files);
  // Loops through file list and frees the file
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

// Closes files that belong to a thread
// From https://github.com/codyjack/OS-pintos/
void
close_file_by_owner (tid_t tid)
{
  struct list_elem *e;
  struct list_elem *next;
  struct file_descriptor *fd_struct; 
  e = list_begin (&open_files);
  // Loops through file list and removes those owned by tid thread
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