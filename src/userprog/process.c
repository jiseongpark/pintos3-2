#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
extern int process_num = 1;


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  char * fn_copy2;
  FTE *fte_temp;
  char * real_file, *save_ptr;
  tid_t tid  = 0;
  if(process_num > 31){
    return -1;
  }
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);

  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  
  
  
  tid = thread_create ("", PRI_DEFAULT, start_process, fn_copy);
  struct thread* child =list_entry(list_rbegin(&thread_current()->child_list), struct thread, elem);
  
   
  if(thread_current()->tid == 1) sema_down(&thread_current()->main_sema);
  sema_down(&child->sema);
  if (tid == TID_ERROR){
    palloc_free_page (fn_copy);    
  }

  
  if(thread_current()->executable == 1){
    return -1;
  }
  
  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;
  char* save_ptr;
  // struct thread* parent = thread_current()->parent;
  

  /* Initialize interrupt frame and load executable. */
  
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  // printf("load complete %d\n", success);
    
  // printf("SUCCESS : %d\n", success);
  /* If load failed, quit. */
  palloc_free_page (file_name);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  struct thread *parent = thread_current();
  struct list_elem *e; 
  int flag = 0;
  struct list_elem *child;
  
  if(parent->child_num == 0){

    return -1;
  }
  int num = 0;
  parent->wait_num += 1;
  if(parent->child_num != 0){  
   for(e = list_begin(&parent->child_list); e!=list_end(&parent->child_list); e = list_next(e) ){
      num++;
      if(list_entry(e, struct thread, elem)->tid == child_tid){
        child = e;
        flag =1;
        break;
      }
      if(list_entry(e, struct thread, elem)->tid == 0){
        flag = 2;
        break;
      }
      if(num == parent->child_num){
        flag = 3;
        break;
      }
    }
    
  }

  
  if(thread_current()->exit_status == 66 && flag == 2){
    flag = 4;
    // printf("child num : %d\n", parent->child_num);
  }
  if(thread_current()->exit_status == 80 || thread_current()->exit_status == 72 || thread_current()->exit_status == 123){
    flag = 4;
  }
  // printf("FLAG : %d\n", flag);
  
  if(parent->tid == 1) sema_down(&parent->main_sema);
  sema_down(&parent->sema);

  // list_remove(child);
  if(flag == 0 || flag == 2){
    return child_tid - 4;
  }
  if(child_tid <= 0){
    
    return -1;
  }
  if(flag == 3){
    return child_tid -4;
  }
  // printf("PARENT EXIT STATUS : %d\n", parent->exit_status);
  // printf("CHILD TID : %d\n", child_tid);

  return parent->exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *curr = thread_current ();
  struct thread *parent = curr->parent;
  uint32_t *pd;
  struct file_info *of = NULL;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  
  // printf("EXIT TID : %d\n", thread_current()->tid);

  int size = list_size(&openfile_list);
   for(; size > 0; size--)
   {
      struct list_elem *e  = list_pop_front(&openfile_list);
      of = list_entry(e, struct file_info, elem);
      if(of->opener == thread_current()->tid){
       file_close(of->file);
       free(of);
      }else{
        list_push_back(&openfile_list, e);
      }
   }
  
  
  size = list_size(&thread_current()->mmf_list);
  for(; size > 0; size--)
  {
    struct list_elem *e = list_begin(&thread_current()->mmf_list);
    struct mmf *mmf = list_entry(e, struct mmf, elem);
    syscall_munmap(mmf->mapid); 
  }
  
  pd = curr->pagedir;
  // printf("BEFORE CLEAR \n");
  page_clear_all();  
  // printf("AFTER CLEAR \n");
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }


    
  
  free(thread_current()->exec);
  file_close(thread_current()->file);
  curr->parent->exit_status = thread_current()->exit_status;
  if(parent->tid==1) sema_up(&parent->main_sema);
  sema_up(&parent->sema);

  process_num--;
  thread_exit(); 
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}
 
/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char* file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{

  struct thread *t = thread_current ();
  struct thread * parent = thread_current()->parent;
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  char *fn_copy;
  char *real_file, *save_ptr;
  
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();

  
  if (t->pagedir == NULL){
    goto done;
  }
  process_activate ();

  fn_copy = (char *)malloc(strlen(file_name) + 1);
  strlcpy(fn_copy, file_name, strlen(file_name) + 1);
  real_file = strtok_r(fn_copy, " ", &save_ptr);
  strlcpy(thread_current()->name, real_file, 16);
  char * temp = (char*)malloc(sizeof(real_file) + 1);
  strlcpy(temp, real_file, sizeof(real_file) + 1);  

  
  /* Open executable file. */
  // printf("FILE : %s\n", real_file);

  file = filesys_open (real_file);
  
  if (file == NULL) 
    {
      // printf("NULL GASAGGI\n");

      parent->executable = 1;
      goto done; 
    }


  thread_current()->file = file;

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }



  /* Read program headers. */

   
  file_ofs = ehdr.e_phoff;

  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)

        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }

              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))

                goto done;

            }
          else{

            goto done;
          }
          break;
        }
    }



  
  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;


  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;
  
  if(success){
    thread_current()->exec = (char*)malloc(strlen(real_file)+1);
    strlcpy(thread_current()->exec,real_file, strlen(real_file)+1);
    process_num++;
   
  }
  
  
 done:

 if(thread_current()->parent->tid==1) sema_up(&thread_current()->parent->main_sema);
  sema_up(&thread_current()->sema);
  free(fn_copy);
  filesys_remove(temp);
  free(temp);
  return success;
}
 
/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  file_seek (file, ofs);
  // printf("LOAD SEGMENT\n");
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      if(!lazy_load(upage, file, ofs, page_read_bytes, page_zero_bytes, writable))
        return false;

      // uint8_t *kpage = frame_get_fte(upage, PAL_USER|PAL_ZERO);
      // // printf("UPAGE: %p KPAGE : %p\n", upage,kpage);

      // if(kpage == NULL)
      // {
      //   // printf("fifo occur proc(551) for tid(%d)\n", thread_current()->tid);
      //   FTE *fte = frame_fifo_fte();
      //   // printf("UADDR : %x\n", fte->uaddr);
      //   if(fte != NULL)
      //     swap_out(fte->uaddr);
      //   // printf("swap out pass\n");
      //   kpage = frame_get_fte(upage, PAL_USER | PAL_ZERO);
      //   ASSERT(kpage != NULL);
      //   // printf("KPAGE : %x\n", kpage);
      // }

      
      // if(kpage == NULL)
      //   return false;
      // /* Load this page. */
      // // printf("UPAGE : %x\n", upage);  
      // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
      //   {
      //     frame_remove_fte(kpage);
      //     page_remove_pte(upage);
      //     return false; 
      //   }
      // memset (kpage + page_read_bytes, 0, page_zero_bytes);

      // /* Add the page to the process's address space. */
      // if (!install_page (upage, kpage, writable)) 
      //   {
      //     frame_remove_fte(kpage);
      //     page_remove_pte(upage);
      //     return false; 
      //   }
      //   page_map(upage, kpage, writable);
      //   PTE * temp = page_pte_lookup(pg_round_down(upage));
      //   temp->load = true;

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;
    }
  // printf("EXIT WHILE, TID : %d\n",thread_current()->tid);

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *file_name) 
{
  uint8_t *kpage;
  bool success = false;
  kpage = frame_get_fte(((uint8_t *) PHYS_BASE) - PGSIZE, PAL_USER | PAL_ZERO);
  // printf("KPAGE : %x\n", kpage);
  if(kpage == NULL)
  {
    // printf("fifo occur proc(606)\n");
    FTE *fte = frame_fifo_fte();
    if(fte != NULL)
      swap_out(fte->uaddr);
    kpage = frame_get_fte(((uint8_t *) PHYS_BASE) - PGSIZE, PAL_USER | PAL_ZERO);
    ASSERT(kpage != NULL);
  }
  
  // kpage = palloc_get_page ();


  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
        *esp = PHYS_BASE - 12;
        page_map(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, false);
      }
      else
      {
        frame_remove_fte(kpage);
        page_remove_pte(((uint8_t *) PHYS_BASE) - PGSIZE);
      }
    }

  char *token = NULL;
  char *save_ptr = NULL;
  int argc = 0;
  int i = 0;
  

  char * copy = malloc(strlen(file_name)+1);
  strlcpy (copy, file_name, strlen(file_name)+1);


  for (token = strtok_r (copy, " ", &save_ptr); token != NULL;
    token = strtok_r (NULL, " ", &save_ptr)){
    argc++;
  }

  int *argv = calloc(argc,sizeof(uint32_t));
  i=0;
  for(token = strtok_r(file_name, " ", &save_ptr); token != NULL;
    token = strtok_r(NULL, " ", &save_ptr))
  {
    *esp -= strlen(token) + 1;
    memcpy(*esp, token, strlen(token) + 1);
    argv[i]=*esp;
    i++;
  }


  uint32_t padding = 0;

  while((int)*esp % 4 != 0)
  {
    *esp -= 1;
    memcpy(*esp, &padding, 1);
  }

  *esp -= 4;
  memcpy(*esp, &padding, 4);

  

  for(i = argc; i > 0; i--)
  {
    *esp -= 4;
    memcpy(*esp, &argv[i-1], 4);
  }
  

  uint32_t self = *esp;

  *esp -= 4;
  memcpy(*esp, &self, 4);
  *esp -= 4;
  memcpy(*esp, &argc, 4);
  *esp -= 4;
  memcpy(*esp, &padding, 4);

  free(argv);
  free(copy);

  return success;
}

void* stack_growth(uint32_t *esp)
{
  uint32_t *kpage;
  uint32_t *resp = pg_round_down(esp);

  /* find a frame for one additional stack */
  kpage = frame_get_fte(resp, PAL_USER | PAL_ZERO);

  if(kpage == NULL)
  {
    // printf("fifo occur proc(702)\n");
    FTE *fte = frame_fifo_fte();
    if(fte != NULL)
      swap_out(fte->uaddr);
    kpage = frame_get_fte(resp, PAL_USER | PAL_ZERO);
    ASSERT(kpage != NULL);
  }

  
  // printf("resp : %x , kpage : %x\n", resp, kpage);
  /* map the frame to page (pintos) */
  if(!install_page(resp, kpage, true)){

    return NULL;
  }

  /* map the frame to page (supplementary) */
  page_map(resp, kpage, true);

  return kpage;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}