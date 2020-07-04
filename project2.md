# Project 2 Design Document


> Spring 2020

## GROUP

- Zhuoyang Ye <yezhuoyang@sjtu.edu.cn>


## PROJECT PARTS


#### DATA STRUCTURES

We add some extra data structures to support system calls.

```c
struct thread{
    uint32_t exit_status; // Store the exit code
    struct semaphore be_waited; // Whether the thread is waited by the parent thread
    
    struct list child_list; // The child threads of this thread
    struct list_elem child_elem;
    
    struct semaphore exit_sem; // Used by the parent thread to control the exit process of the child thread
	bool to_exit;	// Whether the thread is ready to exit. It is possible that parent thread is waiting for it, so we just mark it with 'exit' but it will not exit immediately.
    
    struct list file_descriptors; // The file descriptors 
    unsigned fd_index; // The index of the file descriptor (to be used in the next file_descriptor)
    
    struct file * prog_file; // The file pointer of the program executable.
}

struct file_descriptor
  {
    int fd;
    char name[16];
    struct file * file_pointer;
    struct list_elem elem;
  };

// Because the function start_process accept only one argument, so we create a new struct to pass the arguments, which are neccessary to create a new process.
struct proc_init {
    char * name;
    struct semaphore init_sem; // Only when the program is loaded properly can we start the process
    bool success;
};

static struct lock exit_lock; // Avoid modifying the list of threads when it is being read.
```


#### ALGORITHMS

**get the use data**

To get the user data,  we just read the data from the address *ESP* + 4 and above it, where *ESP* is stored in the *intr_frame*. And we check whether the address is valid.

**wait**

To implement wait, we added several data structures as mentioned before. 

This part is mainly implemented in the function *process_wait*. We just find the child thread and then remove it from the child thread list (because we can't wait for one child thread for twice). And we "down" the semaphore *be_waited* of the child thread, and the parent thread is blocked. After the child thread calls *exit* , *be_waited* is "up"ed again and the parent thread can get the exit code of the child. To ensure that the parent thread gets the exit code before the space of the child thread is deleted, we "down" the *exit_sem* in the *thread_exit* and "up" it again after the parent get the exit code.

