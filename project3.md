# Project 2 Design Document


> Spring 2020

## GROUP

- Zhuoyang Ye <yezhuoyang@sjtu.edu.cn>



## PROJECT PARTS

### A. PAGE TABLE MANAGEMENT  

#### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, `global` or `static` variable, `typedef`, or
> enumeration.  Document the purpose of each in 25 words or less.

```C
// page.h
struct spt_entry
  {
    enum page_type type;
    void *addr;
    bool pinned;
    bool writeable;
    bool is_present;
    
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;

    uint32_t swap_index;
    
    struct hash_elem elem;
  };

extern struct lock filesystem_lock;

// frame.h
struct frame_entry
  {
    void *frame_addr;
    struct thread *owner_thread;
    struct spt_entry *spte;
    //struct hash_elem elem;
    struct list_elem elem;
  };

struct lock frame_table_lock;
struct frame_entry **frame_table_index;
struct list frame_table;
```

#### ALGORITHMS

> A2: In a few paragraphs, describe your code for accessing the data
> stored in the SPT about a given page.


When page fault occurs or we want to evict a page out, we need to access the data stored in SPT about a given address. We implement SPT by hash table using address as index, so we can directly search the address for the corresponding SPT data.

#### SYNCHRONIZATION

> A4: When two user processes both need a new frame at the same time,
> how are races avoided?

I add a new lock called frame-table lock to avoid races conditions. 

### B. PAGING TO AND FROM DISK

#### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

```c
struct lock swap_lock;
struct bitmap *swap_map;
struct block *swap_block_device;
```

#### ALGORITHMS

> B2: When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.


When frame is not pinned and not accessed recently, we can evict it. We record the type of each frame in SPT. If it comes from a file and is dirty, we write it back. Otherwise we dump it into swap device.



### C. MEMORY MAPPED FILES

#### DATA STRUCTURES

> C1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

```C
struct mmap_entry
  {
    struct spt_entry *spte;
    int mapid;
    struct list_elem elem;
  };
```
