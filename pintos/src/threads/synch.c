/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
/*****************************************************************/
// Changed the insertion into the list to use priority comparison to order them based on priority.
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_insert_ordered(&sema->waiters, &thread_current()->elem, thread_priority_compare, NULL); // Inserting in order of priority rather than just at the back.
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
/*****************************************************************/

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/*****************************************************************/
// Added notification of next thread in list to unblock.
// Re-sorts the list after a thread runs in case of donation reverting, priority updating, etc.
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  bool check_priority = false; 		// check if a thread should be rechecked with the current thread 

  struct thread *next_thread;

  ASSERT (sema != NULL);

  old_level = intr_disable (); 		// don't allow interrupts during thread search and unblocking 
  if (!list_empty (&sema->waiters))
  {
    list_sort(&sema->waiters, thread_priority_compare, NULL); 	// sort list for updated priorities 

    next_thread = list_entry (list_pop_front (&sema->waiters), struct thread, elem); // pull next thread
    thread_unblock (next_thread);				// unblock new thread
    check_priority = true;					// recheck priority against other threads
  }
  sema->value++;

  if (check_priority && !intr_context())			// check if we need to give up to higher priority thread
  {
    thread_priority_check(next_thread);
  }
  intr_set_level (old_level);
}
/*****************************************************************/

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/*****************************************************************/
// Added priority donation here.  Checks the highest priority of a thread holding a lock,
// if lower than the current thread (waiter), donate forward (hold this value in a list).
// ********** NEED MORE COMMENTS *********
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  int max_pri = lock->holder->priority;		// finds highest priority of the thread with the lock

  if(lock->semaphore.value == 0)		// begin donation check if someone holds the lock
  {
    // check if a thread's donated priority is higher than normal priority,
    // if so, reset highest priority to donated priority value
    if(!list_empty(&lock->holder->donated_priorities))	
    {						
      max_pri = list_entry(list_front(&lock->holder->donated_priorities), struct thread, pri_elem)->priority;
    }

    // if the current thread has a higher priority than the lock holder, begin donation process
    // DONATE PRIORITY CASE
    if(thread_get_priority() > max_pri)
    {					

      // Currently running thread is going to donate it's priority to lock holder,
      // it's recipient list must be updated to include the lock holder's old priority in order to be able to revert
      list_insert_ordered(&thread_current()->priority_recipients, &lock->holder->recp_elem, thread_priority_compare_donated, NULL);

      // Lock holder is going to update it's priority with the currently running thread's priority
      // Lock holder needs to update it's donated list with the currently running thread's priority
      if(!list_empty(&thread_current()->priority_recipients))
      {
        for (struct list_elem *e = list_begin(&thread_current()->priority_recipients); e != list_end(&thread_current()->priority_recipients); e = list_next(e))
        {
          struct thread *cur_thread = list_entry (e, struct thread, recp_elem);
          if(!list_empty(&cur_thread->priority_recipients)) {
            for (struct list_elem *ee = list_begin(&cur_thread->priority_recipients); ee != list_end(&cur_thread->priority_recipients); ee = list_next(ee))
            {
              struct thread *child_thread = list_entry (ee, struct thread, recp_elem);
              list_insert_ordered(&child_thread->donated_priorities, &thread_current()->pri_elem, thread_priority_compare_donated, NULL);
            }
          }
          list_insert_ordered(&cur_thread->donated_priorities, &thread_current()->pri_elem, thread_priority_compare_donated, NULL);
        }
      }

      // reschedules threads according to newly donated/newly set priorities
      thread_set_priority(thread_current()->priority);
    }
  }
  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
}
/*****************************************************************/

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
/*****************************************************************/
// Check to see if there are more threads waiting on the lock,
// if so, we need to possibly revert donated priorities back to 
// original values.  After that check, release the lock.
void
lock_release (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  // check the list of waiters for a lock
  // if there are waiters, we need to check for donation reversion
  if (!list_empty(&lock->semaphore.waiters))
  {
	  
    // look through the list of waiters on the lock,
    // if a thread is in the waiters list and in a threads donation list
    // if they are present, we must remove their donation
    for (struct list_elem *e = list_begin(&lock->semaphore.waiters); e != list_end(&lock->semaphore.waiters); e = list_next(e))
    {
      struct thread *parent_thread = list_entry (e, struct thread, elem);
      if(!list_empty(&thread_current()->donated_priorities))
      {
        for (struct list_elem *ee = list_begin(&thread_current()->donated_priorities); ee != list_end(&thread_current()->donated_priorities); ee = list_next(ee))
        {
          if (&parent_thread->pri_elem == ee)
          {
            list_remove(ee);
            break;
          }
        }
      }
    }
  }

  // clear the lock and call semaphore_up to make it available again
  lock->holder = NULL;
  sema_up (&lock->semaphore);
}
/*****************************************************************/

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/*****************************************************************/
// ********** NEED MORE COMMENTS **************
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters))
  {
    // sort the list of threads waiting on a condition
    // once sorted, threads will operate in a priority order with regard to condition variables
    list_sort(&cond->waiters, conditional_priority_compare, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}
/*****************************************************************/

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

/* Compares the priority of threads that are waiting on a condition variable. */
/*****************************************************************/
// Order lists of threads waiting on condition variables by their assigned priority
bool
conditional_priority_compare(const struct list_elem *left, const struct list_elem *right, void *aux UNUSED)
{
  // pull out semaphore object from two list_elems of a condition variable's waiter list
  struct semaphore_elem *left_sema = list_entry(left, struct semaphore_elem, elem);
  struct semaphore_elem *right_sema = list_entry(right, struct semaphore_elem, elem);

  // pass these two semaphores to priority compare to order list
  return (thread_priority_compare(list_front(&left_sema->semaphore.waiters),list_front(&right_sema->semaphore.waiters), NULL));
}
/*****************************************************************/
