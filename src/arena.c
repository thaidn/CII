#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "except.h"
#include "arena.h"

#define T Arena_T
#define THRESHOLD 10
#define SIZE(arena) (arena)->limit - (char *)((union header *)(arena) + 1))

const Except_T Arena_NewFailed =
   { "Arena Creation Failed" };  
const Except_T Arena_Failed    =
   { "Arena Allocation Failed" };

struct T {
   T prev;
   char *avail;
   char *limit;
};
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
union header {
   struct T b;
   union align a;
};
static T freechunks;
static T largestchunk;
static int nfree;

T Arena_new(void) {
   T arena = malloc(sizeof (*arena));
   
   if (arena == NULL)
      RAISE(Arena_NewFailed);
   arena->prev = NULL;
   arena->limit = arena->avail = NULL;
   return arena;
}

void Arena_dispose(T *ap) {
   assert(ap && *ap);
   Arena_free(*ap);
   free(*ap);
   *ap = NULL;
}

void *Arena_alloc(T arena, long nbytes,
   const char *file, int line) {
   T tmp;
   
   assert(arena);
   assert(nbytes > 0);
   nbytes = ((nbytes + sizeof (union align) - 1)/
      (sizeof (union align)))*(sizeof (union align));
   tmp = arena;
   while (tmp->prev) {
      if (nbytes > tmp->limit - tmp->avail) {
         tmp = tmp->prev;
      } else {
         arena = tmp;
         break;
      }
   }
   if (nbytes > arena->limit - arena->avail) {
      T ptr;
      char *limit;
      
      if ((ptr = largestchunk) != NULL && (nbytes <= SIZE(ptr)) {
         T curr, before, tmp;
         before = NULL;
         tmp = freechunks;
         for (curr = freechunks; curr; before = curr, curr = curr->prev) {
            if (curr == largestchunk) {
               if (before)
                  before->prev = curr->prev;
               else
                  freechunks = curr->prev;
               break;
            } else if (tmp == NULL || SIZE(curr) > SIZE(tmp)) {
               tmp = curr;
            }
         }
         largestchunk = tmp;
         nfree--;
         limit = ptr->limit;
      } else {
         long m = sizeof (union header) + nbytes + 10*1024;
         ptr = malloc(m);
         if (ptr == NULL)
            {
               if (file == NULL)
                  RAISE(Arena_Failed);
               else
                  Except_raise(&Arena_Failed, file, line);
            }
         limit = (char *)ptr + m;
      }
      *ptr = *arena;
      arena->avail = (char *)((union header *)ptr + 1);
      arena->limit = limit;
      arena->prev  = ptr;
   }
   arena->avail += nbytes;
   return arena->avail - nbytes;
}

void *Arena_calloc(T arena, long count, long nbytes,
   const char *file, int line) {
   void *ptr;
   
   assert(count > 0);
   ptr = Arena_alloc(arena, count*nbytes, file, line);
   memset(ptr, '\0', count*nbytes);
   return ptr;
}

void Arena_free(T arena) {
   assert(arena);
   while (arena->prev) {
      struct T tmp = *arena->prev;
      if (nfree < THRESHOLD) {
         arena->prev->prev = freechunks;
         freechunks = arena->prev;
         nfree++;
         freechunks->limit = arena->limit;
         if (largestchunk == NULL || SIZE(freechunks) > SIZE(largestchunk))
            largestchunk = freechunks; 
      } else
         free(arena->prev);
      *arena = tmp;
   }
   assert(arena->limit == NULL);
   assert(arena->avail == NULL);
}

int Arena_count(T arena) {
   int i = 0;
   
   assert(arena);
   while (arena->prev) {
      ++i;
      arena = arena->prev;
   }
   return i;
}
