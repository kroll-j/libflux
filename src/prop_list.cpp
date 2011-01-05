// Property list used in Flux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prop_list.h"
#include "flux-edm.h"

struct srchstats
{
  int o;
  int ncalls;
  int nfwd, nbwd;

  dword nclocksamples;
  dword kclocks;
} s_id, s_name;

void props_dump(prop_list *props);


// blah blah blah... yes this could be written smaller/cleaner, the code is quite old :)

/* int strcomp3(char *str, char *s1, char *s2)
 *
 * Vergleicht s1 und s2 auf Ähnlichkeit zu str. Wird zum alphabetischen
 * Sortieren benutzt.
 *
 * Gibt 0 zurück, wenn beide gleich ähnlich sind, 1, wenn s1 ähnlicher ist
 * als s2 und 2 wenn s2 ähnlicher ist als s1.
 *
 */
#define diff(a, b) abs((a)==0? (a)-(b): (b)-(a))
int strcomp3(const char *str, const char *s1, const char *s2)
{
  int d1, d2;
  while(*str||*s1||*s2)
  {
    d1= diff(*str, *s1);
    d2= diff(*str, *s2);

    if(d2>d1) return 1;
    if(d1>d2) return 2;

    if(*str) str++;
    if(*s1) s1++;
    if(*s2) s2++;
  }

  return 0;
}


#define badptr \
( !props? puts("props==NULL"), 1: \
  !props->by_id? puts("props->by_id==NULL"), 1: \
  !props->by_name? puts("props->by_name==NULL"), 1: 0 )

#define ptr_check \
  if(badptr) printf("prop_list: in %s, %d\n", __FILE__, __LINE__), *(int*)0= 0; //exit(1);



dword timer(byte startstop)
{
  static dword timer;

#ifdef __i386__	// this is not used currently anyway.
  if(!startstop)
  {
    __asm("rdtsc\n"
          "movl %%eax, %0\n"
           : "=m" (timer)
           :
           : "eax", "edx" );
  }
  else
  {
    __asm("rdtsc\n"
          "subl %0, %%eax\n"
          "movl %%eax, %0\n"
           : "=m" (timer)
           :
           : "eax", "edx" );
  }
#endif
  return timer;
}



/* int props_find_id_pos(prop_list *props, dword id)
 *
 * Findet die Position im nach IDs sortierten array, das dem id am nächsten
 * ist. Wird zum Einfügen und Suchen benutzt.
 *
 */
int props_find_id_pos(prop_list *props, dword id)
{
  int start= 0, end= props->size-1, half;
  int d1, d2;
  int i;
  property **tp= props->by_id;

  s_id.ncalls++;

  if(!props->size) return 0;

  if( id>tp[props->size-1]->id )
    return props->size;
  else if( id<0 )
    return 0;

  while(1)
  {
    s_id.o++;

    d1= abs(tp[start]->id-id);
    d2= abs(tp[end]->id-id);
    half= (end-start)/2;

    if(!d2) return end;
    if(!d1) return start;

    // weiter von start
    if( d1>d2 )
      start= start+half;

    // weiter von end
    else if( d1<d2 )
      end= end-half;

    // gleich weit "weg" oder keine Veränderung, suchen
    if( d1==d2 || !half )
    {
      i= start;

      if( tp[i]->id<id )
        while( i<props->size && tp[i]->id<id ) i++, s_id.nfwd++;
      else
      {
        while( i>=0 && tp[i]->id>=id ) i--, s_id.nbwd++;
        i++;
      }

      return i;
    }
  }
}


/* int props_find_name_pos(prop_list *props, char *name)
 *
 * Findet die Position im nach Namen sortierten array, das name am nächsten
 * ist. Wird zum Einfügen und Suchen benutzt.
 *
 */
int props_find_name_pos(prop_list *props, const char *name)
{
  int start= 0, end= props->size-1, half;
  int d;
  int i;
  property **tp= props->by_name;

  s_name.ncalls++;

  if(!props->size) return 0;

  if( strcmp(name, tp[props->size-1]->name)>0 )
    return props->size;
  else if( strcmp(name, tp[0]->name)<0 )
    return 0;

  while(1)
  {
    s_name.o++;

    d= strcomp3(name, tp[start]->name, tp[end]->name);
    half= (end-start)/2;

    if(!strcmp(name, tp[end]->name)) return end;
    if(!strcmp(name, tp[start]->name)) return start;

    // weiter von start
    if(d==2)
      start= start+half;

    // weiter von end
    else if(d==1)
      end= end-half;

    // gleich weit "weg" oder keine Veränderung, suchen
    if( !d || !half )
    {
      i= start;

      if( strcmp(tp[i]->name, name)<0 )
        while( i<props->size && strcmp(tp[i]->name, name)<0 ) i++, s_name.nfwd++;
      else
      {
        while( i>=0 && strcmp(tp[i]->name, name)>=0 ) i--, s_name.nbwd++;
        i++;
      }

      return i;
    }
  }
}


/* void props_insert_byid(prop_list *props, property *newprop, int pos)
 *
 * Fügt newproperty im nach IDs sortierten array an der Position pos ein.
 *
 */
void props_insert_byid(prop_list *props, property *newprop, int pos)
{
  ptr_check;

  memmove(props->by_id+pos+1, props->by_id+pos,
          (props->size-pos)*sizeof(property*));
  props->by_id[pos]= newprop;
}

/* void props_insert_byname(prop_list *props, property *newprop, int pos)
 *
 * Fügt newproperty im nach Namen sortierten array an der Position pos ein.
 *
 */
void props_insert_byname(prop_list *props, property *newprop, int pos)
{
  ptr_check;

  memmove(props->by_name+pos+1, props->by_name+pos,
          (props->size-pos)*sizeof(property*));
  props->by_name[pos]= newprop;
}





/* int props_init(prop_list *props)
 *
 * Initialisiert eine prop_list struct.
 *
 * Gibt nicht-null zurück für ok, null für zu wenig Speicher.
 *
 */
int props_init(prop_list *props)
{
  if(!props) { puts("props_init: props=0"); return 0; }
  if( !(props->by_id= (property **)malloc( sizeof(property*)*PCHUNKSIZE )) )
    return 0;
  if( !(props->by_name= (property **)malloc( sizeof(property*)*PCHUNKSIZE )) )
    return 0;
  props->size= 0;
  return 1;
}


/* void props_free(prop_list *props)
 *
 * Gibt den Speicher, der von der prop_list belegt wird, wieder frei.
 *
 */
void props_free(prop_list *props)
{
  int i;

  ptr_check;

  for(i= props->size-1; i>=0; i--)
  {
    free(props->by_id[i]->name);
    free(props->by_id[i]);
    props->by_id[i]= 0;
  }

  free(props->by_id);
  free(props->by_name);
  props->by_id= 0;
  props->by_name= 0;
}


/* property *props_find_byid(prop_list *props, int id)
 *
 * Sucht nach der property mit dem ID id.
 *
 * Gibt einen Zeiger auf die gefundene property-Struktur zurück, oder 0,
 * wenn sie nicht gefunden wurde.
 *
 */
property *props_find_byid(prop_list *props, int id)
{
    int idx;

    ptr_check;

    idx= props_find_id_pos(props, id);

    if(idx<0 || idx>=props->size) return 0;
    else if(props->by_id[idx] && props->by_id[idx]->id==id) return props->by_id[idx];
    else if(!props->by_id[idx]) printf("prop_list: find: idx %d is empty?!\n", idx);
    else printf("prop_list: find: %d!=%d\n", props->by_id[idx]->id, id);
    return 0;
}

/* property *props_find_byname(prop_list *props, char *name)
 *
 * Sucht nach der property mit dem Namen name.
 *
 * Gibt einen Zeiger auf die gefundene property-Struktur zurück, oder 0,
 * wenn sie nicht gefunden wurde.
 *
 */
property *props_find_byname(prop_list *props, const char *name)
{
  int idx;

  ptr_check;
  timer(0);
  idx= props_find_name_pos(props, name);
  s_name.kclocks+= (timer(1)>>10);
  s_name.nclocksamples++;

  if( idx<props->size && !strcmp(props->by_name[idx]->name, name) )
    return props->by_name[idx];
  else
    //~ printf("prop_list: find: name[%d]!=%s\ndump:\n", idx, name);
    //~ props_dump(props);
    //~ puts("");
    return 0;
}



/* int props_add(prop_list *props, char *name, int id, int type, char *desc)
 *
 * Sortiert eine neue property in die arrays ein.
 *
 * Gibt die Anzahl der properties zurück, oder 0, wenn nicht genug Speicher
 * frei ist.
 *
 */
int props_add(prop_list *props, const char *name, int id, prop_t value, int type, const char *desc)
{
  int idx_id, idx_name;
  property *newprop;

  ptr_check;

    idx_id= props_find_id_pos(props, id);

    if(idx_id<props->size && props->by_id[idx_id]->id==id)
      { printf("prop_list: add: id %d already at position %d\n", id, idx_id); return 0; }

    idx_name= props_find_name_pos(props, name);

    if(idx_name<props->size && !strcmp(props->by_name[idx_name]->name, name))
      { printf("prop_list: add: name '%s' already at position %d\n", name, idx_name); return 0; }

  if( !((props->size+1)%PCHUNKSIZE) )
  {
    int bufsize= (props->size+1+PCHUNKSIZE) * sizeof(property*);
    props->by_id= (property**)realloc(props->by_id, bufsize);
    props->by_name= (property**)realloc(props->by_name, bufsize);
    if( !props->by_id || !props->by_name )
      return 0;
  }

  if( !(newprop= (property*)malloc(sizeof(property))) )
    return 0;

  if( !(newprop->name= strdup(name)) )
    return 0;
  newprop->id= id;
  if(type==PROP_STRING)
  {
      if( !(newprop->pvalue= strdup((char*)value)) )
	  return 0;
  }
  else
      newprop->ivalue= value;

  newprop->type= type;

  if( !(newprop->desc= strdup(desc)) )
    return 0;

  if(props->size==0)
    props->by_id[0]= props->by_name[0]= newprop;
  else
  {
    props_insert_byid(props, newprop, idx_id);
    props_insert_byname(props, newprop, idx_name);
  }

  return ++props->size;
}


/* int props_set_byid(prop_list *props, int id, dword value)
 *
 * Setzt den Wert der property mit dem ID.
 *
 * Gibt nicht-null zurück, wenn das property gesetzt wurde, null, wenn
 * es nicht gefunden wurde.
 *
 */
int props_set_byid(prop_list *props, int id, prop_t value)
{
  int idx;

  ptr_check;
  idx= props_find_id_pos(props, id);

  if( idx>=props->size||props->by_id[idx]->id!=id )
  {
    return 0;
  }

  //~ conoutf("props_set_byid(): 0x%08X -- %d = %d", props->by_id[idx], idx, value);

  props->by_id[idx]->ivalue= value;

  return 1;
}

/* int props_set_byname(prop_list *props, char *name, dword value)
 *
 * Setzt den Wert der property mit dem Namen NAME.
 *
 * Gibt nicht-null zurück, wenn das property gesetzt wurde, null, wenn
 * es nicht gefunden wurde.
 *
 */
int props_set_byname(prop_list *props, char *name, prop_t value)
{
  int idx;

  ptr_check;
  idx= props_find_name_pos(props, name);

  if( idx>=props->size || strcmp(props->by_name[idx]->name, name) )
    return 0;

  props->by_name[idx]->ivalue= value;

  return 1;
}



/* int props_free_byid(prop_list *props, dword id)
 *
 * Löscht das property mit dem ID id aus den arrays.
 *
 * Gibt nicht-null zurück, wenn das property gelöscht wurde, null, wenn
 * es nicht gefunden wurde.
 *
 */
int props_free_byid(prop_list *props, dword id)
{
  int idx;
  int idx2;

  ptr_check;
  idx= props_find_id_pos(props, id);

  if( idx>=props->size||props->by_id[idx]->id!=id )
    return 0;

  idx2= props_find_name_pos(props, props->by_id[idx]->name);

  free(props->by_id[idx]->name);
  free(props->by_id[idx]);
  memmove(props->by_id+idx, props->by_id+idx+1,
          (props->size-idx)*sizeof(property*));

  memmove(props->by_name+idx2, props->by_name+idx2+1,
          (props->size-idx2)*sizeof(property*));

  props->size--;
  return 1;
}


/* int props_free_byname(prop_list *props, char *name)
 *
 * Löscht das property mit dem Namen name aus den arrays.
 *
 * Gibt nicht-null zurück, wenn das property gelöscht wurde, null, wenn
 * es nicht gefunden wurde.
 *
 */
int props_free_byname(prop_list *props, char *name)
{
  int idx;
  int idx2;

  ptr_check;
  idx= props_find_name_pos(props, name);

  if( idx>=props->size||strcmp(props->by_name[idx]->name, name) )
    return 0;

  idx2= props_find_id_pos(props, props->by_name[idx]->id);

  free(props->by_name[idx]->name);
  free(props->by_name[idx]);
  memmove(props->by_name+idx, props->by_name+idx+1,
          (props->size-idx)*sizeof(property*));

  memmove(props->by_id+idx2, props->by_id+idx2+1,
          (props->size-idx2)*sizeof(property*));

  props->size--;
  return 1;
}


prop_list *props_clone(prop_list *props)
{
  prop_list *newprops;
  int i;

  ptr_check;

  if( !(newprops= (prop_list *)calloc(1, sizeof(prop_list))) )
    return 0;

  // @! Warum brauche ich da Platz fuer 1 item mehr? (Sonst Buffer Overrun)
  if( !(newprops->by_id= (property **)calloc(props->size+1, sizeof(property*))) )
    return 0;

  if( !(newprops->by_name= (property **)calloc(props->size+1, sizeof(property*))) )
    return 0;

  for(i= props->size-1; i>=0; i--)
  {
    if( !(newprops->by_id[i]= (property*)malloc(sizeof(property))) )
      return 0;

    *newprops->by_id[i]= *props->by_id[i];

    newprops->by_id[i]->name= strdup(props->by_id[i]->name);
    newprops->by_id[i]->desc= strdup(props->by_id[i]->desc);

    if(props->by_id[i]->type==PROP_DWORD)
      newprops->by_id[i]->ivalue= props->by_id[i]->ivalue;
    else if(props->by_id[i]->type==PROP_STRING)
      newprops->by_id[i]->pvalue= strdup(props->by_id[i]->pvalue);
    else
    {
      printf("props_clone: unknown prop type %d\n", props->by_id[i]->type);
      return 0;
    }
  }

  newprops->size= props->size;

  for(i= props->size-1; i>=0; i--)
  {
    if( !(newprops->by_name[i]= props_find_byid(newprops, props->by_name[i]->id)) )
    {
      printf("props_clone: id %d not found\n", props->by_id[i]->id);
      //~ puts("dump:");
      //~ props_dump(props);
      return 0;
    }
  }

  return newprops;
}




/*
 * Testfunktion, zeigt alle props an
 */
void props_dump(prop_list *props)
{
  int i; //, k= 0;
  //~ puts("Nach ID:");
  //~ for(i= 0; i<props->size; i++)
  //~ {
    //~ printf("%4d: %s %s\n", props->by_id[i]->id, props->by_id[i]->name,
           //~ props->by_id[i]->id>=k? "": "<--");

    //~ if(props->by_id[i]->type==PROP_STRING)
      //~ printf("value: \"%s\"\n", props->by_id[i]->value);
    //~ else
      //~ printf("value: %08X\n", props->by_id[i]->value);

    //~ if(k<props->by_id[i]->id) k= props->by_id[i]->id;
  //~ }
  //~ puts("\nNach Name:");
  for(i= 0; i<props->size; i++)
  {
    printf("%s (%s)  ", props->by_name[i]->name, props->by_name[i]->type==PROP_DWORD? "int": "str");
  }
    puts("");
}


#if 0

/*
 * Testfunktion, Zufallswert zwischen 0 und n
 */
int unique_rand(int n)
{
#define MAXRAND 64000
  int i;
  static int table[MAXRAND]= { -2 };

  if(table[0]==-2)
    for(i= 0; i<MAXRAND; i++)
      table[i]= i;

  i= random()%n;
  while(table[i%n]==-1) i++;
  table[i%n]= -1;
  return i%n;
}



int main(int argc, char *argv[])
{
  int i, k, l;
  char ch[50];
  FILE *f;
  prop_list props;
  property *tp;
  int errors= 0;

  if( !props_init(&props) )
    printf("init failed\n"), exit(1);


  f= fopen("stuff.txt", "r");
  i= 0;
  while( fscanf(f, "%49s\n", ch)==1 && ++i<MAXRAND )
    if( !props_add(&props, ch, unique_rand(MAXRAND-1)) ) exit(1);
  fclose(f);
  printf("\n ================== %d entries ================== \n", props.size); fflush(stdout);
  show_all(&props);


  puts("\n\n ================== find-test: ================== \n\nSuche nach ID:"); fflush(stdout);
  for(k= 0; k<10000; k++)
  {
    l= rand()%props.size;
    i= props.by_id[l]->id;
    tp= props_find_byid(&props, i);
    if(tp)
      printf("%4d %4d: %s\n", i, tp->id, tp->name);
    else puts("---------------------not found!---------------------"), errors++;
  }

  puts("\nSuche nach Name:");
  for(k= 0; k<10000; k++)
  {
    i= rand()%props.size;
    tp= props_find_byname(&props, props.by_name[i]->name);
    if(tp)
      printf("%4d: %s \t\t\"%s\"\n", tp->id, tp->name, props.by_name[i]->name);
    else puts("---------------------not found!---------------------"), errors++;
  }


  printf("\n\n%d Fehler\n", errors);

  printf("\nid searches\n");
  printf("ncalls: %d\no: %d\no/call: %.2f\n", s_id.ncalls, s_id.o,
         (double)s_id.o/s_id.ncalls);
  printf("nfwd/call: %.2f\nnbwd/call: %.2f\n", (double)s_id.nfwd/s_id.ncalls,
         (double)s_id.nbwd/s_id.ncalls);

  printf("\nname searches\n");
  printf("ncalls: %d\no: %d\no/call: %.2f\n", s_name.ncalls, s_name.o,
         (double)s_name.o/s_name.ncalls);
  printf("nfwd/call: %.2f\nnbwd/call: %.2f\n", (double)s_name.nfwd/s_name.ncalls,
         (double)s_name.nbwd/s_name.ncalls);
  printf("clocks/srch: %.f\n", (double)s_name.kclocks*1024.0/s_name.nclocksamples);

  fflush(stdout);


  puts("\n ================== free-test ================== ");
  for(k= i= props.size/2; i>0; i--)    // die Hälfte löschen
    props_free_byid(&props, props.by_id[rand()%props.size]->id);
  printf("%d gelöscht, %d übrig\n", k, props.size); fflush(stdout);

  puts("Alle Entries:");
  show_all(&props);


  // Speicher freigeben
  props_free(&props);

  // Fehler
  puts("\nptr_check test: props_add mit ge-free-ter struct:");
  fflush(stdout);
  props_add(&props, "asdf", 15);

  return 0;
}


#endif



