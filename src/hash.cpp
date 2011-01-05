// Hash used for translating integer window IDs to pointers.
// Like much of the old flux code, this isn't exactly beautiful...
// This could be rewritten in much fewer lines of code.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flux-edm.h"

//#define TESTING
//#define STATS
//#define PRINTCOLLISIONS

#ifndef TESTING
#include "flux.h"
#endif

#include "hash.h"






#define HASHBLOCKSIZE   128     // Blockgroesze. Bei 64k interessanterweise
                                // _keine_ Kollisionen bis zum rehashing.
#define MAXFILLCOUNT    90      // Max. Fuellmenge in Prozent vor rehashing
#define MINFILLCOUNT    80      // Min. Fuellmenge in Prozent vor rehashing




extern "C" dword crunch_number(dword i)
{
  dword n, n0= i, n1= i;

#ifdef __i386__
  asm("ror $7, %0\n": "+r" (n0));
  asm("ror $13, %0\n": "+r" (n1));
  n= (n0&0x7F37F773) | (n1&~0x7F37F773);

  n0= i, n1= i;
  asm("ror $8, %0\n": "+r" (n0));
  asm("ror $16, %0\n": "+r" (n1));
  n^= (n0&0xF0F0F0F0) | (n1&0x0F0F0F0F);

  n0= n;
  asm("rol $11, %0\n": "+r" (n0));
  n^= n0;

  asm("rol $19, %0\n": "+r" (n0));
  n^= n0;

  asm("ror $3, %0\n": "+r" (n0));
  n^= n0;

  return n;
#else
    // straight C translation of the above
    
    #define ROL(X, N) ( (X)<<(N) | (X)>>(32-N) )
    #define ROR(X, N) ( (X)>>(N) | (X)<<(32-N) )
    
    //~ asm("ror $7, %0\n": "+r" (n0));
    //~ asm("ror $13, %0\n": "+r" (n1));
    //~ n= (n0&0x7F37F773) | (n1&~0x7F37F773);
    n0= ROL(n0, 7);
    n1= ROR(n1, 13);
    n= (n0&0x7F37F773) | (n1&~0x7F37F773);

    //~ n0= i, n1= i;
    //~ asm("ror $8, %0\n": "+r" (n0));
    //~ asm("ror $16, %0\n": "+r" (n1));
    //~ n^= (n0&0xF0F0F0F0) | (n1&0x0F0F0F0F);
    n0= i, n1= i;
    n0= ROR(n0, 8);
    n1= ROR(n1, 16);
    n^= (n0&0xF0F0F0F0) | (n1&0x0F0F0F0F);
    
    //~ n0= n;
    //~ asm("rol $11, %0\n": "+r" (n0));
    //~ n^= n0;
    n0= n;
    n0= ROL(n0, 11);
    n^= n0;
    
    //~ asm("rol $19, %0\n": "+r" (n0));
    //~ n^= n0;
    n0= ROL(n0, 19);
    n^= n0;

    //~ asm("ror $3, %0\n": "+r" (n0));
    //~ n^= n0;
    n0= ROR(n0, 3);
    n^= n0;
    
    return n;
#endif
}





template<class T> bool int_hash<T>::construct()
{
  items= (hash_item_t *)calloc(sizeof(hash_item_t), HASHBLOCKSIZE);
  if(!items) return false;
  max_items= HASHBLOCKSIZE;
  n_items= 0;
#ifdef STATS
  stat_coll_add= stat_coll_lookup= 0;
#endif
  return true;
}

template<class T> bool int_hash<T>::destroy()
{
  free(items);
  return true;
}




template<class T> inline void int_hash<T>::setitem(int idx, dword id, T *value)
{
  items[idx].id= id;
  items[idx].value= value;
}

template<> inline void int_hash<window_list>::setitem(int idx, dword id, window_list *value)
{
  items[idx].value= value;
}






template<class T> int int_hash<T>::lookup_idx(dword id)
{
  dword hashnum= crunch_number(id);
  dword idx= hashnum%max_items;
  dword x= idx;
  int i;

  if( items[x].id!=id || !items[x].value )
  {
#   ifdef PRINTCOLLISIONS
    printf("Collision: (%d,%u) -> (%d, %u) ",
           id,hashnum, items[x].id,crunch_number(items[x].id));
#   endif
    for(i= 1; i<max_items/2; i++)
    {
      x= dword(idx+i)%max_items;
      if( items[x].id==id && items[x].value )
      {
#       ifdef PRINTCOLLISIONS
        printf("(%d)\n", i);
#       endif
#       ifdef STATS
        stat_coll_lookup+= i;
#       endif
        return x;
      }

      x= dword(idx-i)%max_items;
      if( items[x].id==id && items[x].value )
      {
#       ifdef PRINTCOLLISIONS
        printf("(%d)\n", i);
#       endif
#       ifdef STATS
        stat_coll_lookup+= i;
#       endif
        return x;
      }
    }
    return -1;
  }

  return x;
}



template<> int int_hash<window_list>::lookup_idx(dword id)
{
  dword hashnum= crunch_number(id);
  dword idx= hashnum%max_items;
  dword x= idx;
  int i;

  if( !items[x].value || items[x].value->self->id!=id  )
  {
#   ifdef PRINTCOLLISIONS
    printf("Collision: (%d,%u) -> (%d, %u) ",
           id,hashnum, (items[x].value? items[x].value->self->id: 0),
           (items[x].value? crunch_number(items[x].value->self->id): (unsigned)-1) );
#   endif
    for(i= 1; i<max_items/2; i++)
    {
      x= dword(idx+i)%max_items;
      if( items[x].value && items[x].value->self->id==id  )
      {
#       ifdef PRINTCOLLISIONS
        printf("(%d)\n", i);
#       endif
#       ifdef STATS
        stat_coll_lookup+= i;
#       endif
        return x;
      }

      x= dword(idx-i)%max_items;
      if( items[x].value && items[x].value->self->id==id )
      {
#       ifdef PRINTCOLLISIONS
        printf("(%d)\n", i);
#       endif
#       ifdef STATS
        stat_coll_lookup+= i;
#       endif
        return x;
      }
    }
    return -1;
  }

  return x;
}


template<class T> bool int_hash<T>::reinsert(int oldmax_items, hash_item_t *olditems)
{
  for(int i= 0; i<oldmax_items; i++)
  {
    if(olditems[i].value)
    {
      if(!add(olditems[i].id, olditems[i].value))
        return false;
    }
  }
  return true;
}

template<> bool int_hash<window_list>::reinsert(int oldmax_items, hash_item_t *olditems)
{
  for(int i= 0; i<oldmax_items; i++)
  {
    if(olditems[i].value)
    {
      if(!add(olditems[i].value->self->id, olditems[i].value))
        return false;
    }
  }
  return true;
}


template<class T> bool int_hash<T>::realloc(int newmax_items)
{
  int oldmax_items= max_items;
  hash_item_t *olditems= items;

  //~ dbgi("realloc - newsize %d, n_items %d (%d%% was %d%%)\n",
         //~ newmax_items, n_items,
         //~ n_items*100/newmax_items, n_items*100/oldmax_items);

  if( !(items= (hash_item_t *)calloc(sizeof(hash_item_t), newmax_items)) )
    return false;

  max_items= newmax_items;
  n_items= 0;

  if(!reinsert(oldmax_items, olditems))
    return false;

  free(olditems);

  return true;
}



template<class T> int int_hash<T>::add(dword id, T *value)
{
  dword hashnum, idx;
  int i, x;

  if( n_items*100 > max_items*MAXFILLCOUNT )
  {
    if(!realloc(max_items*2))
    {
      logmsg("hash: couldn't reallocate buffer!\n");
      return 0;
    }
  }

  hashnum= crunch_number(id);
  idx= hashnum%max_items;
  x= idx;


  // kein Check auf doppelte Werte...

  if( items[x].value )
  {
#   ifdef PRINTCOLLISIONS
    printf("Collision: (%3d%%) (%d,%u) ",
           n_items*100/max_items, id,hashnum);
#   endif
    for(i= 1; i<max_items/2; i++)
    {
      x= (idx+i)%max_items;
      if( !items[x].value )
        break;

      x= (idx-i)%max_items;
      if( !items[x].value )
        break;
    }
#   ifdef PRINTCOLLISIONS
    printf("(%d)\n", i);
#   endif

#   ifdef STATS
    stat_coll_add+= i;
#   endif
  }

  setitem(x, id, value);

  return ++n_items;
}


template<class T> int int_hash<T>::del(dword id)
{
  int idx= lookup_idx(id);

  if(idx==-1) return 0;

  setitem(idx, 0, 0);

  if( n_items <= (max_items/2*MINFILLCOUNT)/100 &&
      max_items/2 >= HASHBLOCKSIZE )
  {
    realloc(max_items/2);
  }

  return n_items--;
}


template<class T> T *int_hash<T>::lookup(dword id)
{
  int idx= lookup_idx(id);

  if(idx==-1) return 0;
  return items[idx].value;
}



template<class T> void int_hash<T>::enum_items( void (*callback)(int_hash<T> *hash, T *item) )
{
  for(int i= 0; i<max_items; i++)
  {
    if(items[i].value)
    {
      int old_n_items= n_items;

      callback(this, items[i].value);

      if(old_n_items!=n_items)
        i= -1;  // Falls das Item entfernt wird - und genau das ist der Sinn
                // der Sache
    }
  }
}



// Damit die "Templatespezialisierung" erzeugt wird
int_hash<window_list> eat_memory_uselessly;






// =============================== TESTING ====================================







#ifdef TESTING

#include <sys/time.h>



struct teststruct
{
  int num;
  char *name;

  teststruct(int num, char *name)
  {
    this->num= num;
    this->name= strdup(name);
  }

  ~teststruct()
  {
    free(name);
  }

  void print()
  {
    printf("%3d: %s\n", num, name);
  }
};



void test1()
{
  struct timeval tv_start, tv_end;

  int_hash<teststruct> hash;

  hash.construct();


#define N 100000

  for(int i= 0; i<N; i++)
  {
    char ch[128];
    sprintf(ch, "Number %d", i);
    teststruct *t= new teststruct(i, ch);
    if(!t) printf("couldn't alloc %d!\n", i);
    hash.add(i, t);
  }

  puts("---lookup begin---"); fflush(stdout);
  gettimeofday(&tv_start, 0);

  for(int i= 0; i<N; i++)
  {
    int n= i; //rand()%N;
    teststruct *t= hash.lookup(n);
    if(!t) printf("%d not found!\n", n), fflush(stdout);
    else if(t->num!=n) printf("wrong number (%3d -> %3d)!\n", n, t->num), fflush(stdout);
  }

  gettimeofday(&tv_end, 0);

  double t1= tv_start.tv_sec + double(tv_start.tv_usec)/1000000;
  double t2= tv_end.tv_sec + double(tv_end.tv_usec)/1000000;

  printf("---lookup end---\n"
         "time: %.4f (%.4fmsec/access)\n"
#ifdef STATS
         "collisions/add:\t\t%.2f\n"
         "collisions/lookup:\t%.2f\n"
#endif
         ,
         t2-t1, (t2-t1)*1000/N
#ifdef STATS
         ,
         double(hash.stat_coll_add)/N,
         double(hash.stat_coll_lookup)/N
#endif
         );
  fflush(stdout);



  puts("");


  for(int i= 0; i<N; i+= 3)
  {
    teststruct *s= hash.lookup(i);
    if(!s) printf("Couldn't find %d!\n", i);
    else delete s;

    if(hash.del(i)<0)
      printf("Couldn't delete %d!\n", i);


    s= hash.lookup(i+1);
    if(!s) printf("Couldn't find %d!\n", i+1);
    else delete s;

    if(hash.del(i+1)<0)
      printf("Couldn't delete %d!\n", i+1);
  }

#ifdef STATS
  hash.stat_coll_lookup= 0;
#endif

  puts("---lookup begin---"); fflush(stdout);
  gettimeofday(&tv_start, 0);

  for(int i= 2; i<N; i+= 3)
  {
    teststruct *t= hash.lookup(i);
    if(!t) printf("%d not found!\n", i), fflush(stdout);
    else if(t->num!=i) printf("wrong number (%3d -> %3d)!\n", i, t->num), fflush(stdout);
  }

  gettimeofday(&tv_end, 0);

  t1= tv_start.tv_sec + double(tv_start.tv_usec)/1000000;
  t2= tv_end.tv_sec + double(tv_end.tv_usec)/1000000;

  printf("---lookup end---\n"
         "time: %.4f (%.4fmsec/access)\n"
#ifdef STATS
         "collisions/lookup:\t%.2f\n"
#endif
         ,
         t2-t1, (t2-t1)*1000/(N*2/3)
#ifdef STATS
         ,
         double(hash.stat_coll_lookup)/(N*2/3)
#endif
         );
  fflush(stdout);


  for(int i= 2; i<N; i+= 3)
  {
    teststruct *s= hash.lookup(i);
    if(!s) printf("Couldn't find %d!\n", i), fflush(stdout);
    else delete s;

    if(hash.del(i)<0)
      printf("Couldn't delete %d!\n", i), fflush(stdout);
  }


  hash.destroy();
}





void test2()
{
  struct timeval tv_start, tv_end;

  int_hash<window_list> hash;

  hash.construct();


#undef N
#define N 100000

  for(int i= 0; i<N; i++)
  {
    window_list *w= new window_list(i);
    hash.add(i, w);
  }

  puts("---lookup begin---"); fflush(stdout);
  gettimeofday(&tv_start, 0);

  for(int i= 0; i<N; i++)
  {
    int n= i; //rand()%N;
    window_list *t= hash.lookup(n);
    if(!t) printf("%d not found!\n", n), fflush(stdout);
    else if(t->self->id!=n) printf("wrong number (%3d -> %3d)!\n", n, t->self->id), fflush(stdout);
  }

  gettimeofday(&tv_end, 0);

  double t1= tv_start.tv_sec + double(tv_start.tv_usec)/1000000;
  double t2= tv_end.tv_sec + double(tv_end.tv_usec)/1000000;

  printf("---lookup end---\n"
         "time: %.4f (%.4fmsec/access)\n"
#ifdef STATS
         "collisions/add:\t\t%.2f\n"
         "collisions/lookup:\t%.2f\n"
#endif
         ,
         t2-t1, (t2-t1)*1000/N
#ifdef STATS
         ,
         double(hash.stat_coll_add)/N,
         double(hash.stat_coll_lookup)/N
#endif
         );
  fflush(stdout);

  for(int i= 0; i<N; i++)
  {
    window_list *s= hash.lookup(i);
    if(!s) printf("Couldn't find %d!\n", i), fflush(stdout);

    if(hash.del(i)<0)
      printf("Couldn't delete %d!\n", i), fflush(stdout);

    else delete s;
  }



  hash.destroy();
}


int main(void)
{
  puts("\n\n---test1---\n");
  test1();
  puts("\n\n---test2---\n");
  test2();
}


#endif











