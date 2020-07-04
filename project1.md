# Project 1 Design Document

> Spring 2020

## GROUP

- Zhuoyang Ye <yezhuoyang@sjtu.edu.cn>

## PROJECT PARTS

#### DATA STRUCTURES

```c
// The list of all sleeping threads
static struct list sleeping_threads;

struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority, which may be different from original priority because of donation */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

    /* Owned by timer.c */
    int64_t wake_up_ticks;
    struct list_elem sleepelem;         /* List element for sleeping threads list. */
    struct semaphore sleep_semaphore;   /* semaphore to control thread sleeping*/

    /* Priority donation use */
    int original_priority;       
    struct list locks;                  /* The locks current thread hold */      
    struct lock *waiting_lock;      /* The thread current thread is waiting for */
  };

/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    struct list_elem elem;      /* My Implementation : Owned by threads that hold this lock */
    int max_priority;           /* My Implementation : Max priority among threads wait for this lock*/
  };
```

### A. ALARM CLOCK  

#### ALGORITHMS

> A1: Briefly describe the algorithmic flow for a call to `timer_sleep()`,
> including the effects of the timer interrupt handler.

When timer_sleep is called, the current thread is put into a sleeping thread queue ordered by wake up ticks(With interrupts turned off).
Then, let the current semaphore wait for the sleep_semaphore.
The timer interrupt handler checks the sleeping thread queue and wakes up all threads which are ready by signaling the  semaphore.


> A2: What steps are taken to minimize the amount of time spent in
> the timer interrupt handler?

The sleeping thread queue is ordered by wake up ticks.


#### SYNCHRONIZATION

> A3: How are race conditions avoided when multiple threads call
> `timer_sleep()` simultaneously? How are race conditions avoided when a timer interrupt occurs
> during a call to `timer_sleep()`?

The interrupts are turned off while inserting a thread into sleeping thread queue.

### PRIORITY SCHEDULING

#### ALGORITHMS

> B1: How do you ensure that the highest priority thread waiting for
> a lock, semaphore, or condition variable wakes up first?

I add a list of threads called waiters to the struct semaphore. When a thread calls sema_down() to a certain semaphore , the thread is added to 
the waiters ordered by it's priority.  


> B2: Describe the sequence of events when a call to `lock_acquire()`
> causes a priority donation.  How is nested donation handled?


1.  If the lock is held by another thread, we update the priority of all the threads on the waiting chain with the priority of current thread.
2.  Down the semaphore. If the lock is held by another thread, current thread will go to sleep. Otherwise it will successfully down the semaphore.
3.  Modify the holder of the lock, and the lock into current threads' holding list, and update the priority of current thread.

> B3: Describe the sequence of events when `lock_release()` is called
> on a lock that a higher-priority thread is waiting for.

1.  Turn off interrupts and remove the lock from the holding list of current thread, and update the priority of current thread.
2.  Up the semaphore and turn on the interrupts.
