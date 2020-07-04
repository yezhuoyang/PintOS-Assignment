#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "pagedir.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/input.h"
#include "devices/shutdown.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/exception.h"

static void syscall_handler(struct intr_frame *);


static int get_syscall_type(struct intr_frame *);
static void get_syscall_arg(struct intr_frame *, uint32_t *, int);


static bool sys_create(const char *file, unsigned initial_size);
static bool sys_remove(const char *file);
static int sys_open(const char *file);
static void sys_close(int fd);
void sys_exit(int status);
static int sys_write(int fd, const void *buffer, unsigned size);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_filesize(int fd);
static pid_t sys_exec(const char *cmd_line);
static int sys_wait(pid_t pid);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_halt(void);
static int sys_mmap(int fd, void *addr);
static void sys_munmap(int map);

struct lock filesystem_lock;

static struct spt_entry *
check_and_pin_addr(void *addr, void *esp)
{
    struct spt_entry *spte=get_spte(addr);
    if(spte != NULL)
    {
        spt_load(spte);
    }
    else
    {
        if (is_stack_growth(addr,esp))
        {
            if (!spt_stack_growth(addr))
                sys_exit (-1);
        }
        else
        {
            sys_exit (-1);
        }
    }
    return spte;
}

static void
check_and_pin_buffer(void *uaddr, unsigned int len, void *esp, bool write)
{
    for(const void *addr = uaddr; addr < uaddr + len; ++addr)
    {
        if(!is_valid_user_addr (addr)) sys_exit (-1);
        struct spt_entry *spte = check_and_pin_addr(addr, esp);
        if(spte != NULL && write && !spte->writeable) sys_exit (-1);
    }
}

static void
check_and_pin_string(const void *str, void *esp)
{
    check_and_pin_addr(str, esp);
    while(*(char *)str != 0)
    {
        str = (char *)str + 1;
        check_and_pin_addr(str, esp);
    }
}

static void
unpin_addr (void *addr)
{
    struct spt_entry *spte = get_spte (addr);
    if (spte != NULL)
        spte->pinned = false;
}

static void
unpin_buffer (void *uaddr, unsigned int len)
{
    for (void *addr = uaddr; addr < uaddr + len; ++addr)
        unpin_addr (addr);
}

static void
unpin_string (void *str)
{
    unpin_addr(str);
    while (*(char *)str != 0)
    {
        str = (char *)str + 1;
        unpin_addr(str);
    }
}

static void
valid_uaddr(const void * uaddr, unsigned int len)
{
    for(const void * addr = uaddr; addr < uaddr + len ; ++addr)
        if ((!addr) ||!(is_user_vaddr (addr)) ||get_spte(addr) == NULL)
        {
            sys_exit(-1);
            return;
        }
}

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesystem_lock);
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    uint32_t syscall_args[4];
    int type = get_syscall_type(f);
    check_and_pin_addr((const void *)f->esp, f->esp);
    switch (type)
    {
        case SYS_CREATE:
            get_syscall_arg(f, syscall_args, 2);
            check_and_pin_string ((const void *)syscall_args[0], f->esp);
            f->eax = sys_create((char *)syscall_args[0], syscall_args[1]);
            ASSERT(is_valid_user_addr (syscall_args[0]));
            unpin_string((void *)syscall_args[0]);
            break;
        case SYS_REMOVE:
            get_syscall_arg(f, syscall_args, 1);
            check_and_pin_string((const void *)syscall_args[0], f->esp);
            f->eax=sys_remove((char *)syscall_args[0]);
            break;
        case SYS_OPEN:
            get_syscall_arg(f, syscall_args, 1);
            check_and_pin_string ((const void *)syscall_args[0], f->esp);
            f->eax = sys_open((char *)syscall_args[0]);
            ASSERT (is_valid_user_addr (syscall_args[0]));
            unpin_string ((void *)syscall_args[0]);
            break;
        case SYS_CLOSE:
            get_syscall_arg(f, syscall_args, 1);
            sys_close(syscall_args[0]);
            break;
        case SYS_EXIT:
            get_syscall_arg(f, syscall_args, 1);
            sys_exit(syscall_args[0]);
            break;
        case SYS_WRITE:
            get_syscall_arg(f, syscall_args, 3);
            check_and_pin_buffer ((void *)syscall_args[1], syscall_args[2], f->esp, false);
            f->eax = sys_write(syscall_args[0], (void *)syscall_args[1], syscall_args[2]);
            unpin_buffer ((void *)syscall_args[1], syscall_args[2]);
            break;
        case SYS_READ:
            get_syscall_arg(f, syscall_args, 3);
            check_and_pin_buffer ((void *)syscall_args[1], syscall_args[2], f->esp, true);
            f->eax = sys_read(syscall_args[0], (void *)syscall_args[1], syscall_args[2]);
            unpin_buffer ((void *)syscall_args[1], syscall_args[2]);
            break;
        case SYS_FILESIZE:
            get_syscall_arg(f, syscall_args, 1);
            f->eax = sys_filesize(syscall_args[0]);
            break;
        case SYS_EXEC:
            get_syscall_arg(f, syscall_args, 1);
            check_and_pin_string ((const void *)syscall_args[0], f->esp);
            f->eax = sys_exec((char *)syscall_args[0]);
            ASSERT (is_valid_user_addr (syscall_args[0]));
            unpin_string ((void *)syscall_args[0]);
            break;
        case SYS_WAIT:
            get_syscall_arg(f, syscall_args, 1);
            f->eax = sys_wait(syscall_args[0]);
            break;
        case SYS_SEEK:
            get_syscall_arg(f, syscall_args, 2);
            sys_seek(syscall_args[0], syscall_args[1]);
            break;
        case SYS_TELL:
            get_syscall_arg(f, syscall_args, 1);
            f->eax = sys_tell(syscall_args[0]);
            break;
        case SYS_MMAP:
            get_syscall_arg(f, syscall_args, 2);
            f->eax = sys_mmap(syscall_args[0], (void *)syscall_args[1]);
            break;
        case SYS_MUNMAP:
            get_syscall_arg(f, syscall_args, 1);
            sys_munmap(syscall_args[0]);
            break;
        case SYS_HALT:
            sys_halt();
            break;
    }
    unpin_addr (f->esp);
}


/* Get arguments which have been pushed into stack in lib/user/syscall.c */
static void
get_syscall_arg(struct intr_frame *f, uint32_t *buffer, int argc)
{
    uint32_t *ptr;
    for (ptr=(uint32_t *)f->esp + 1; argc > 0; ++buffer, --argc, ++ptr)
    {
        check_and_pin_addr (ptr, sizeof(uint32_t));
        *buffer = *ptr;
    }
}

/* Get the type of system call */
static int
get_syscall_type(struct intr_frame *f)
{
    valid_uaddr(f->esp, sizeof(uint32_t));
    return *((uint32_t *)f->esp);
}

static void
valid_string(const char *str)
{
    const char *ptr;
    for(ptr = str; valid_uaddr(ptr, 1), *ptr; ++ptr);
    valid_uaddr(ptr, 1);
}

static struct file_descriptor *
get_fdstruct(int fd)
{
    struct list_elem *e;

    for(e = list_begin(&thread_current()->file_descriptors);
         e != list_end(&thread_current()->file_descriptors);
         e = list_next(e))
    {
        struct file_descriptor *f = list_entry(e, struct file_descriptor, elem);
        if (f->fd == fd)
            return f;
    }
    return NULL;
}

/* Implements of syscalls */
static bool
sys_create(const char *file, unsigned initial_size)
{

    /* Check the string */
    valid_string(file);

    /* Call filesys */
    lock_acquire(&filesystem_lock);
    bool success= filesys_create(file, initial_size);
    lock_release(&filesystem_lock);
    return success;
}

static bool
sys_remove(const char *file)
{
    /* Check file name */
    valid_string(file);

    lock_acquire(&filesystem_lock);
    bool success = filesys_remove(file);
    lock_release(&filesystem_lock);
    return success;
}

static int
sys_open(const char *file)
{
    valid_string(file);

    lock_acquire(&filesystem_lock);
    struct file *fo = filesys_open(file);
    if (!fo)
    {
        lock_release(&filesystem_lock);
        return -1;
    }
    struct file_descriptor *fd_s = malloc(sizeof(struct file_descriptor));

    fd_s->fd=thread_current()->fd_index++;
    fd_s->file_pointer = fo;
    strlcpy(fd_s->name, file, strlen(file));
    list_push_back(&thread_current()->file_descriptors, &fd_s->elem);
    lock_release(&filesystem_lock);
    return fd_s->fd;

}

static void
sys_close(int fd)
{
    if(fd < 2)
        sys_exit(-1);
    struct file_descriptor *fd_s=get_fdstruct(fd);
    if (!fd_s)
        sys_exit(-1);

    lock_acquire(&filesystem_lock);

    file_close(fd_s->file_pointer);
    list_remove(&fd_s->elem);
    free(fd_s);

    lock_release(&filesystem_lock);
}

void sys_exit(int status)
{
    /* Release file descriptors held by the thread */
    //ASSERT (status = -1);

    while(!list_empty(&thread_current()->file_descriptors))
    {
        struct list_elem *e = list_pop_front(&thread_current()->file_descriptors);
        struct file_descriptor *fd_s = list_entry(e, struct file_descriptor, elem);
        file_close(fd_s->file_pointer);
        free(fd_s);
    }

    thread_current()->exit_status = status;
    printf("%s: exit(%d)\n", thread_current()->name, status);

    thread_exit();
}

static int
sys_write(int fd, const void *buffer, unsigned size)
{
    lock_acquire(&filesystem_lock);
    if(fd == 0)
    {
        lock_release(&filesystem_lock);
        sys_exit(-1);
        return -1;
    }
    else if (fd == 1)
    {
        /* Write to the Console*/
        putbuf((const char *)buffer, size);
        lock_release(&filesystem_lock);
        return size;
    }
    else
    {
        struct file_descriptor *fd_s = get_fdstruct(fd);
        if (!fd_s)
        {
            lock_release(&filesystem_lock);
            return -1;
        }
        int r = file_write(fd_s->file_pointer, buffer, size);
        lock_release(&filesystem_lock);
        return r;
    }
}

static int
sys_read(int fd, void *buffer, unsigned size)
{
    lock_acquire(&filesystem_lock);
    if(fd == 0)
    {
        input_getc();
        return 0;
    }
    else if (fd == 1)
    {
        lock_release(&filesystem_lock);
        sys_exit(-1);
        return -1;
    }
    else
    {
        struct file_descriptor *fd_s = get_fdstruct(fd);
        if(!fd_s)
        {
            lock_release(&filesystem_lock);
            return -1;
        }
        int bytes= file_read(fd_s->file_pointer, buffer, size);
        lock_release(&filesystem_lock);
        return bytes;
    }
}

static int
sys_filesize(int fd)
{
    struct file_descriptor *fd_s = get_fdstruct(fd);
    if(!fd_s)
        return -1;
    lock_acquire(&filesystem_lock);
    int r = file_length(fd_s->file_pointer);
    lock_release(&filesystem_lock);
    return r;
}

static pid_t
sys_exec(const char *cmd_line)
{
    valid_string(cmd_line);
    return process_execute(cmd_line);
}

static int
sys_wait(pid_t pid)
{
    return process_wait(pid);
}

static void
sys_seek(int fd, unsigned position)
{
    struct file_descriptor *fd_s=get_fdstruct(fd);
    if (!fd_s)
        return;
    lock_acquire(&filesystem_lock);
    file_seek(fd_s->file_pointer, position);
    lock_release(&filesystem_lock);
}

static unsigned
sys_tell(int fd)
{
    struct file_descriptor *fd_s = get_fdstruct(fd);
    if (!fd_s)
        return -1;
    lock_acquire(&filesystem_lock);
    unsigned pos = file_tell(fd_s->file_pointer);
    lock_release(&filesystem_lock);
    return pos;
}

static void
sys_halt(void)
{
    shutdown_power_off();
}

static int
sys_mmap(int fd, void *addr)
{
    struct file_descriptor *fd_s = get_fdstruct(fd);
    int length = file_length (fd_s->file_pointer);
    if(!fd_s || !is_valid_user_addr (addr) || ((uint32_t) addr % PGSIZE) != 0 || length == 0)
    {
        return -1;
    }
    struct file *file = file_reopen(fd_s->file_pointer);
    off_t ofs = 0;
    uint32_t read_bytes = length;
    uint32_t zero_bytes = 0;
    while (read_bytes > 0)
    {
        uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        uint32_t page_zero_bytes = PGSIZE - page_read_bytes;

        if(!spt_link_mmap(file, ofs, addr, page_read_bytes, page_zero_bytes, true)) return -1;

        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        ofs+=page_read_bytes;

        addr+=PGSIZE;
    }

    return thread_current ()->mapid++;
}

static void
sys_munmap(int map)
{
    remove_mapid(&thread_current ()->mmap_list, map);
}



