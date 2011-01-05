#ifndef ACTION_QUEUE_H
#define ACTION_QUEUE_H
#include "defs.h"

enum action_type
{                       // params:
  AQ_W_CLOSE= 0,        // dword id
  AQ_W_MINIMIZE         // dword id
};

typedef struct action
{
  enum action_type type;
} action;

typedef struct action_w_close: public action
{
  dword id;
} action_w_close;

typedef struct action_w_minimize: public action
{
  dword id;
} action_w_minimize;

typedef struct action_queue
{
  struct action_queue *prev, *next;
  struct action *self;
} action_queue;


// "API" Funktionen
bool aq_push(enum action_type action, ...);
void aq_exec();


#endif
