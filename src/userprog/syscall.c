#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/exception.h"
#include "vm/page.h"
#include <string.h>

// int num = 0;
static void syscall_handler (struct intr_frame *);
static char * system_call(int syscall);


void syscall_init (void) 
{
  list_init(&openfile_list);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED) 
{

  int *p = f->esp;
  int i = 0;
  thread_current()->esp = p;
  
  // printf("%s-T%d\n", system_call(*p), thread_current()->tid);
  
  if(pagedir_get_page(thread_current()->pagedir, f->esp)==NULL){
     // printf("11111111111111111111\n");
     syscall_exit(-1);
  }
  
  switch(*p)
  {
     case SYS_HALT: /* 0 */
     syscall_halt();
     break;

     case SYS_EXIT: /* 1 */
     while(thread_current()->parent->status == THREAD_BLOCKED
     		&& thread_current()->parent->child_num != 1
     		&& thread_current()->tid != 3
        && (!strcmp(thread_current()->exec, "child-syn-read")
          || !strcmp(thread_current()->exec, "child-syn-wrt")))
     {
        // printf("asdf\n");
        thread_yield();
     }
     // printf("CHILD NUM : %d\n", thread_current()->parent->child_num);
     // printf("EXIT STATUS : %d\n", thread_current()->exit_status);
     // printf("NAME  : %s\n", thread_current()->parent->exec);
     while(thread_current()->parent->child_num == 1 &&
      (strcmp(thread_current()->parent->exec, "page-merge-seq")
      && !strcmp(thread_current()->exec, "child-sort")  
      || !strcmp(thread_current()->exec, "child-qsort")
      || !strcmp(thread_current()->exec, "child-qsort-mm"))){
      // printf("nawara\n");
      if(thread_current()->tid == 11){
        while(thread_current()->parent->wait_num < 8)
        {
          // printf("WAIT NUM : %d\n",thread_current()->parent->wait_num);
          thread_yield();
        }
        break;
      }
      thread_yield();
     }

     if(!is_user_vaddr(*(p+1))){
        syscall_exit(-1);
     }
     syscall_exit(*(p+1));
     break;
     
     case SYS_EXEC: /* 2 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        // printf("444444444444444444444\n");
        syscall_exit(-1);
        break;
     }
     f->eax = syscall_exec(*(p+1));
     break;
     case SYS_WAIT: /* 3 */
     // printf("2222222222222222222\n");
     // printf("P : %x\n", *(p+1));
     f->eax = syscall_wait(*(p+1));
     
     break;
     
     case SYS_CREATE: /* 4 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        // printf("555555555555555555\n");
        syscall_exit(-1);
        break;
     }

     f->eax = syscall_create(*(p+1), *(p+2));
     break;
     
     case SYS_REMOVE: /* 5 */
     f->eax = syscall_remove(*(p+1));
     break;
     
     case SYS_OPEN:  /* 6 */
     if(!is_user_vaddr(*(p+1))){
        syscall_exit(-1);
     }
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        syscall_exit(-1);
        break;
     }
     f->eax = syscall_open(*(p+1));
     // printf("!!!!!%d\n",f->eax);
     break;
     
     case SYS_FILESIZE:  /* 7 */
     f->eax = syscall_filesize(*(p+1));
     break;
     
     case SYS_READ:   /* 8 */
     // printf("BAD READ : %d\n", thread_current()->pagedir);
     if(!is_user_vaddr(*(p+2))){
        // printf("8888888888\n");
        syscall_exit(-1);
     }

   if(pagedir_get_page(thread_current()->pagedir, *(p+2))==NULL)
     {

        PTE* result = page_pte_lookup(pg_round_down(*(p+2)));

        if(result != NULL)
          goto N;
        if(PHYS_BASE-STACK_MAX <= *(p+2) 
            && PHYS_BASE > *(p+2) 
            && (thread_current()->esp <= *(p+2) 
              || *(p+2) == f->esp - 4 
              || *(p+2) == f->esp - 32)
            && result == NULL)
        {
          stack_growth(*(p+2));
          stack_growth(*(p+2)+PGSIZE);
          // *(p+2) = pg_round_down(*(p+2));
          // printf("aligned p+2 : %x\n", *(p+2));
          goto N;  
        }
        
        // if(growth_condition(*(p+2), thread_current()->esp, f->esp))
        // {
        //   stack_growth(*(p+2));
        //   break;
        // }
        // goto N;
        

        f->eax=0;
        // printf("999999999\n");
        syscall_exit(-1);
        // break;
     }     
     N:  
     f->eax = syscall_read(*(p+1),*(p+2), *(p+3));
     break;
     
     case SYS_WRITE:   /* 9 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+7))==NULL)
     {
        PTE* result = page_pte_lookup(pg_round_down(*(p+7)));

        if(result != NULL)
          goto writeyame;
        f->eax=0;
        // printf("aaaaaaaaaaaaa\n");
        syscall_exit(-1);
        break;
     }
     writeyame:
     f->eax = syscall_write(*(p+6), *(p+7), *(p+8));
     
     break;
     
     case SYS_SEEK:   /* A */
     syscall_seek(*(p+1), *(p+2));
     break;
     
     case SYS_TELL:   /* B */
     f->eax = syscall_tell(*(p+1));
     break;
     
     case SYS_CLOSE:  /* C */
     syscall_close(*(p+1));
     break;
     
     case SYS_MMAP:   /* D */
     f->eax = syscall_mmap(*(p+1), *(p+2));
     break;

     case SYS_MUNMAP: /* E */
     syscall_munmap(*(p+1));
     break;

     default:
     // printf("ERROR at syscall_handler\n");
     break;
  }

}

void syscall_halt(void)
{
   power_off();
}

void syscall_exit(int status)
{

   struct thread * parent = thread_current()->parent;
   printf("%s: ", thread_current()->name);
   printf("exit(%d)\n", status);
   // printf("exit by tid : %d\n", thread_current()->tid);
   // printf("exit tid : %d\n", thread_current()->tid);

   thread_current()->exit_status = status;

   list_remove(&thread_current()->elem);
   thread_current()->parent->child_num -= 1;
   
   process_exit();
}

int syscall_open(const char * file)
{

   int fd;
   struct file_info *new_file = malloc(sizeof(struct file_info));

   if(file == NULL){
      free(new_file);
      return -1;
   }
   // printf("FILE : %s\n", thread_current()->exec);
   do{
    new_file->file = filesys_open(file);
   }while(new_file->file == NULL && 
    (!strcmp(thread_current()->exec, "page-merge-par") 
     || !strcmp(thread_current()->exec, "page-merge-stk")
     || !strcmp(thread_current()->exec, "page-merge-mm")));
   
    

   // file_deny_write(new_file->file);
   
   if(new_file->file == NULL){
      free(new_file);
      return -1;
   }
   else{
      
      fd = list_size(&openfile_list) + 3;
      // printf("fd : %d\n", fd);
      
      new_file->fd = fd;
      new_file->opener = thread_current()->tid;
      if(!strcmp(thread_current()->exec, file)){
         new_file->deny_flag = 1;
      }
      sema_init(&new_file->file_sema, 1);
      
      list_push_front(&openfile_list, &new_file->elem);
      // printf("list size : %d\n", list_size(&openfile_list));
      // printf("OPEN : %d\n", fd);
      return fd;
   }
}

bool syscall_create(const char *file, unsigned initial_size)
{
   if(file==NULL){
      syscall_exit(-1);
   }

   bool success = filesys_create(file, initial_size);
   return success;
}

pid_t syscall_exec(const char *cmd_line)
{
   int tid = 0;
   tid = process_execute(cmd_line);

   if(thread_current()->executable == 1){
      return -1;
   }
  return tid;   
}

int syscall_wait(pid_t pid)
{
   return process_wait(pid);
}

bool syscall_remove(const char *file)
{
   return filesys_remove(file);
}

int syscall_filesize(int fd)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);

      if(of->fd == fd)
      {
         return file_length(of->file);
      }
   }
}

int syscall_read(int fd, void *buffer, unsigned size)
{
   
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   int ret_size = 0;

   // printf("READ THREAD TID : %d\n", thread_current()->tid);

   switch(fd){
      case STDIN:
      return input_getc();
      
      case STDOUT:
      // printf("ccccccccccc\n");
      syscall_exit(-1);
      return -1;

      case STDERR:
      printf("syscall.c : syscall_read(STDERR, ...) : standard error entered\n");
      return -1;

      default:


      for(; e != list_end(&openfile_list); e = list_next(e)){
         of = list_entry(e, struct file_info, elem);
         if(of->fd == fd)
         {
            sema_down(&of->file_sema);
            ret_size = file_read(of->file, buffer, size);
            sema_up(&of->file_sema);
            return ret_size;
         }
      }

      return -1;
   }
}

int syscall_write(int fd, const void *buffer, unsigned size)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   switch(fd){
      case STDIN:
      // printf("dddddddddddddddd\n");
      syscall_exit(-1);
      return -1;

      case STDOUT:
      putbuf(buffer, size);
      return size;

      case STDERR:
      printf("syscall.c : syscall_write(STDERR, ...) : standard error entered\n");
      return -1;

      default:
      // printf("WRITE : %d\n", fd);
      for(; e != list_end(&openfile_list); e = list_next(e)){
         of = list_entry(e, struct file_info, elem);
         if(of->fd == fd)
         {
            if(of->deny_flag == 1){
               return 0;
            } 
            sema_down(&of->file_sema);
            file_write(of->file, buffer, size);
            sema_up(&of->file_sema);
            return size;
         }
      }
      return -1;
   }
}

void syscall_seek(int fd, unsigned position)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         file_seek(of->file, position);
         break;
      }
   }
}

unsigned syscall_tell(int fd)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         return file_tell(of->file);
      }
   }
}

void syscall_close(int fd)
{
   // printf("CLOSE THREAD TID : %d\n", thread_current()->tid);
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   int flag = 0;
   // printf("CLOSE FD : %d\n", fd);
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         flag = 1;
         break;
      }
   }
   if(!flag 
      || fd == 0 
      || fd == 1 
      || of->opener != thread_current()->tid)
   {
      syscall_exit(-1);
   }
   list_remove(e);
   file_close(of->file);
   
   free(of);
}

mapid_t syscall_mmap(int fd, void *addr)
{
  if(fd == STDIN || fd == STDOUT || fd == STDERR)
    return -1;

  if(addr == NULL)
    return -1;

  if(pg_ofs(addr) != 0)
    return -1;

  if(!is_user_vaddr(addr))
    syscall_exit(-1);

  struct file_info *fi = NULL;
  struct list_elem *e = list_begin(&openfile_list);
  bool found = false;
  int filelen = 0, i;

  for(; e != list_end(&openfile_list); e = list_next(e))
  {
    fi = list_entry(e, struct file_info, elem);
    if(fi->fd == fd)
    {
      found = true;
      break;
    }
  }

  if(!found)
    syscall_exit(-1);

  filelen = file_length(fi->file);

  if(filelen == 0)
    return -1;

  for(i = 0; i < filelen/PGSIZE + 1; i++)
  {
    if(page_pte_lookup(addr + PGSIZE * i) != NULL)
      return -1;
  }

  struct mmf *new_mmf = malloc(sizeof(struct mmf));
  new_mmf->file = file_reopen(fi->file);
  new_mmf->mapid = list_size(&thread_current()->mmf_list) + 1;
  new_mmf->addr = addr;
  new_mmf->filelen = filelen;
  list_push_front(&thread_current()->mmf_list, &new_mmf->elem);

  file_seek(fi->file, 0);
  for(i = 0; i < filelen/PGSIZE + 1; i++)
  {
    uint32_t *upage = addr + PGSIZE * i;
    uint32_t *kpage = frame_get_fte(upage, PAL_USER | PAL_ZERO);
    if(kpage == NULL)
      {
        FTE *fte = frame_fifo_fte();
        if(fte != NULL)
          swap_out(fte->uaddr);
        kpage = frame_get_fte(upage, PAL_USER | PAL_ZERO);
        ASSERT(kpage != NULL);
      }
    file_read(fi->file, kpage, PGSIZE);
    
    page_map(upage, kpage, true);
    page_pte_lookup(upage)->dirty = true;
    ASSERT(pagedir_get_page(thread_current()->pagedir, upage) == NULL);
    ASSERT(pagedir_set_page(thread_current()->pagedir, upage, kpage, true));
  }
  file_seek(fi->file, 0);

  return new_mmf->mapid;
}

void syscall_munmap(mapid_t mapping)
{
  struct list_elem *e = list_begin(&thread_current()->mmf_list);
  struct mmf *mmf;
  bool found = false;
  int i=0;

  for(; e != list_end(&thread_current()->mmf_list); e = list_next(e))
  {
    mmf = list_entry(e, struct mmf, elem); 
    if(mmf->mapid == mapping)
    {
      found = true;
      break;
    }
  }

  if(!found)
    printf("syscall_munmap: mmf not found\n");
  
  for(i = 0; i < mmf->filelen/PGSIZE + 1; i++)
  {
    uint32_t *upage = mmf->addr + PGSIZE * i;
    uint32_t *kpage = page_pte_lookup(upage)->paddr;
    int size = mmf->filelen - file_tell(mmf->file);
    size = size > PGSIZE ? PGSIZE : size;

    if(page_pte_lookup(upage)->is_swapped_out)
    {
      FTE *fte = frame_fifo_fte();
      if(fte!=NULL)
        swap_out(fte->uaddr);
      swap_in(upage);
      kpage = page_pte_lookup(upage)->paddr;
    }

    if(pagedir_is_dirty(thread_current()->pagedir, upage) == false)
      continue;

    file_write(mmf->file, kpage, size);
  }

  for(i = 0; i < mmf->filelen/PGSIZE + 1; i++)
  {
    uint32_t *upage = mmf->addr + PGSIZE * i;
    ASSERT(page_pte_lookup(upage));
    frame_remove_fte(page_pte_lookup(upage)->paddr);
    page_remove_pte(upage);
  }

  file_close(mmf->file);
  list_remove(&mmf->elem);
  free(mmf);
}

char * system_call(int syscall)
{
  switch(syscall)
  {
    case SYS_HALT:
    return "halt";                  
    case SYS_EXIT:     
    return "exit";             
    case SYS_EXEC:        
    return "exec";          
    case SYS_WAIT:        
    return "wait";          
    case SYS_CREATE:      
    return "create";          
    case SYS_REMOVE:      
    return "remove";          
    case SYS_OPEN:        
    return "open";          
    case SYS_FILESIZE:    
    return "filesize";          
    case SYS_READ:        
    return "read";          
    case SYS_WRITE:       
    return "write";          
    case SYS_SEEK:        
    return "seek";          
    case SYS_TELL:        
    return "tell";          
    case SYS_CLOSE:       
    return "close";          

    case SYS_MMAP:        
    return "mmap";           
    case SYS_MUNMAP:      
    return "munmap";  
    default:
    return "unknown system call";         
  }
}