#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "defs.h"
#include "flux.h"
#include "action_queue.h"
#include "prop_list.h"
#include "flux-edm.h"

// Action Queue. Wird vom "User-Code" gefuellt und beim naechsten Ablauf
// des main loops ausgefuehrt. "q" zeigt immer auf den Eintrag, der zuletzt
// hinzugefuegt wurde.
static action_queue *q;

// Groeszen von struct action, index ist enum action_type
int aq_struct_sizes[]=
{
  4,    // w_close
  4,    // w_minimize
};

// haengt eine neue "struct action" mit Groesze structsize an q an.
// gibt q zurueck.
// Moege die Welt untergehen wenn calloc fehlschlaegt.
action_queue *aq_push_struct(int structsize)
{
  // @! Ich sollte das als union machen, ohne "self"

  // Wenn noch kein Eintrag in der queue steht
  if(!q)
  {
    // erster Eintrag in der queue
    q= (action_queue *)calloc(sizeof(action_queue), 1);
    q->self= (action *)calloc(structsize+4, 1);
    return q;
  }

  // Sonst q->next auf neuen Eintrag und q weiterschalten
  q->next= (action_queue *)calloc(sizeof(action_queue), 1);
  q->next->prev= q;
  q= q->next;
  q->self= (action *)calloc(structsize+4, 1);

  return q;
}


// Neue action in die queue. action ist der typ, z. B. AQ_W_CLOSE.
// Restliche Parameter haengen von action ab.
// return: true fuer ok, false fuer Fehler (action unbekannt).
// Wird von aussen aufgerufen.
bool aq_push(enum action_type action, ...)
{
  va_list ap;

  // aq_struct_sizes[] muss fuer jede action einen Groeszeneintrag haben
    int a= (int)action;
  if( a<0 || a>=sizeof(aq_struct_sizes)/4 )
  {
    printf("tried to push unknown action %d to queue\n", action);
    return false;
  }

  // neue action struct
  action_queue *next= aq_push_struct(aq_struct_sizes[action]);
  next->self->type= action;

  va_start(ap, action);

  // action-parameter setzen
  switch(action)
  {
    case AQ_W_CLOSE:
    {
      action_w_close *atn= (action_w_close *)next->self;
      atn->id= va_arg(ap, dword);
      break;
    }

    case AQ_W_MINIMIZE:
    {
      action_w_minimize *atn= (action_w_minimize *)next->self;
      atn->id= va_arg(ap, dword);
      break;
    }
  }

  va_end(ap);
  return true;
}




// Arbeitet die Befehle in der queue ab.
// Wird von aussen aufgerufen.
void aq_exec()
{
  action_queue *walkq= q;

  if(!walkq) return;

  // zum Anfang der Queue
  while(walkq->prev) walkq= walkq->prev;

  // alle actions ausfuehren
  while(walkq)
  {
    switch(walkq->self->type)
    {
      case AQ_W_CLOSE:
      {
        action_w_close *atn= (action_w_close *)walkq->self;
        wnd_destroy(atn->id);
        break;
      }

      case AQ_W_MINIMIZE:
      {
        action_w_minimize *a= (action_w_minimize *)walkq->self;

        wnd_show(a->id, false);
        dword icon= (dword)wnd_getprop(a->id, "desktop_icon");

        if(!icon)
        {
          icon= clone_group("dskicon", 0, 64,64, 64,64, ALIGN_LEFT|ALIGN_TOP);
          wnd_setprop(icon, "parent", (prop_t)a->id);
          wnd_prop_add(a->id, "desktop_icon", (prop_t)icon, PROP_DWORD);
        }
        else
          wnd_show(icon, true);

        break;
      }

      default:
        printf("unknown action %d in queue\n", walkq->self->type);
    }

    free(walkq->self);
    action_queue *tmp= walkq;
    walkq= walkq->next;
    free(tmp);
  }

  // Queue ist jetzt leer, also auf 0 setzen.
  q= 0;
}




