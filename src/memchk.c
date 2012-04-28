#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "except.h"
#include "mem.h"

union align {
#ifdef MAXALIGN
   char pad[MAXALIGN];
#else
   int i;
   long l;
   long *lp;
   void *p;
   void (*fp)(void);
   float f;
   double d;
   long double ld;
#endif
};

#define hash(p, t) (((unsigned long)(p)>>3) & \
   (sizeof (t)/sizeof ((t)[0])-1))
   
#define NDESCRIPTORS 512

#define NALLOC ((4096 + sizeof (union align) - 1)/ \
   (sizeof (union align)))*(sizeof (union align))
   
#define NELEMS(x) ((sizeof (x))/(sizeof ((x)[0])))

#define BLOCK_FMT "This block is %ld bytes long" \
   "and was allocated from %s:%d\n"

const Except_T Mem_Failed = { "Allocation Failed" };

static struct descriptor {
   struct descriptor *free;
   struct descriptor *link;
   const void *ptr;
   long size;
   const char *file;
   int line;
} *htab[2048];

static struct descriptor freelist = { &freelist };

static struct descriptor *find(const void *ptr) {
   struct descriptor *bp = htab[hash(ptr, htab)];
   while (bp && bp->ptr != ptr)
      bp = bp->link;
   return bp;
}

static FILE *log;

void Mem_free(void *ptr, const char *file, int line) {
   if (ptr) {
      struct descriptor *bp = NULL;
      if (((unsigned long)ptr)%(sizeof (union align)) != 0
      || (bp = find(ptr)) == NULL || bp->free) {
            if (log == NULL) {
               Except_raise(&Assert_Failed, file, line);
            }
            fprintf(log, "** freeing %s\n", 
               (bp && bp->free) ? "free memory" : "unallocated memory");
            if (file)
               fprintf(log, "Mem_free(%p) called from %s:%d\n", 
                  ptr, file, line);
            if (bp && bp->free && bp->file)
               fprintf(log, BLOCK_FMT, bp->size, bp->file, bp->line);
            return;        
      }
      bp->free = freelist.free;
      freelist.free = bp;
   }
}

void *Mem_resize(void *ptr, long nbytes,
   const char *file, int line) {
   struct descriptor *bp;
   void *newptr;
   assert(ptr);
   assert(nbytes > 0);
   if (((unsigned long)ptr)%(sizeof (union align)) != 0
   || (bp = find(ptr)) == NULL || bp->free) {
      if (log == NULL) {
         Except_raise(&Assert_Failed, file, line);
      }
      fprintf(log, "** resizing %s\n", 
         (bp && bp->free) ? "free memory" : "unallocated memory");
      if (file)
         fprintf(log, "Mem_resize(%p) called from %s:%d\n", 
         ptr, file, line);
      if (bp && bp->free && bp->file)
         fprintf(log, BLOCK_FMT, bp->size, bp->file, bp->line);
      return NULL;
   }
   newptr = Mem_alloc(nbytes, file, line);
   memcpy(newptr, ptr,
      nbytes < bp->size ? nbytes : bp->size);
   Mem_free(ptr, file, line);
   return newptr;
}

void *Mem_calloc(long count, long nbytes,
   const char *file, int line) {
   void *ptr;
   assert(count > 0);
   assert(nbytes > 0);
   ptr = Mem_alloc(count*nbytes, file, line);
   memset(ptr, '\0', count*nbytes);
   return ptr;
}

static struct descriptor *dalloc(void *ptr, long size,
   const char *file, int line) {
   static struct descriptor *avail;
   static int nleft;
   if (nleft <= 0) {
      avail = malloc(NDESCRIPTORS*sizeof (*avail));
      if (avail == NULL)
         return NULL;
      nleft = NDESCRIPTORS;
   }
   avail->ptr  = ptr;
   avail->size = size;
   avail->file = file;
   avail->line = line;
   avail->free = avail->link = NULL;
   nleft--;
   return avail++;
}

void *Mem_alloc(long nbytes, const char *file, int line){
   struct descriptor *curr, *prev;
   void *ptr;
   assert(nbytes > 0);
   nbytes = ((nbytes + sizeof (union align) - 1)/
      (sizeof (union align)))*(sizeof (union align));
   for (prev = &freelist, curr = freelist.free; curr; 
      prev = curr, curr = curr->free) {
      if (curr->size > nbytes) {
         curr->size -= nbytes;
         ptr = (char *)curr->ptr + curr->size;
         if (curr->size == sizeof(union align))
            prev->free = curr->free;
         if ((curr = dalloc(ptr, nbytes, file, line)) != NULL) {
            unsigned h = hash(ptr, htab);
            curr->link = htab[h];
            htab[h] = curr;
            return ptr;
         } else
            {
               if (file == NULL)
                  RAISE(Mem_Failed);
               else
                  Except_raise(&Mem_Failed, file, line);
            }
      }
      if (curr == &freelist) {
         struct descriptor *newptr;
         if ((ptr = malloc(nbytes + NALLOC)) == NULL
         ||  (newptr = dalloc(ptr, nbytes + NALLOC,
               __FILE__, __LINE__)) == NULL)
            {
               if (file == NULL)
                  RAISE(Mem_Failed);
               else
                  Except_raise(&Mem_Failed, file, line);
            }
         newptr->free = freelist.free;
         freelist.free = newptr;
      }
   }
   assert(0);
   return NULL;
}

void Mem_log(FILE *fp) {
   log = fp;
}

void inuse(const void *ptr, long size,
   const char *file, int line, void *cl) {
      FILE *fp = cl;
      
      fprintf(fp, "** memory in use at %p\n", ptr);
      fprintf(fp, BLOCK_FMT, size, file, line);
}

void Mem_leak(FILE *fp) {
   if (fp == NULL)
      fp = stderr;
   Mem_map(inuse, fp);
}

void Mem_map(void apply(const void *ptr, long size,
	const char *file, int line, void *cl), void *cl) {
	struct descriptor *bp;
   int i;
   assert(apply);
   for (i = 0; i < NELEMS(htab); i++)
		for (bp = htab[i]; bp; bp = bp->link)
         if (bp->free == NULL)
            apply(bp->ptr, bp->size, bp->file, bp->line, cl);
}