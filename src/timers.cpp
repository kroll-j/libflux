#include <stdio.h>
#include "flux.h"
#include "sys.h"
#include "flux-edm.h"


#define MAX_TIMERS      256     // Vielleicht spaeter dynamisch


typedef struct
{
  int id;
  dword interval;       // Millisekunden
  dword next_tick;
  timer_func tick_func;
  dword tick_arg;
} timer_struct;


static int n_timers;
static timer_struct timers[MAX_TIMERS];


// Timer nach next_tick einsortieren
static void re_sort_timer(int idx)
{
  int i;

  for(i= idx; i>0; i--)
  {
    if(timers[i].next_tick < timers[i-1].next_tick)
    {
      timer_struct tmp= timers[i-1];
      timers[i-1]= timers[i];
      timers[i]= tmp;
    }
  }
}


// Timerfunktionen ausfuehren
void run_timers()
{
  int i;
  dword time= sys_msectime();

  // Die ersten Timer durchgehen
  for(i= 0; i<n_timers && timers[i].next_tick<time; i++)
  {
    timers[i].next_tick+= timers[i].interval;
    timers[i].tick_func(timers[i].tick_arg, timers[i].id, time);
  }

  // Timer neu sortieren, wenn welche ausgefuehrt worden sind
  for(i--; i>0; i--)
    re_sort_timer(i);
}




// Neuen Timer erstellen
int timer_create(timer_func tick_func, prop_t tick_arg, dword interval)
{
  static int timer_id= 0;

  if(!tick_func || !interval || n_timers>=MAX_TIMERS) return 0;

  // Parameter setzen
  timers[n_timers].id= ++timer_id;
  timers[n_timers].tick_func= tick_func;
  timers[n_timers].tick_arg= tick_arg;
  timers[n_timers].interval= interval;
  timers[n_timers].next_tick= sys_msectime()+interval;

  // und einsortieren
  re_sort_timer(n_timers);

  n_timers++;
  return timer_id;
}


// Timer killen
int timer_kill(int id)
{
  int idx;

  // Timer-Index finden
  for(idx= n_timers-1; idx>=0 && timers[idx].id!=id; idx--)
    ;

  if(timers[idx].id!=id) return 0;

  // "Spaetere" Timer nach vorne schieben
  for(int i= idx; i<n_timers-1; i++)
    timers[i]= timers[i+1];

  n_timers--;
  return true;
}
