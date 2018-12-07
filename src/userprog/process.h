#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "userprog/exception.h"
#include <stdbool.h>


#define STDIN 0   /* Standard input. */
#define STDOUT 1  /* Standard output. */
#define STDERR 2  /* Standard error. */

struct file_info{
	struct file *file;       /* Structure of open file. */
	int fd;                  /* File descriptor. */
	struct list_elem elem;   /* List element. */
	int opener;            /* process which open the file*/ 
	int deny_flag;            /* deny_flag */
	struct semaphore file_sema;
};

struct list openfile_list;


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void process_exit (void);

void* stack_growth(uint32_t *esp);

#endif /* userprog/process.h */





