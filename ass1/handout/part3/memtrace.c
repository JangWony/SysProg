//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"
//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);
//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;
  LOG_START();
  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS(n_allocb,n_allocb/(n_malloc+n_realloc+n_calloc) , n_freeb);

  LOG_NONFREED_START();
  item *i = list->next;
  while(i!=NULL){
	if(i->cnt>0)
		LOG_BLOCK(i->ptr, i->size, i->cnt, i->fname, i->ofs);	
	i = i->next;
  }
  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...

void *malloc(size_t size)
{
	char *error;
	void *ptr;

	if(!mallocp){
		mallocp = dlsym(RTLD_NEXT,"malloc");
		if((error = dlerror())!=NULL){

			fputs(error, stderr);
			exit(1);
		}
	}
	ptr = mallocp(size);
	n_malloc+=1;
	n_allocb+=(int)size;
	alloc(list, ptr, size);
	
	LOG_MALLOC((int)size,ptr);
	return ptr;
}
void free(void *ptr)
{
	char *error;
	
	if(!ptr)
		return;	
	if(!freep){
		freep = dlsym(RTLD_NEXT, "free");
		if((error = dlerror())!= NULL){
			fputs(error, stderr);
			exit(1);
		}
	}
        
	LOG_FREE(ptr);
	
	freep(ptr);
        
	n_freeb+=(int)(dealloc(list,ptr)->size);
}
void *calloc(size_t nmemb, size_t size){
        char *error;
        void *ptr;
	if(!callocp){
                callocp = dlsym(RTLD_NEXT,"calloc");
                if((error = dlerror())!=NULL){

                        fputs(error, stderr);
                        exit(1);
                }
        }
        ptr = callocp(nmemb, size);
        n_calloc+=1;
        n_allocb+=(int)nmemb * (int)size;
        alloc(list, ptr, nmemb * size);

        LOG_CALLOC((int)nmemb,(int)size,ptr);
	return ptr;
}
void *realloc(void *ptr, size_t size){
        char *error;
	void *newptr;
    if(!reallocp){
        reallocp = dlsym(RTLD_NEXT,"realloc");
        if((error = dlerror())!=NULL){

            fputs(error, stderr);
            exit(1);
        }
    }
    n_realloc+=1;
    n_allocb+=(int)size;
        
	newptr = reallocp(ptr,size);
	// dealloc
    dealloc(list,ptr);

    // alloc
    alloc(list,newptr,size);

	LOG_REALLOC(ptr, (int)size,newptr);
	size_t old_size = find(list,ptr)->size;

	// by size, freeb setting
	if(old_size >= size)
		n_freeb += (long)(old_size - size);
	
	return newptr;

}
