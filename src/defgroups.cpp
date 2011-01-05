// Default Flux Window Groups
// Don't consider this code as a good example of how to use Flux. Much of it is quite old and I would
// do it differently today...

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include "flux.h"
#include "sys.h"
#include "prop_list.h"
#include "flux-edm.h"


// ------------------------ Scroll Box ------------------------

prop_t hscrlbox_cbprops(prop_t arg, struct primitive *self,
                     int type, const char *name, int id, prop_t value)
{
  switch(type)
  {
//    case PROP_ADD:
    case PROP_SET:
      if( !strcmp(name, "knobsize") )
      {
        wnd_setsize(wnd_walk(self->id, CHILD, NEXT, CHILD, SELF), MAXSCALE, value);
      }
      else if( !strcmp(name, "knobpos") )
      {
        dword knobid= wnd_walk(self->id, CHILD, NEXT, CHILD, SELF);
        int knobheight= wnd_getrelh(knobid);
        wnd_sety(knobid, value*(MAXSCALE-knobheight)/MAXSCALE);
      }
      else if( !strcmp(name, "scrollpos") )
      {
        dword knobid= wnd_walk(self->id, CHILD, NEXT, CHILD, SELF);
        int knobheight= wnd_getrelh(knobid);
        if((int)value<0) value= 0;
        else if(value>MAXSCALE) value= MAXSCALE;
        wnd_sety(knobid, value*(MAXSCALE-knobheight)/MAXSCALE);

        scrlbox_onchange callback= (scrlbox_onchange)wnd_getprop(self->id, "on_change");
        if(callback) callback(self->id, value);
      }
      return value;


    case PROP_GET:
      if( !strcmp(name, "scrollpos") )
      {
        dword knobid= wnd_walk(self->id, CHILD, NEXT, CHILD, SELF);
        int knobheight= wnd_getrelh(knobid);
        int knobpos= wnd_getrely(knobid);
// @!
        return knobpos*(MAXSCALE+knobheight)/MAXSCALE;
      }
      return value;

    default:
      return value;
  }
}

int hscrlbox_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  static dword moving= 0;
  static pos movestart;

  switch(event)
  {
    case MOUSE_DOWN:
      if(btn!=MOUSE_BTN1) break;
      moving= self->id;
      movestart.x= x;
      movestart.y= y;
      wnd_set_mouse_capture(self->id);
      break;

    case MOUSE_UP:
      moving= 0;
      wnd_set_mouse_capture(0);
      break;

    case MOUSE_OVER:
      if(moving==self->id)
      {
        rect rc, prc;
        prim_get_abspos(self->parent, &prc, true);
        prim_get_abspos(self, &rc, true);
        prc+= self->parent->rcnonframe;

        int x= mouse_x()-prc.x-movestart.x;
        int y= mouse_y()-prc.y-movestart.y;

        if(x<0) x= 0; else if(x>prc.rgt) x= prc.rgt;
        if(y<0) y= 0; else if(y>prc.btm) y= prc.btm;

        //~ int x_rel= (prc.rgt-prc.x? x*MAXSCALE / (prc.rgt-prc.x): 0);
        int y_rel= (prc.btm-prc.y? y*MAXSCALE / (prc.btm-prc.y): 0);

        if(y_rel<0) y_rel= 0;
        else if(y_rel>MAXSCALE-(self->btm-self->y)) y_rel= MAXSCALE-(self->btm-self->y);

        wnd_setpos(self->id, 0, y_rel);

        dword parent= wnd_walk(self->id, PARENT, PARENT, SELF);

        scrlbox_onchange callback= (scrlbox_onchange)wnd_getprop(parent, "on_change");
        if(callback)
        {
          int btnheight= wnd_geth(self->id);
          int prntheight= prc.btm-prc.y-btnheight;
          if(y<0) y= 0;
          else if(y>prntheight) y= prntheight;
          y_rel= y*MAXSCALE / prntheight;
          callback(parent, y_rel);
        }
      }
      break;
  }
    return 0;
}




typedef struct
{
  int idscrlbar;
  int speed;
} scrlbtn_struct;

void scrlbox_scrollbtn_timer(scrlbtn_struct *arg, int idtimer, dword time)
{
  if(!mouse_btns()&1)
  {
    free(arg);
    timer_kill(idtimer);
    return;
  }

  wnd_setprop(arg->idscrlbar, "scrollpos",
               wnd_getprop(arg->idscrlbar, "scrollpos")+arg->speed);
}


void scrlboxbtn_scrlup(dword btnid, int mousebtns)
{
  if(!mousebtns)
  {
    scrlbtn_struct *arg= (scrlbtn_struct *)malloc(sizeof(scrlbtn_struct));
    arg->idscrlbar= wnd_walk(btnid, PARENT, SELF);
    arg->speed= -MAXSCALE*5/100;
    timer_create((timer_func)scrlbox_scrollbtn_timer, (prop_t)arg, 100);
  }

/*
  if(mousebtns)
  {
    dword parent= wnd_walk(btnid, PARENT, SELF);
    wnd_setprop(parent, "scrollpos",
                 wnd_getprop(parent, "scrollpos")-MAXSCALE*5/100);
  }
*/
}
void scrlboxbtn_scrldn(dword btnid, int mousebtns)
{
  if(mousebtns)
  {
    dword parent= wnd_walk(btnid, PARENT, SELF);
    wnd_setprop(parent, "scrollpos",
                 wnd_getprop(parent, "scrollpos")+MAXSCALE*5/100);
  }
}


void create_scrollbox_group()
{
  dword group= create_group("scrollbox");
  wnd_set_props_callback(group, hscrlbox_cbprops, 0);

  //~ dword frame=
    clone_frame("loweredframe0", group);

  dword bakgnd= create_rect(group, 0,10, MAXSCALE,10, COL_ITEMLO,
                            ALIGN_TOP|ALIGN_BOTTOM|WREL);

  dword button= create_rect(bakgnd, 0,0, MAXSCALE,MAXSCALE/8, COL_ITEM, YREL|WREL|HREL);
  clone_frame("raisedframe0", button);
  wnd_set_mouse_callback(button, hscrlbox_cbmouse, 0);

  button= clone_group("titlebtn", group, 0,0, MAXSCALE,10, ALIGN_TOP|WREL);
  wnd_setprop(button, "font", (prop_t)FONT_SYMBOL);
  wnd_setprop(button, "text", (prop_t)"+"); //"\x84");
  wnd_setprop(button, "on_click", (prop_t)scrlboxbtn_scrlup);

  button= clone_group("titlebtn", group, 0,0, MAXSCALE,10, ALIGN_BOTTOM|WREL);
  wnd_setprop(button, "font", (prop_t)FONT_SYMBOL);
  wnd_setprop(button, "text", (prop_t)"-"); //"\x83");
  wnd_setprop(button, "on_click", (prop_t)scrlboxbtn_scrldn);

  wnd_prop_add(group, "knobsize", MAXSCALE/8, PROP_DWORD);
  wnd_prop_add(group, "knobpos", 0, PROP_DWORD);
  wnd_prop_add(group, "scrollpos", 0, PROP_DWORD);
  wnd_prop_add(group, "on_change", 0, PROP_DWORD);
}







// ------------------------ Dropdown Liste ------------------------



// "on_click" Funktion fuer Dropdown-Button
void dropdnbtn_onclick(int id, int btn)
{
  if(btn)
  {
    dword parentid= wnd_walk(id, PARENT, PARENT, SELF);
    int extended= wnd_getprop(parentid, "extended");

    if(extended)
      wnd_setprop(parentid, "extended", 0);
    else
      wnd_setprop(parentid, "extended", 1);
  }
}

#define LBITEMTRANS 0

// Maus-Callback fuer Dropdown items
int lbitem_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  dword idbase= wnd_walk(self->id, CHILD, SELF);        // Hintergrund

  // wenn "arg" nicht-null ist, ist es ein statisches item (Separator)

  if(event==MOUSE_IN && !arg)
  {
    if(btn) rect_setcolor(idbase, COL_ITEMLO|LBITEMTRANS);
    else rect_setcolor(idbase, COL_ITEMHI|LBITEMTRANS);
    void (*cb)(dword id, int btn, bool selected)= (void(*)(dword,int,bool))wnd_getprop( wnd_walk(self->id, PARENT, SELF), "on_select" );
    if(cb) cb(self->id, btn, true);
  }
  else if(event==MOUSE_OUT && !arg)
  {
    void (*cb)(dword id, int btn, bool selected)= (void(*)(dword,int,bool))wnd_getprop( wnd_walk(self->id, PARENT, SELF), "on_select" );
    if(cb) cb(self->id, btn, false);
    rect_setcolor(idbase, COL_ITEM|LBITEMTRANS);
  }

  else if(event==MOUSE_DOWN && !arg)
    rect_setcolor(idbase, COL_ITEMLO|LBITEMTRANS);

  else if(event==MOUSE_UP && !arg)
  {
    rect rc;
    prim_get_abspos(self, &rc, true);

    btn_cbclick callback= (btn_cbclick)wnd_getprop(self->id, "on_click");

    if(ptinrect(x+rc.x, y+rc.y, rc) && callback)
      callback(self->id, 1, 0);

    rect_setcolor(idbase, COL_ITEMHI|LBITEMTRANS);
  }

  return 0;
}


// "privdata" fuer listbox liste
struct lblist_data
{
  int index;            // "index" property, Index in item-liste
  int n_items;          // "n_items" property, Anzahl Items
  // @!
  dword items[64];      // Fenster-IDs der items
};


// status callback fuer Dropdown-Liste
int dd_cbstatus(prop_t arg, struct primitive *self, int type)
{
    switch(type)
    {
    	case STAT_CREATE:
    	{
	    dword oldlist= (dword)wnd_getprop(self->id, "list_id");
	    // Dropdown Liste
	    dword lst= clone_group("dropdown_list", NOPARENT, 0,0, 0,0, ALIGN_LEFT|ALIGN_TOP);
	    wnd_show(lst, false);
	    wnd_prop_add(self->id, "list_id", (prop_t)lst, PROP_DWORD);
	    wnd_setprop(lst, "parent", self->id);

	    if(oldlist)
	    {
		lblist_data *privdata= (lblist_data*) wlist_find(windows, oldlist)->self->privdata;
		int n= privdata->n_items;
		for(int i= 0; i<n; i++)
		{
		    wnd_setprop(oldlist, "index", i);
		    char *text= (char*)wnd_getprop(oldlist, "item_text");
		    char *data= (char*)wnd_getprop(oldlist, "item_data");
		    wnd_setprop(lst, "append_item", (prop_t)text);
		    wnd_setprop(lst, "item_data", (prop_t)data);
		}
		wnd_setprop(lst, "on_click", wnd_getprop(oldlist, "on_click"));
	    }

	    return 1;
    	}

    	case STAT_DESTROY:
    	{
	    // Dropdown Liste schließen
	    dword lst= wnd_getprop(self->id, "list_id");
	    if(lst) wnd_close(lst);
	    return 1;
    	}
    }
    return 0;
}

// props callback fuer Dropdown-Liste
prop_t dd_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  switch(type)
  {
    case PROP_ADD:
      return value;

    case PROP_SET:
      if(!strcmp(name, "extended"))
      {
        if(value)
        {
          rect rc;
          wnd_get_abspos(wnd_walk(self->id, CHILD, SELF), &rc);

          dword lst= wnd_getprop(self->id, "list_id");
          wnd_setpos(lst, rc.x, rc.btm);
	  wnd_setisize(lst, rc.rgt-rc.x, wnd_getprop(lst, "height"));
          wnd_set_zpos(lst, Z_TOP);
          wnd_totop(lst);
          wnd_show(lst, true);

          dword idframe2= wnd_walk(self->id, CHILD, CHILD, NEXT, SELF);
          wnd_show(idframe2, true);

          return true;
        }
        else
        {
          dword lst= wnd_getprop(self->id, "list_id");
          wnd_show(lst, false);

          dword idframe2= wnd_walk(self->id, CHILD, CHILD, NEXT, SELF);
          wnd_show(idframe2, false);

          return false;
        }
      }

      // selection index setzen
      else if(!strcmp(name, "sel_index"))
      {
	dword lst= wnd_getprop(self->id, "list_id");
        //~ int old_idx= wnd_getprop( lst, "index" );
        wnd_setprop(lst, "index", value);
        char *seltext= (char*)wnd_getprop(lst, "item_text");

        if(seltext)
          text_settext( wnd_walk(self->id, CHILD, CHILD, NEXT, NEXT, SELF),
                        seltext );

        void (*callback)(int id, int index)=
                (void(*)(int,int)) wnd_getprop(self->id, "on_select");

        if(callback) callback(self->id, wnd_getprop(lst, "index"));
      }

      // selection index (nach name) setzen
      else if(!strcmp(name, "sel_text"))
      {
	  char *txt= (char*)value;
	  dword lst= wnd_getprop(self->id, "list_id");
	  int n= wnd_getprop(lst, "n_items");
	  for( int i= 0; i<n; i++ )
	  {
	      wnd_setprop(lst, "index", i);
	      char *itemtext= (char*)wnd_getprop(lst, "item_text");
	      if(!strcmp(txt, itemtext))
	      {
		  wnd_setprop(self->id, "sel_index", i);
		  return i;
	      }
	  }
	  return 0;
      }

      // Index setzen
      else if(!strcmp(name, "index"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "index", value);
      }

      // Item am Index setzen
      else if(!strcmp(name, "item_text"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "item_text", value);
      }

      else if(!strcmp(name, "item_data"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "item_data", value);
      }

      // Item vor dem Index einfuegen
      else if(!strcmp(name, "insert_item"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "insert_item", value);
      }

      // Item am Ende anfuegen
      else if(!strcmp(name, "append_item"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "append_item", value);
      }

      // Item loeschen
      else if(!strcmp(name, "delete_item"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_setprop(lst, "delete_item", value);
      }

      return value;

    case PROP_GET:
      // Item-name am Index abfragen
      if(!strcmp(name, "item_text"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_getprop(lst, "item_text");
      }

      else if(!strcmp(name, "item_data"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_getprop(lst, "item_data");
      }

      // index abfragen
      else if(!strcmp(name, "index"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_getprop(lst, "index");
      }

      // n_items abfragen
      else if(!strcmp(name, "n_items"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_getprop(lst, "n_items");
      }

      // Hoehe der Liste berechnen
      else if(!strcmp(name, "height"))
      {
        dword lst= wnd_getprop(self->id, "list_id");
        return wnd_getprop(lst, "height");
      }

    default:
      return value;
  }
}




// props callback fuer Listbox items
prop_t lbitem_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  switch(type)
  {
    case PROP_ADD:
    case PROP_SET:

      if(!strcmp(name, "text"))
        text_settext( wnd_walk(self->id, CHILD, NEXT, SELF), (char*)value );

      return value;

    case PROP_GET:
      return value;
  }
    return 0;
}

// "on_click" fuer dropdown items
void dropdownitem_onclick(dword id, int btn)
{
    dword idlist= wnd_walk(id, PARENT, SELF);
    dword idparent= wnd_getprop(idlist, "parent");
    wnd_setprop(idparent, "sel_index", wnd_getprop(id, "id"));
    wnd_setprop(idparent, "extended", false);
}


// Maus-Callback fuer Dropdown
int dd_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  if(event==MOUSE_DOWN)
  {
    dword parent= wnd_walk( self->id, PARENT, SELF );
    dword extended= wnd_getprop(parent, "extended");
    if(extended) wnd_setprop(parent, "extended", false);
    else wnd_setprop(parent, "extended", true);
  }
//  else if(event==MOUSE_UP)
//    wnd_setprop(wnd_walk(self->id, PARENT, SELF), "extended", false);

  return 0;
}


#define LBIDEFHEIGHT		14

// props callback fuer Listbox-Liste
prop_t lblist_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  lblist_data *data= (lblist_data *)self->privdata;
  if(!data) { logmsg("lblist_cbprops: self->privdata==NULL\n"); exit(1); }

  switch(type)
  {
    case PROP_ADD:
    case PROP_SET:
      // Index setzen
      if(!strcmp(name, "index"))
      {
        if((signed)value<0) value= 0;
        if((signed)value>data->n_items) value= data->n_items;
        data->index= value;
	return value;
      }

      else if(!strcmp(name, "on_click"))
      {
	  for(int i= 0; i<data->n_items; i++)
	      wnd_setprop(data->items[i], "on_click", value);
	  return value;
      }

      // Item am Index setzen
      else if(!strcmp(name, "item_text") || !strcmp(name, "item_data"))
      {
        if(!value || !data->n_items) return 0;

        int i= data->index;
        if(i<0) i= 0; else if(i>data->n_items-1) i= data->n_items-1;

        wnd_setprop(data->items[i], !strcmp(name, "item_data")? (char*)"data": (char*)"text", value);

        return value;
      }

      // Item vor dem Index einfuegen
      else if(!strcmp(name, "insert_item"))
      {
        if(!value || data->n_items>62) return 0;

        int i= data->index;
        if(i<0) i= 0;
        else if(data->n_items && i>data->n_items-1) i= data->n_items-1;

        // Items hinter dem Index nach unten verschieben
        if(data->n_items)
        {
          memmove( data->items+i+1, data->items+i, (data->n_items-i)*4 );

          for(int k= i+1; k<=data->n_items; k++)
          {
            wnd_setpos(data->items[k], 0, k*LBIDEFHEIGHT);
            wnd_setprop(data->items[k], "id", k);
          }
        }

        // Item einfuegen
        data->items[i]= clone_group("dropdown_item", self->id, 0,i*LBIDEFHEIGHT, MAXSCALE,LBIDEFHEIGHT, ALIGN_TOP|WREL);
        wnd_setprop(data->items[i], "text", value);
	wnd_setprop(data->items[i], "on_click", wnd_getprop(self->id, "on_click"));
        wnd_prop_add(data->items[i], "id", i, PROP_DWORD);

	data->n_items++;

	int itemw= font_gettextwidth(FONT_ITEMS, (char*)value);
	if(wnd_getw(self->id)-4 < itemw) wnd_setwidth(self->id, itemw+4);

        wnd_setiheight(self->id, wnd_getprop(self->id, "height"));

        return value;
      }

      // Item am Ende anfuegen
      else if(!strcmp(name, "append_item"))
      {
        if(!value || data->n_items>62) return 0;

        int i= data->n_items;
        dword wnd= data->items[i]= clone_group("dropdown_item", self->id, 0,i*LBIDEFHEIGHT, MAXSCALE,LBIDEFHEIGHT, ALIGN_TOP|WREL);

        if(!strcmp((char *)value, "\\SEPARATOR"))
        {
          wnd_set_mouse_callback(data->items[i], lbitem_cbmouse, 1);
          wnd_destroy(wnd_walk(data->items[i], CHILD, CHILD, SELF));
          clone_group("raisedframe0", data->items[i],
                      0,0, 9000,2, ALIGN_HCENTER|ALIGN_VCENTER|WREL);
        }
        else
        {
          wnd_setprop(data->items[i], "text", value);
	  wnd_setprop(data->items[i], "on_click", wnd_getprop(self->id, "on_click"));
          wnd_prop_add(data->items[i], "id", i, PROP_DWORD);
	  int itemw= font_gettextwidth(FONT_ITEMS, (char*)value);
	  if(wnd_getw(self->id)-8 < itemw) wnd_setwidth(self->id, itemw+8);
        }

        wnd_setprop(self->id, "index", data->n_items++);

        wnd_setiheight(self->id, wnd_getprop(self->id, "height"));

        return wnd;
      }

      // Item loeschen...
      else if(!strcmp(name, "delete_item"))
      {
        if(type==PROP_ADD || (signed)value>=data->n_items)
          return (prop_t)-1;

        for(int i= value; i<data->n_items; i++)
        {
          wnd_sety(data->items[i], (i-1)*LBIDEFHEIGHT);
          wnd_setprop(data->items[i], "id", i);
        }

        memmove(data->items+value, data->items+value+1, (data->n_items-value)*sizeof(data->items[0]));

        data->n_items--;
        wnd_setiheight(self->id, wnd_getprop(self->id, "height"));

        return data->n_items;
      }

      // unbek. property
      return value;

    case PROP_GET:
      // Item-name am Index abfragen
      if(!strcmp(name, "item_text") || !strcmp(name, "item_data"))
      {
        int i= data->index;
        if(i<0) i= 0; else if(i>data->n_items-1) i= data->n_items-1;
        return (prop_t)wnd_getprop(data->items[i], !strcmp(name, "item_text")? (char*)"text": (char*)"data");
      }

      // index abfragen
      else if(!strcmp(name, "index"))
        return data->index;

      // n_items abfragen
      else if(!strcmp(name, "n_items"))
      {
        return data->n_items;
      }

      // Hoehe berechnen
      else if(!strcmp(name, "height"))
        return data->n_items*LBIDEFHEIGHT;

      // unbek. property
      return value;
  }
    return 0;
}


// status callback fuer Listbox-Liste
int lblist_cbstatus(prop_t arg, struct primitive *self, int type)
{
  switch(type)
  {
    case STAT_CREATE:
      self->privdata= (void *)calloc(sizeof(lblist_data), 1);
      return 1;

    case STAT_DESTROY:
      free(self->privdata);
      return 1;
  }
    return 0;
}


void create_dropdown_group()
{
  // fonts fuer text und symbole
  font *ftext, *fsym;
  ftext= FONT_DEFAULT;
  fsym= FONT_SYMBOL;

  int height= font_height(ftext);

  // Auswahlliste
  dword idlist= create_group("dropdown_list");
  rect_setcolor(idlist, COL_WINDOW);
  wnd_setresizable(idlist, false);
    clone_frame("raisedframe0", idlist);
  //~ frame_setcolors(wnd_walk(frm, CHILD, SELF), COL_FRAMEHI, COL_FRAMEHI, COL_FRAMELO, COL_FRAMELO);
  //~ clone_frame("minititlebar", idlist);
  wnd_set_status_callback(idlist, lblist_cbstatus, 0);
  wnd_set_props_callback(idlist, lblist_cbprops, 0);
  wnd_prop_add(idlist, "parent", 0, PROP_DWORD);
  wnd_prop_add(idlist, "index", 0, PROP_DWORD);
  wnd_prop_add(idlist, "insert_item", 0, PROP_DWORD);
  wnd_prop_add(idlist, "append_item", 0, PROP_DWORD);
  wnd_prop_add(idlist, "delete_item", 0, PROP_DWORD);
  wnd_prop_add(idlist, "item_text", 0, PROP_DWORD);
  wnd_prop_add(idlist, "item_data", (prop_t)"", PROP_STRING);
  wnd_prop_add(idlist, "height", 0, PROP_DWORD);
  wnd_prop_add(idlist, "on_click", (prop_t)dropdownitem_onclick, PROP_DWORD);
  wnd_prop_add(idlist, "on_select", 0, PROP_DWORD);
  wnd_prop_add(idlist, "n_items", 0, PROP_DWORD);

  dword idmain= create_group("dropdown");
  wnd_setsize(idmain, 64,height);
  wnd_setresizable(idmain, false);
  wnd_set_status_callback(idmain, dd_cbstatus, 0);
  wnd_set_props_callback(idmain, dd_cbprops, 0);
  wnd_prop_add(idmain, "extended", 0, PROP_DWORD);
  wnd_prop_add(idmain, "sel_index", 0, PROP_DWORD);
  wnd_prop_add(idmain, "sel_text", (prop_t)"", PROP_DWORD);
  wnd_prop_add(idmain, "on_select", 0, PROP_DWORD);

  // von der Auswahlliste "durchgeschleifte" properties
  wnd_prop_add(idmain, "index", 0, PROP_DWORD);
  wnd_prop_add(idmain, "insert_item", 0, PROP_DWORD);
  wnd_prop_add(idmain, "append_item", 0, PROP_DWORD);
  wnd_prop_add(idmain, "delete_item", 0, PROP_DWORD);
  wnd_prop_add(idmain, "item_text", 0, PROP_DWORD);
  wnd_prop_add(idmain, "item_data", (prop_t)"", PROP_DWORD);
  wnd_prop_add(idmain, "height", 0, PROP_DWORD);


  // Hintergrund fuer Auswahltext
  dword idseltextbg= create_rect(idmain, 0,0, MAXSCALE,height+2, COL_ITEM,
                                 ALIGN_LEFT|ALIGN_TOP|WREL);
  wnd_set_mouse_callback(idseltextbg, dd_cbmouse, 0);

  // Frame
  //~ dword idframe=
  clone_frame("raisedframe0", idseltextbg);
  wnd_show( clone_group("loweredframe0", idseltextbg), false );

  // Ausgewaehlter Text
  //~ dword idseltext=
  create_text(idseltextbg, 2,0, MAXSCALE,font_height(ftext),
                               "",
                               COL_ITEMTEXT, ftext,
                               ALIGN_LEFT|ALIGN_VCENTER|WREL);

  // dropdown button
  dword idbtn= clone_group("titlebtn", idseltextbg, 0,0, height,MAXSCALE,
                           ALIGN_RIGHT|ALIGN_TOP|HREL);
  wnd_setprop(idbtn, "text", (prop_t)"\x83");
  wnd_setprop(idbtn, "font", (prop_t)fsym);
  wnd_setprop(idbtn, "on_click", (prop_t)dropdnbtn_onclick);


  // Listbox Item
  dword iditem= create_group("dropdown_item");
  wnd_setsize(iditem, 64,height);
  create_rect(iditem, 0,0, MAXSCALE,MAXSCALE, COL_ITEM, ALIGN_LEFT|ALIGN_TOP|WREL|HREL);
  create_text(iditem, 2,0, MAXSCALE,MAXSCALE, "", COL_ITEMTEXT, ftext, ALIGN_LEFT|ALIGN_TOP|WREL|HREL);
  wnd_set_mouse_callback(iditem, lbitem_cbmouse, 0);
  wnd_set_props_callback(iditem, lbitem_cbprops, 0);
  wnd_prop_add(iditem, "on_click", (prop_t)dropdownitem_onclick, PROP_DWORD);
  wnd_prop_add(iditem, "text", (prop_t)"", PROP_STRING);
  wnd_prop_add(iditem, "data", (prop_t)"", PROP_STRING);
}











// ------------------------ List Box ------------------------



typedef struct
{
  int index;
  int n_items;
  int selected;
  dword lastselect;     // Zeit in msec des letzten Auswahl-Klicks
} listbox_data;


// status callback fuer Listbox
int listbox_cbstatus(prop_t arg, struct primitive *self, int type)
{
  switch(type)
  {
    case STAT_CREATE:
      self->privdata= (void *)calloc(1, sizeof(listbox_data));
      ((listbox_data*)self->privdata)->selected= -1;
      return 1;

    case STAT_DESTROY:
      free(self->privdata);
      return 1;
  }
    return 0;
}


prop_t listbox_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  listbox_data *data= (listbox_data *)self->privdata;

  switch(type)
  {
    case PROP_SET:
    {
      if(!strcmp(name, "append_item"))
      {
        int height= font_height(FONT_ITEMS) + 2;
        int y= data->n_items * height;

        dword item= clone_group("listbox_item", wnd_walk(self->id, CHILD, NEXT, SELF),
                                0,y, MAXSCALE,height, ALIGN_LEFT|ALIGN_TOP|WREL);
        text_settext(wnd_walk(item, CHILD, NEXT, SELF), (char*)value);

        data->n_items++;
      }
      else if(!strcmp(name, "scroll_pos"))
      {
        int wndheight= wnd_geth(self->id) - 2; // -2 wg. frame
        int itemheight= (font_height(FONT_ITEMS)+2);
        int height= data->n_items * itemheight;
        int ypos= (-height+wndheight-itemheight) * (int)value / MAXSCALE;
        ypos= ypos/itemheight*itemheight;
        if(-ypos>height-wndheight) ypos= -(height-wndheight);
        wnd_sety(wnd_walk(self->id, CHILD, NEXT, SELF), ypos);
      }
      else if(!strcmp(name, "index"))
      {
        data->index= value;
      }
      else if(!strcmp(name, "clear"))
      {
        dword item= wnd_walk(self->id, CHILD, NEXT, CHILD, SELF);
        do
        {
          wnd_close(item);
        } while( (item= wnd_walk(item, NEXT, SELF))!=NOWND );

        wnd_sety(wnd_walk(self->id, CHILD, NEXT, SELF), 0);

        data->index= data->n_items= 0;
        data->selected= -1;
      }
      break;
    }

    case PROP_GET:
      if(!strcmp(name, "n_items"))
        return data->n_items;
      else if(!strcmp(name, "item_text"))
      {
        static char text[1024];

        if((signed)value>=data->n_items)
          return 0;

        int item= wnd_walk(self->id, CHILD, NEXT, CHILD, SELF);
        for(int i= 0; i<data->index; i++)
          item= wnd_walk(item, NEXT, SELF);

        text_gettext(wnd_walk(item, CHILD, NEXT, SELF), text, 1023);
        return (prop_t)text;
      }
      break;
  }

  return value;
}


int listbox_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  listbox_data *data= (listbox_data *)self->parent->privdata;
  bool dblclicked= false;

  switch(event)
  {
    case MOUSE_DOWN:
      wnd_set_mouse_capture(self->id);
      if(sys_msectime() - data->lastselect < 250)
        dblclicked= true;
      // fall through

    case MOUSE_OVER:
      if(btn==1)
      {
        int itemheight= font_height(FONT_ITEMS)+2;
        int selected= y/itemheight;

        if(selected<0) selected= 0;
        if(selected>data->n_items-1) selected= data->n_items-1;

        if(selected!=data->selected || event==MOUSE_DOWN)
        {
          dword child;
          int i;

          if(data->selected>=0)
          {
            child= wnd_walk(self->id, CHILD, SELF);
            for(i= 0; i<data->selected; i++)
              child= wnd_walk(child, NEXT, SELF);
            rect_setcolor(child, INVISIBLE);
          }

          child= wnd_walk(self->id, CHILD, SELF);
          for(i= 0; i<selected; i++)
            child= wnd_walk(child, NEXT, SELF);

          rect_setcolor(child, COL_ITEMHI);

          int y= wnd_getrely(self->id);
          int parentheight= wnd_geth(self->parent->id);
          int ypos= selected*itemheight;

          if(ypos+y<0) wnd_sety(self->id, -ypos);
          else if(ypos+y>parentheight-itemheight-2)
          {
            wnd_sety(self->id, -ypos+parentheight-itemheight-2);
          }

          dblclicked&= (data->selected==selected);
          data->lastselect= sys_msectime();
          data->selected= selected;

          listbox_onselect callback= (listbox_onselect)wnd_getprop(self->id, "on_select");
	  if(!callback && self->parent) callback= (listbox_onselect)wnd_getprop(self->parent->id, "on_select");
          if(callback) callback(self->parent->id, selected, dblclicked);
        }
      }
      break;

    case MOUSE_UP:
      wnd_set_mouse_capture(0);
      break;
  }
    return 0;
}




void create_lbitem_group()
{
  dword iditem= create_group("listbox_item");

  create_rect(iditem, 0,0, MAXSCALE,MAXSCALE,
                            INVISIBLE, WREL|HREL);
  create_text(iditem, 4,1, 2,MAXSCALE, "", COL_ITEMTEXT, FONT_ITEMS,
                          ALIGN_LEFT|ALIGN_RIGHT|WREL|HREL);
}



void create_listbox_group()
{
  create_lbitem_group();


  dword idgrp1= create_group("loweredframe2");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 2,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);

  create_frame(idgrp1, 0,0, 0,0, 1,
               COL_FRAMELO|TRANSL_2, COL_FRAMELO|TRANSL_2,
               COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);


  dword idlist= create_group("listbox");
  clone_frame("loweredframe0", idlist);

  dword idrect= create_rect(idlist, 0,0, MAXSCALE,0, COL_ITEM, ALIGN_TOP|ALIGN_BOTTOM|WREL);
  wnd_set_mouse_callback(idrect, listbox_cbmouse, 0);

  wnd_set_status_callback(idlist, listbox_cbstatus, 0);
  wnd_set_props_callback(idlist, listbox_cbprops, 0);
  wnd_prop_add(idlist, "index", 0, PROP_DWORD);
  wnd_prop_add(idlist, "insert_item", 0, PROP_DWORD);
  wnd_prop_add(idlist, "append_item", 0, PROP_DWORD);
  wnd_prop_add(idlist, "scroll_pos", 0, PROP_DWORD);
  wnd_prop_add(idlist, "on_select", 0, PROP_DWORD);
  wnd_prop_add(idlist, "n_items", 0, PROP_DWORD);
  wnd_prop_add(idlist, "item_text", 0, PROP_DWORD);
  wnd_prop_add(idlist, "clear", 0, PROP_DWORD);
}




















void on_color_select(int id, int index)
{
  static dword col_table[][NSYSCOLORS]=
  {
    // Sky Blue
    { 0x303850|TRANSL_2, 0x505070, 0xF0F0FF, 0x506080, 0x607090, 0x304070, 0xE0E0FF,
      0x707090, 0xF8F8FF, 0x000010 },

    // Grey World
    {
      0x303030|TRANSL_2,
      0x505050,
      0xFFFFFF,
      0x606060,
      0x707070,
      0x505050,
      0xF0F0F0,
      0x808080,
      0xFFFFFF,
      0x000000
    },

    // Red Alert
    {
      0x303030|TRANSL_2,
      0x402010,
      0xFFFFFF,
      0x602010,
      0x903020,
      0x301008,
      0xF06040,
      0x803010,
      0xFFA080,
      0x000000
    },

    // Mostly Green
    {
      0x404040|TRANSL_2,
      0x306050,
      0xFFFFFF,
      0x406068,
      0x507078,
      0x305058,
      0xD0E0D0,
      0x508070,
      0xFFFFFF,
      0x000000
    },

    // XP Kindergarten
    {
      0x203050|TRANSL_2,
      0x606090,
      0xFFFFFF,
      0x008000,
      0x0080A0,
      0x004060,
      0xFFFFFF,
      0x3030A0,
      0xFFFFFF,
      0x000000
    }
  };

  memcpy(syscol_table, &col_table[index][0], sizeof(syscol_table));

  rect rc= { 0,0, viewport.rgt,viewport.btm };
  redraw_rect(&rc);
  update_rect(&rc);
}

void on_vidmode_select(int id, int index)
{
  struct { int width, height; } vidmodes[]=
  {
    { 320, 240 },
    { 400, 300 },
    { 640, 480 },
    { 800, 600 },
    { 1024, 768 }
  };

  flux_setvideomode(vidmodes[index].width, vidmodes[index].height, bpp, true);
  redraw_rect(&viewport);
  update_rect(&viewport);
}

void on_bpp_select(int id, int index)
{
  if(index==0) bpp= 16;
  else if(index==1) bpp= 32;

  flux_setvideomode(viewport.rgt, viewport.btm, bpp, true);
  redraw_rect(&viewport);
  update_rect(&viewport);
}


void create_list(dword idwnd, int y, const char *title, void (*cb)(int, int),
                 const char *item0, ...)
{
  dword tmp=
  clone_group("loweredframe0", idwnd, 0,y, MAXSCALE,40, ALIGN_TOP|WREL);

  create_text(tmp, 4,4, 95,16, title, COL_TEXT, FONT_DEFAULT, ALIGN_TOP);

  dword lst= clone_group("dropdown", tmp, 4,20, 4,16,
                         ALIGN_TOP|ALIGN_LEFT|ALIGN_RIGHT);

  va_list ap;
  va_start(ap, item0);
  const char *item= item0;

  while(item)
  {
    wnd_setprop(lst, "append_item", (prop_t)item);
    item= va_arg(ap, char*);
  }

  wnd_setprop(lst, "on_select", (prop_t)cb);
}




void fullscreen_toggle(dword id, bool on)
{
#ifdef FLUX_EDM
    extern void fullscreen(); fullscreen();
#endif
}

void video_dlg(dword parent)
{
  dword newwnd= create_rect(parent, (viewport.rgt-274)>>1,(viewport.btm-280)>>1, 32,32,
                            COL_WINDOW);

  dword frame= clone_frame("titleframe", newwnd);
  wnd_setprop(frame, "title", (prop_t)"Anzeigenoptionszeug");

  wnd_setisize(newwnd, 256,48*4-8);


  create_list(newwnd, 48*0, "Farbschema:", on_color_select,
              "Sky Blue", "Grey World", "Red Alert", "Mostly Green",
              "XP Kindergarten", 0);

  create_list(newwnd, 48*1, "Videomodus:", on_vidmode_select,
              "320x240", "400x300", "640x480", "800x600", "1024x768", 0);

  create_list(newwnd, 48*2, "Farbtiefe:", on_bpp_select,
              "16 Bits/Pixel", "32 Bits/Pixel", 0);


    ////////

    dword cb= clone_group("checkbox", newwnd, 0,48*3, MAXSCALE,16, WREL);
    wnd_setprop(cb, "text", (prop_t)"Fullscreen");
    wnd_setprop(cb, "on_toggle", (prop_t)fullscreen_toggle);
}




// ----------------------------- Title Frame -----------------------------



// Props Callback fuer Title Frame
prop_t tframe_cbprops(prop_t arg, struct primitive *self,
                   int type, const char *name, int id, prop_t value)
{
  switch(type)
  {
    case PROP_GET:
      printf("PROP_GET %s=%d\n", name, (int)value);
      return value;

    case PROP_ADD:
    case PROP_SET:
      if( !strcmp(name, "title") )
      {
        //~ self= self->children->self->children->self->children->self;
        //~ dword id= self->id;
	//~ dword id= wnd_walk(self->id, CHILD, CHILD, CHILD, LAST, PREV, PREV, PREV, PREV, SELF);
	//~ self= findprim(id);
	dword id= wnd_walk(self->id, CHILD, NEXT, CHILD, CHILD, SELF);
	self= findprim(id);
	while(self && self->type!=PT_TEXT)
	    id= wnd_walk(id, NEXT, SELF),
	    self= findprim(id);
        text_settext(id, (char*)value);
	int w= font_gettextwidth( ((prim_text*)self)->font, (char*)value );
        wnd_setwidth(id, w);
      }
      return value;

    default:
      return value;
  }
}


#ifdef FLUX_EDM
extern int wsnap;
#else
int wsnap= 4;
#endif

bool snappos(rect &p, rect &rc)
{
    bool snapped= false;

    if(abs(p.x-rc.x)<wsnap) p.rgt-= p.x-rc.x, p.x= rc.x, snapped= true;
    if(abs(p.y-rc.y)<wsnap) p.btm-= p.y-rc.y, p.y= rc.y, snapped= true;

    if(abs(p.rgt-viewport.rgt)<wsnap) p.x-= p.rgt-viewport.rgt, p.rgt= viewport.rgt, snapped= true;
    if(abs(p.btm-viewport.btm)<wsnap) p.y-= p.btm-viewport.btm, p.btm= viewport.btm, snapped= true;

    return snapped;
}

void snapmove(dword p_id, int xd, int yd)
{
    rect p;

    wnd_get_abspos(p_id, &p);

    p.rgt= xd+(p.rgt-p.x); p.x= xd;
    p.btm= yd+(p.btm-p.y); p.y= yd;

    snappos(p, viewport);

    wnd_setpos(p_id, p.x, p.y);
}

// Mouse Callback fuer Title Bar
int tbar_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  static dword moving= 0;
  static pos move_start;

  switch(event)
  {
    case MOUSE_DOWN:
    {
      if(btn!=MOUSE_BTN1) break;

      moving= self->id;
      move_start.x= x; move_start.y= y;
      wnd_set_mouse_capture(self->id);
      break;
    }

    case MOUSE_UP:
      moving= 0;
      wnd_set_mouse_capture(0);
      break;

    case MOUSE_OVER:
      if(moving==self->id)
      {
	    dword p_id= wnd_walk(self->id, PARENT, PARENT, PARENT, PARENT, SELF);
	    if(p_id==NOWND) p_id= wnd_walk(self->id, PARENT, PARENT, PARENT, SELF);
	    if(p_id==NOWND) p_id= wnd_walk(self->id, PARENT, PARENT, SELF);
	    if(p_id==NOWND) break;
#ifdef FLUX_NDS
	    wnd_setpos(p_id, mouse_x()-move_start.x, mouse_y()-move_start.y);
#else
	    snapmove(p_id, mouse_x()-move_start.x, mouse_y()-move_start.y);
#endif
      }
      break;
  }
    return 0;
}

// Frame X-Button
void btn_close(int id, int btn)
{
  if(!btn)
  {
    int p_id= wnd_walk(id, PARENT, PARENT, PARENT, PARENT, SELF);
    wnd_close(p_id);
  }
}

// Frame _-Button
void btn_minimize(int id, int btn)
{
  if(!btn)
  {
    int p_id= wnd_walk(id, PARENT, PARENT, PARENT, PARENT, SELF);
    wnd_minimize(p_id);
  }
}


void create_titleframe()
{
  dword idtitlegrp= create_group("titleframe");

  create_rect(idtitlegrp, 0,0, 0,0, INVISIBLE, ALIGN_TOP|ALIGN_BOTTOM|ALIGN_LEFT|ALIGN_RIGHT);

  dword idtbar= create_group("titlebar");
  dword tbk= create_rect(idtbar, 0,0, MAXSCALE,font_height(FONT_DEFAULT)+8, COL_TITLE, WREL);
  create_text(tbk, 0,0, 0,font_height(FONT_DEFAULT),
              "", COL_TEXT, FONT_DEFAULT, ALIGN_HCENTER|ALIGN_VCENTER);
  create_rect(tbk, 0,0, MAXSCALE,1, COL_FRAMEHI|TRANSL_1, ALIGN_BOTTOM|WREL);
  create_rect(tbk, 0,1, MAXSCALE,1, COL_FRAMELO|TRANSL_2, ALIGN_BOTTOM|WREL);

  dword bclose= clone_group("titlebtn", tbk, 4,0, 14,14, ALIGN_RIGHT|ALIGN_VCENTER);
  wnd_setprop(bclose, "font", (prop_t)FONT_DEFAULT);
  wnd_setprop(bclose, "text", (prop_t)"x");
  wnd_setprop(bclose, "on_click", (prop_t)btn_close);

  //~ dword bshade= clone_group("titlebtn", tbk, 2,0, 12,12, ALIGN_VCENTER);
  //~ wnd_setprop(bshade, "font", (prop_t)FONT_SYMBOL);
  //~ wnd_setprop(bshade, "text", (prop_t)"\x81");
  //~ wnd_setprop(bshade, "on_click", (prop_t)btn_minimize);

//  dword bmini= clone_group("titlebtn", tbk, 15,0, 12,12, ALIGN_RIGHT|ALIGN_VCENTER);
//  wnd_setprop(bmini, "font", (prop_t)FONT_SYMBOL);
//  wnd_setprop(bmini, "text", (prop_t)"\x82");
//  wnd_setprop(bmini, "on_click", (prop_t)btn_minimize);

  wnd_set_mouse_callback(tbk, tbar_cbmouse, 0);
  wnd_set_props_callback(idtitlegrp, tframe_cbprops, 0);

  clone_frame("titlebar", idtitlegrp);
  clone_frame("raisedframe2", idtitlegrp);
  create_rect(idtitlegrp, 0,0, 6,MAXSCALE, TRANSL_NOPAINT, ALIGN_LEFT|HREL);
  create_rect(idtitlegrp, 0,0, 6,MAXSCALE, TRANSL_NOPAINT, ALIGN_RIGHT|HREL);
  create_rect(idtitlegrp, 0,font_height(FONT_DEFAULT)+4, 100,4, TRANSL_NOPAINT, ALIGN_TOP|WREL);
  create_rect(idtitlegrp, 0,0, MAXSCALE,6, TRANSL_NOPAINT, ALIGN_BOTTOM|WREL);

  wnd_prop_add(idtitlegrp, "title", (prop_t)"", PROP_STRING);

  dword id= create_group("minititlebar");
  tbk= create_rect(id, 0,0, MAXSCALE,6, COL_TITLE|TRANSL_2, WREL);
  char txt[]= "....";
  create_text(tbk, 0,-8, font_gettextwidth(FONT_ITEMS, txt),12, txt, COL_ITEMTEXT, FONT_ITEMS, ALIGN_TOP|ALIGN_HCENTER);
  wnd_set_mouse_callback(tbk, tbar_cbmouse, 0);
  clone_frame("raisedframe0", id);
}




// ----------------------------- Desktop Icon -----------------------------


void icon_showwnd(dword idicon, dword idwnd)
{
  rect rcicon;
  rect rcwnd;
  dword idminiwnd= wnd_walk(idicon, CHILD, NEXT, SELF);

  wnd_get_abspos(idminiwnd, &rcicon);
  wnd_get_abspos(idwnd, &rcwnd);

  int width= rcicon.rgt-rcicon.x,
      height= rcicon.btm-rcicon.y,
      ww= rcwnd.rgt-rcwnd.x,
      wh= rcwnd.btm-rcwnd.y;

  wnd_setsize(idwnd, 0,0);
  wnd_show(idwnd, true);

#define NFRAMES 7
  for(int i= NFRAMES-1; i>=0; i--)
  {
    int wnd_width= ww * (NFRAMES-i)/NFRAMES,
        wnd_height= wh * (NFRAMES-i)/NFRAMES;

#ifdef FLUX_EDM
    extern void tick(); tick();
#endif
    wnd_setsize(idminiwnd, width*i/NFRAMES, height*i/NFRAMES);
    wnd_setsize(idwnd, wnd_width, wnd_height);
    wnd_setpos( idwnd, rcicon.x+(rcwnd.x-rcicon.x)*(NFRAMES-i)/NFRAMES, rcicon.y+(rcwnd.y-rcicon.y)*(NFRAMES-i)/NFRAMES );
    sys_msleep(12);
  }
#undef NFRAMES

  wnd_setsize(idwnd, rcwnd.rgt-rcwnd.x, rcwnd.btm-rcwnd.y);

  wnd_show(idicon, false);
  wnd_setsize(idminiwnd, width, height);
}

int icon_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  static pos move_start;
  static pos wndpos_start;
  static dword moving;
  static dword selrect;

  switch(event)
  {
    case MOUSE_DOWN:
      if(btn!=1)
      {
        dword parentid= wnd_getprop(self->parent->id, "parent");
        icon_showwnd(self->parent->id, parentid);
        return 0;
      }

      move_start.x= mouse_x(); move_start.y= mouse_y();
      wndpos_start.x= self->parent->x; wndpos_start.y= self->parent->y;
      moving= self->id;
      wnd_set_mouse_capture(self->id);
      wnd_show(wnd_walk(self->id, PARENT, CHILD, NEXT, NEXT, SELF), true);
      selrect= create_rect(wnd_walk(self->id, PARENT, PARENT, SELF),
                           (self->parent->x)/16*16+16,
                           (self->parent->y)/16*16,
                           self->rgt-self->x,self->btm-self->y,
                           COL_ITEMHI|TRANSL_2);
      wnd_show(selrect, false);
      wnd_totop(self->parent->id);
      rect_setcolor(self->id, COL_WINDOW|TRANSL_1);
      break;

    case MOUSE_UP:
    {
      if( moving==self->id &&
          move_start.x==mouse_x() && move_start.y==mouse_y() )
      {
        dword parentid= wnd_getprop(self->parent->id, "parent");
        icon_showwnd(self->parent->id, parentid);
      }

      int xa= 8, ya= 8;
      if(self->parent->x<0) xa= -8; if(self->parent->y<0) ya= -8;

      wnd_setpos(self->parent->id, (self->parent->x+xa)/16*16,
                 (self->parent->y+ya)/16*16);

      wnd_show(wnd_walk(self->id, PARENT, CHILD, NEXT, NEXT, SELF), false);

//      aq_push(AQ_W_CLOSE, selrect);
      wnd_close(selrect);

      rect_setcolor(self->id, COL_WINDOW);

      moving= false;
      wnd_set_mouse_capture(0);
      break;
    }

    case MOUSE_OVER:
      if(btn&1 && moving==self->id)
      {
        wnd_setpos( self->parent->id,
                    (wndpos_start.x + mouse_x()-move_start.x),
                    (wndpos_start.y + mouse_y()-move_start.y) );

        wnd_show(selrect, true);

        int xa= 8, ya= 8;
        if(self->parent->x<0) xa= -8; if(self->parent->y<0) ya= -8;

        wnd_setpos(selrect,
                   (self->parent->x+xa)/16*16+16,
                   (self->parent->y+ya)/16*16);
/*
        if(self->parent->x>0)
          wnd_setpos(selrect,
                     (self->parent->x+8)/16*16+16,
                     (self->parent->y+8)/16*16);
        else
          wnd_setpos(selrect,
                     (self->parent->x-8)/16*16+16,
                     (self->parent->y+8)/16*16);
*/

      }
      break;
  }

  return 1;
}


void create_desktopicon()
{
  font *fnt= FONT_ITEMS;
  dword idicon= create_group("dskicon");

  wnd_setsize(idicon, 64,64);   // Nur zur Orientierung

  dword idtext= create_text(idicon, 0,20, font_gettextwidth(fnt, "Text"),12,
                            "Text", COL_ITEMTEXT, fnt, ALIGN_HCENTER);

  dword idminiwnd= create_rect(idicon, 0,0, 28,20, COL_WINDOW, ALIGN_TOP|ALIGN_HCENTER);

  clone_group("raisedframe0", idminiwnd);
  dword idminititle= create_rect(idminiwnd, 1,0, 26,7, COL_TITLE);
  create_text(idminiwnd, 0,-font_height(fnt)+6,
              font_gettextwidth(fnt, "..."),12, "...",
              COL_TEXT, fnt, ALIGN_HCENTER);
  clone_group("raisedframe0", idminititle, 1,2, 3,3, ALIGN_RIGHT);
//  clone_group("raisedframe0", idminititle, 1,2, 3,3, ALIGN_LEFT);
  clone_group("raisedframe0", idminiwnd, 0,0, MAXSCALE,7, ALIGN_LEFT|ALIGN_TOP|WREL);
  clone_group("raisedframe0", idminiwnd, 2,2, 4,3, ALIGN_LEFT|ALIGN_BOTTOM);
  clone_group("raisedframe0", idminiwnd, 8,2, 6,3, ALIGN_LEFT|ALIGN_BOTTOM);

  dword idsel= create_rect(idicon, 0,0, 28,20, COL_ITEMLO|TRANSL_1, ALIGN_TOP|ALIGN_HCENTER);
  wnd_show(idsel, false);

  wnd_prop_add(idicon, "parent", 0, PROP_DWORD);
  wnd_set_mouse_callback(idminiwnd, icon_cbmouse, 0);
  wnd_set_mouse_callback(idtext, icon_cbmouse, 0);
}




// ----------------------------- Buttons -----------------------------

struct btn_data
{
  btn_cbclick cb_click;
  byte mouse_over, btn_down;
  dword col_bg, col_bghi, col_text;
};

void btn_setlook(primitive *self, bool hilight, bool pressed);

int btn_cbstatus(prop_t arg, primitive *self, int type)
{
  switch(type)
  {
    case STAT_CREATE:
      self->privdata= (void*)calloc(1, sizeof(btn_data));
      return true;

    case STAT_DESTROY:
      if(self->privdata) free(self->privdata);
      return true;
  }
    return 0;
}

prop_t btn_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  btn_data *data= (btn_data *)self->privdata;
  switch(type)
  {
    case PROP_ADD:
	btn_setlook(self, false, false);
    case PROP_SET:
      if(!strcmp(name, "text"))
      {
	  prim_text *txt= (prim_text *)self->children->next->self;
	  if(txt->type==PT_TEXT)
	  {
	    txt->rgt= font_gettextwidth(txt->font, (char*)value);
	    text_settext(txt->id, (char *)value);
	    wnd_setwidth( wnd_walk(self->id, CHILD, NEXT, NEXT, NEXT, SELF), font_gettextwidth((font*)wnd_getprop(self->id, "font"), (char*)value) );

#if 0
	    rect rc;
	    prim_get_abspos(self, &rc, true);
	    redraw_rect(&rc);
	    update_rect(&rc);
#endif
      	  }
      }
      else if(!strcmp(name, "on_click"))
      {
        ((btn_data*)self->privdata)->cb_click= (btn_cbclick)value;
      }
      else if(!strcmp(name, "font"))
      {
        font *fnt= (font*)value;
        if(fnt)
        {
	  prim_text *txt= (prim_text *)self->children->next->self;
	  if(txt->type==PT_TEXT)
	  {
	      int tw= font_gettextwidth(fnt, txt->text);
	      txt->font= fnt;
	      txt->rgt= txt->x+tw;
	      txt->btm= txt->y+font_height(fnt);
	      wnd_setwidth(wnd_walk(self->id, CHILD, NEXT, NEXT, NEXT, SELF), tw);
	  }
        }
      }
      else if(!strncmp(name, "col_", 4))
      {
        if(!strcmp(name+4, "bg"))
          data->col_bg= value,
	  rect_setcolor(wnd_walk(self->id, CHILD, CHILD, SELF), value);
        else if(!strcmp(name+4, "bghi"))
          data->col_bghi= value;
        else if(!strcmp(name+4, "text"))
          data->col_text= value;

        btn_setlook(self, false, false);
      }
      else if(!strcmp(name, "style"))
      {
	  if(value==0)
		wnd_show(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, SELF), true),
	    	wnd_show(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, NEXT, SELF), true);
	  else if(value==1)
		wnd_show(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, SELF), false),
	    	wnd_show(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, NEXT, SELF), false),
	  	rect_setcolor(wnd_walk(self->id, CHILD, SELF), INVISIBLE);
	  	//~ wnd_show(wnd_walk(self->id, CHILD, SELF), false);
      }
      else if(!strcmp(name, "checked"))
      {
	  if(value)
	  {
	      rect_setcolor(wnd_walk(self->id, CHILD, SELF), COL_FRAMELO|TRANSL_2);
	  }
	  else
	  {
	      rect_setcolor(wnd_walk(self->id, CHILD, SELF), INVISIBLE);
	  }
      }
      return value;

    case PROP_GET:
    default:
      return value;
  }
}

void btn_setlook(primitive *self, bool hilight, bool pressed)
{
    btn_data *data= (btn_data *)self->privdata;
    int style= wnd_getprop(self->id, "style");

    if(pressed)
    {
	if(style==0)
	{
	    frame_setcolors(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, SELF),
			    COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
			    COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2);


	    dword idtext= wnd_walk(self->id, CHILD, NEXT, SELF);
	    if(wnd_getrelx(idtext)==0)
	    	wnd_setpos(idtext, wnd_getrelx(idtext)+1, wnd_getrely(idtext)+1);
	}
	else if(style==1 && !wnd_getprop(self->id, "checked"))
	    rect_setcolor(wnd_walk(self->id, CHILD, SELF), COL_FRAMELO|TRANSL_2);
    }
    else
    {
	if(style==0)
	{
	    frame_setcolors(wnd_walk(self->id, CHILD, NEXT, NEXT, CHILD, SELF),
			    COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
			    COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1);

	    dword idtext= wnd_walk(self->id, CHILD, NEXT, SELF);
	    if(wnd_getrelx(idtext)!=0)
	    	wnd_setpos(idtext, wnd_getrelx(idtext)-1, wnd_getrely(idtext)-1);
	}
	else if(style==1 && !wnd_getprop(self->id, "checked"))
	    rect_setcolor(wnd_walk(self->id, CHILD, SELF), INVISIBLE);
    }

    if(hilight)
    {
    	if(style==0)
	    rect_setcolor(wnd_walk(self->id, CHILD, SELF), data->col_bghi);
	else if(style==1)
	    rect_setcolor(wnd_walk(self->id, CHILD, NEXT, NEXT, NEXT, SELF), COL_ITEMTEXT);
    }
    else
    {
    	if(style==0)
	    rect_setcolor(wnd_walk(self->id, CHILD, SELF), data->col_bg);
	else if(style==1)
	    rect_setcolor(wnd_walk(self->id, CHILD, NEXT, NEXT, NEXT, SELF), INVISIBLE);
    }
}

int btn_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
  //~ prim_rect *base= (prim_rect*)self->children->self;    // Hintergrund
  btn_data *data= (btn_data*)self->privdata;

  if(event==MOUSE_IN)  //  || event==MOUSE_OVER
  {
    data->mouse_over= true;
    if(btn) btn_setlook(self, false, true);
    else btn_setlook(self, true, false);
	void (*cb)(int id, int event, int btn)= (void(*)(int,int,int))wnd_getprop(self->id, "onmouseover");
    	if(cb) cb(self->id, event, btn);
  }
  else if(event==MOUSE_OUT)
  {
    data->mouse_over= false,
    btn_setlook(self, false, false);
  }

  else if(event==MOUSE_DOWN)
  {
  	if(btn!=1) return 0;

    if(!data->cb_click)
      data->cb_click= (btn_cbclick)wnd_getprop(self->id, "on_click");

    if(data->cb_click)
      data->cb_click(self->id, true, btn);

    data->btn_down= btn;
    btn_setlook(self, wnd_getprop(self->id, "style")==0? false: true, true);
  }

  else if(event==MOUSE_UP)
  {
    rect rc;
    prim_get_abspos(self, &rc, true);

    if(!data->cb_click)
      data->cb_click= (btn_cbclick)wnd_getprop(self->id, "on_click");

    if( ptinrect(x+rc.x, y+rc.y, rc) &&
        data->cb_click &&
        data->btn_down==1 &&
        !(data->btn_down&btn) )
      data->cb_click(self->id, false, data->btn_down);

    data->btn_down= 0;

    btn_setlook(self, true, false);
  }

  return 0;
}


void create_button_groups()
{
  dword hbtngrp= create_group("button");
  dword hrect= create_rect(hbtngrp, 0,0, MAXSCALE,MAXSCALE, 0, WREL|HREL);
  create_text(hbtngrp, 0,0, font_gettextwidth(FONT_ITEMS, "Button"),13, "Button",
              COL_ITEMTEXT, FONT_ITEMS, ALIGN_HCENTER|ALIGN_VCENTER);
  clone_group("raisedframe1", hbtngrp);
  create_rect(hbtngrp, 0,font_gettextheight(FONT_ITEMS, "Button")/2, font_gettextwidth(FONT_ITEMS, "Button"),1,
              INVISIBLE, ALIGN_HCENTER|ALIGN_VCENTER);

  wnd_set_mouse_callback(hbtngrp, btn_cbmouse, 0);
  wnd_set_props_callback(hbtngrp, btn_cbprops, 0);
  wnd_set_status_callback(hbtngrp, btn_cbstatus, 0);
  wnd_prop_add(hbtngrp, "text", (prop_t)"Button", PROP_STRING);
  wnd_prop_add(hbtngrp, "on_click", 0, PROP_DWORD);
  wnd_prop_add(hbtngrp, "font", (prop_t)FONT_DEFAULT, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_bg", COL_ITEM, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_bghi", COL_ITEMHI, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_text", COL_ITEMTEXT, PROP_DWORD);
  wnd_prop_add(hbtngrp, "style", 0, PROP_DWORD);
  wnd_prop_add(hbtngrp, "checked", 0, PROP_DWORD);
  wnd_prop_add(hbtngrp, "onmouseover", 0, PROP_DWORD);


  hbtngrp= create_group("titlebtn");
  hrect= create_rect(hbtngrp, 0,0, MAXSCALE,MAXSCALE, COL_ITEM, WREL|HREL);
  create_text(hbtngrp, 2,0, font_gettextwidth(FONT_DEFAULT, "Button"),13, "Button",
              COL_ITEMTEXT, FONT_DEFAULT, ALIGN_HCENTER|ALIGN_VCENTER);
  clone_group("raisedframe0", hbtngrp);

  wnd_set_mouse_callback(hbtngrp, btn_cbmouse, 0);
  wnd_set_props_callback(hbtngrp, btn_cbprops, 0);
  wnd_set_status_callback(hbtngrp, btn_cbstatus, 0);
  wnd_prop_add(hbtngrp, "text", (prop_t)"Button", PROP_STRING);
  wnd_prop_add(hbtngrp, "on_click", 0, PROP_DWORD);
  wnd_prop_add(hbtngrp, "font", (prop_t)FONT_DEFAULT, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_bg", COL_ITEM, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_bghi", COL_ITEMHI, PROP_DWORD);
  wnd_prop_add(hbtngrp, "col_text", COL_ITEMTEXT, PROP_DWORD);
  wnd_prop_add(hbtngrp, "style", 0, PROP_DWORD);
  wnd_prop_add(hbtngrp, "onmouseover", 0, PROP_DWORD);
}



// ----------------------------- frames -----------------------------

void create_frame_groups()
{
  dword idgrp1= create_group("raisedframe0");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);


  idgrp1= create_group("loweredframe0");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);


  idgrp1= create_group("raisedframe1");
  create_frame(idgrp1, 1,1, 1,1, 1,
               COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);

  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMELO|TRANSL_2, COL_FRAMELO|TRANSL_2,
               COL_FRAMELO|TRANSL_3, COL_FRAMELO|TRANSL_3,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);



  idgrp1= create_group("raisedframe2");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 2,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);

  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);



  idgrp1= create_group("loweredframe1");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMELO|TRANSL_1,COL_FRAMELO|TRANSL_1,
               COL_FRAMEHI|TRANSL_1,COL_FRAMEHI|TRANSL_1,
               WREL|HREL);


  idgrp1= create_group("beveledframe0");
  create_frame(idgrp1, 0,0, MAXSCALE,MAXSCALE, 1,
               COL_FRAMEHI|TRANSL_2, COL_FRAMEHI|TRANSL_2,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_TOP|WREL|HREL);
  create_frame(idgrp1, 1,1, 1,1, 1,
               COL_FRAMELO|TRANSL_1,COL_FRAMELO|TRANSL_1,
               COL_FRAMEHI|TRANSL_1,COL_FRAMEHI|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
}




// ----------------------------- Tooltips -----------------------------

void tooltip_timer(prop_t idtooltip, int idtimer, dword time)
{
  rect rc;

  wnd_get_abspos(wnd_getprop(idtooltip, "parent"), &rc);

  if( !ptinrect(mouse_x(), mouse_y(), rc) )
  {
    timer_kill(idtimer);
    wnd_close(idtooltip);
  }
}

prop_t tooltip_cbprops(prop_t arg, primitive *self, int type, const char *name, int id, prop_t value)
{
  switch(type)
  {
    case PROP_SET:
      if(!strcmp(name, "text"))
      {
        int width= font_gettextwidth(FONT_ITEMS, (char *)value);
        int height= font_gettextheight(FONT_ITEMS, (char *)value);
        int x= wnd_getx(wnd_getprop(self->id, "parent"));
        int y= wnd_gety(wnd_getprop(self->id, "parent"))-height-4;

        if(x+width+8>viewport.rgt)
          x= viewport.rgt-width-8;

        wnd_setpos(self->id, x, y);
        wnd_setsize(self->id, width+8, height+4);

        dword idtext= wnd_walk(self->id, CHILD, NEXT, SELF);
        wnd_setsize(idtext, width, height);
        text_settext(idtext, (char *)value);

        idtext= wnd_walk(idtext, NEXT, SELF);
        wnd_setsize(idtext, width, height);
        text_settext(idtext, (char *)value);

        wnd_set_zpos(self->id, Z_TOP);
      }
      else if(!strcmp(name, "parent"))
      {
        timer_create(tooltip_timer, self->id, 1000);
        wnd_set_zpos(self->id, Z_TOP);
      }
      break;
  }

  return value;
}

void create_tooltip_group()
{
  dword group= create_group("tooltip");
  wnd_set_zpos(group, Z_TOP);

  dword idrect= create_rect(group, 0,0, MAXSCALE,MAXSCALE, COL_TITLE|TRANSL_3, WREL|HREL);
  create_text(group, 1,1, 0,0, "", 0x000000, FONT_ITEMS, ALIGN_HCENTER|ALIGN_VCENTER);
  create_text(group, 0,0, 0,0, "", 0xFFFFFF, FONT_ITEMS, ALIGN_HCENTER|ALIGN_VCENTER);

/*
  create_frame(idrect, 0,0, 0,0, 4,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
  create_frame(idrect, 0,0, 0,0, 3,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
  create_frame(idrect, 0,0, 0,0, 2,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
  create_frame(idrect, 0,0, 0,0, 1,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
*/
  create_frame(idrect, 0,0, 0,0, 1,
               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);
//  create_frame(idrect, 1,1, 1,1, 1,
//               COL_FRAMELO|TRANSL_1, COL_FRAMELO|TRANSL_1,
//               COL_FRAMEHI|TRANSL_1, COL_FRAMEHI|TRANSL_1,
//               ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);

  wnd_prop_add(group, "text", (prop_t)"", PROP_DWORD);
  wnd_prop_add(group, "parent", 0, PROP_DWORD);
  wnd_set_props_callback(group, tooltip_cbprops, 0);
}







// ----------------------------- Misc. Code -----------------------------


// Kleine Buttons links oben
void btn_quit(dword btnid, int mousebtn)
{
  //~ if(mousebtn)
    //~ quit= true;
}

void btn_killall(dword btnid, int mousebtn)
{
  if(mousebtn)
  {
    wlist_free(windows->next);
    windows->next= 0;
    redraw_rect(&viewport);
    update_rect(&viewport);
  }
}



// Buttons rechts oben
void btn_videodlg(dword btnid, int mousebtn)
{
  if(mousebtn)
    video_dlg(NOPARENT);
}

void btn_filesel(dword btnid, int mousebtn)
{
#if 0
  if(mousebtn)
    filesel();
#endif
}




// clock + date code - a leftover from the standalone test app
char *ascii_time(bool verbose= false)
{
    static char ret[512];
    time_t thetime= time(0);
    tm *loctime= localtime(&thetime);

    bool pm= loctime->tm_hour>=12;
    int hour= loctime->tm_hour%12;
    if(!hour && pm) hour= 12;

    if(verbose)
    {
    	static const char *wdays[]= { "sunday", "monday", "tuesday", "wednesday",
                            	"thursday", "friday", "saturday" };

    	static const char *months[]= { "january", "february", "march", "april",
				"june", "jule", "august", "september",
			       	"october", "november", "december"};

    	sprintf(ret,
            "%s, %s %02d AD %d\n"
            "%02d:%02d:%02d %s, %s",
            wdays[loctime->tm_wday],
            months[loctime->tm_mon],
            loctime->tm_mday, loctime->tm_year+1900,
            hour, loctime->tm_min, loctime->tm_sec,
            (pm? "PM": "AM"),
            (loctime->tm_isdst>0? "Daylight Saving Time": "Daylight Wasting Time") );
    }
    else
    {
    	sprintf(ret, "%02d:%02d %s", hour, loctime->tm_min, (pm? "PM": "AM"));
    }

    return ret;
}

void clock_update(dword idclock)
{
  char *asc_time= ascii_time();
  int width= font_gettextwidth(FONT_ITEMS, asc_time);
  int idtext2= wnd_walk(idclock, CHILD, NEXT, SELF);
  text_settext(idtext2, asc_time);
  wnd_setwidth(idtext2, width);
}

void clock_timer(prop_t arg, int idtimer, dword time)
{
  clock_update(arg);
}

void clock_init_timer(prop_t arg, int idtimer, dword time)
{
  clock_update(arg);
  timer_kill(idtimer);
  timer_create(clock_timer, arg, 60*1000);
}



bool clock_update_tooltip(dword idtooltip)
{
  if(!wnd_setprop(idtooltip, "text", (prop_t)ascii_time(true)))
    return false;
  return true;
}

void clock_tooltip_timer(prop_t idtooltip, int idtimer, dword time)
{
  if( !clock_update_tooltip(idtooltip) )
    timer_kill(idtimer);
}


int clock_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
    /*
  if(event==MOUSE_IN)
  {
    rect rc;
    wnd_get_abspos(self->id, &rc);
    dword idtooltip= clone_group("tooltip", NOPARENT, rc.x,rc.y, 0,0, ALIGN_TOP|ALIGN_LEFT);
    wnd_setprop(idtooltip, "parent", self->id);
    clock_update_tooltip(idtooltip);
    timer_create(clock_tooltip_timer, idtooltip, 1000);
  }
    */
    return 0;
}


#define CLOCKWIDTH      font_width(FONT_ITEMS)*8 * 11/16
#define CLOCKHEIGHT     font_height(FONT_ITEMS)
void create_clock()
{
  dword idrect= create_rect(NOPARENT, 0,0, 0,0, 0x808080|TRANSL_2, ALIGN_RIGHT|ALIGN_BOTTOM);
  wnd_setresizable(idrect, false);
  wnd_set_mouse_callback(idrect, clock_cbmouse, 0);

  clone_group("beveledframe0", idrect, 0,0, MAXSCALE, MAXSCALE, WREL|HREL);

  create_text(idrect, 0,0, CLOCKWIDTH,CLOCKHEIGHT, "",
                             0xFFFFFF, FONT_ITEMS, ALIGN_HCENTER|ALIGN_VCENTER);

  clock_update(idrect);

  time_t thetime= time(0);
  tm *loctime= localtime(&thetime);
  timer_create(clock_init_timer, idrect, (60-(loctime->tm_sec%60))*1000 + 100);

  wnd_setisize(idrect, CLOCKWIDTH+8, CLOCKHEIGHT+4);
}



// ----------------------------- Checkbox -----------------------------
typedef void (*checkbox_ontoggle)(dword id, bool checked);

void default_cbtoggle(dword id, bool checked)
{
    logmsg("checkbox %s", checked? "CHECKED": "UNCHECKED");
}

prop_t checkbox_cbprops(prop_t arg, struct primitive *self,
                     int type, const char *name, int id, prop_t value)
{
    switch(type)
    {
    	case PROP_ADD:
    	case PROP_SET:
	    if(!strcmp(name, "checked"))
	    {
		dword boxtxt= wnd_walk(self->id, CHILD, CHILD, SELF);
		if(value)
		    text_settext(boxtxt, "x"); //"\x87");
		else
		    text_settext(boxtxt, "");
	    }
	    else if(!strcmp(name, "text"))
	    {
		dword txt= wnd_walk(self->id, CHILD, NEXT, SELF);
		text_settext(txt, (char*)value);
	    }
	    return value;

	default:
	    return value;
    }
}

int checkbox_cbmouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
    dword box= wnd_walk(self->id, CHILD, SELF);
    switch(event)
    {
	case MOUSE_IN:
	    if(!self->privdata || !btn) break;
	    // else fall through
	case MOUSE_DOWN:
	{
	    rect_setcolor(box, COL_ITEMHI);
	    self->privdata= (void*)1;	// flag mousedown
	    break;
	}

	case MOUSE_OUT:
	case MOUSE_UP:
	{
	    rect_setcolor(box, COL_FRAMELO|TRANSL_1);
	    if(self->privdata && event==MOUSE_UP)
	    {
	    	self->privdata= 0;
		bool status= !(bool)wnd_getprop(self->id, "checked");
		wnd_setprop(self->id, "checked", status);
		checkbox_ontoggle cb= (checkbox_ontoggle)wnd_getprop(self->id, "on_toggle");
		if(cb) cb(self->id, status);
	    }
	    break;
	}
    }
    return 0;
}

void create_checkbox_group()
{
    dword grp= create_group("checkbox");
    dword btn= create_rect(grp, 0,0, 13,13, COL_FRAMELO|TRANSL_1, ALIGN_LEFT|ALIGN_VCENTER);
    create_text(btn, 1,-1,8,13, "\x87", COL_FRAMEHI, FONT_SYMBOL, ALIGN_HCENTER|ALIGN_VCENTER);	// btn
    create_text(grp, 16,0, MAXSCALE,font_height(FONT_DEFAULT), "", COL_ITEMTEXT, FONT_DEFAULT, ALIGN_VCENTER|WREL); // text
    clone_frame("loweredframe0", btn);

    wnd_set_mouse_callback(grp, checkbox_cbmouse, 0);

    wnd_prop_add(grp, "on_toggle", (prop_t)default_cbtoggle, PROP_DWORD);
    wnd_prop_add(grp, "checked", 0, PROP_DWORD);
    wnd_prop_add(grp, "text",(prop_t)"CheckBox", PROP_STRING);
    wnd_set_props_callback(grp, checkbox_cbprops, 0);
}



void ctxmenuitem_onclick(dword id, int btn)
{
    dword idlist= wnd_walk(id, PARENT, SELF);
    wnd_show(idlist, false);
    wnd_setprop((dword)wnd_getprop(idlist, "parent"), "checked", 0);
    logmsg( "Klick: %s", (char*)wnd_getprop(id, "data") );
}

void menubtn_onclick(dword id, int btn)
{
    dword idlist= wnd_getprop(id, "list_id");
    if(btn && wnd_isvisible(idlist)) wnd_show(idlist, false), wnd_setprop(id, "checked", 0);
    else if(btn)
    {
    	rect p;
    	wnd_get_abspos(id, &p);
	wnd_setpos(idlist, p.x, p.btm);
	wnd_show(idlist, true);
	wnd_setprop(id, "checked", 1);
    }
}

void testdlg()
{
    dword wnd= create_rect(NOPARENT, 100,100, 300,300, COL_WINDOW, ALIGN_LEFT|ALIGN_TOP);
    wnd_totop(wnd);
    //~ wnd_setresizable(wnd, false);
    dword frame= clone_frame("titleframe", wnd);
    wnd_setprop(frame, "title", (prop_t)"test dialog");

    clone_group("button", wnd, 0,0, 64,18, ALIGN_LEFT|ALIGN_TOP);

    dword btn2= clone_group("button", wnd, 64+4,0, 64,18, ALIGN_LEFT|ALIGN_TOP);
    wnd_setprop(btn2, "style", 1);
    wnd_setprop(btn2, "text", (prop_t)"BlahBlah");
    wnd_setprop(btn2, "on_click", (prop_t)menubtn_onclick);

    rect p;
    wnd_get_abspos(btn2, &p);
    dword id= clone_group("dropdown_list", NOPARENT, p.x,p.btm, 64,100, ALIGN_LEFT|ALIGN_TOP);
    wnd_show(id, false);
    wnd_set_zpos(id, Z_TOP);
    wnd_setprop(id, "parent", (prop_t)btn2);
    wnd_setprop(id, "on_click", (prop_t)ctxmenuitem_onclick);
    wnd_setprop(id, "append_item", (prop_t)"Text1");
    wnd_setprop(id, "item_data", (prop_t)"Text1");
    wnd_setprop(id, "append_item", (prop_t)"Text2");
    wnd_setprop(id, "item_data", (prop_t)"Text2");
    wnd_setprop(id, "append_item", (prop_t)"\\SEPARATOR");
    wnd_setprop(id, "append_item", (prop_t)"Blah!");
    wnd_setprop(id, "item_data", (prop_t)"Text3!");


    wnd_prop_add(btn2, "list_id", (prop_t)id, PROP_DWORD);


//    dword colwheel= clone_group("edm_colorwheel", wnd, 0,24, 200,200, ALIGN_LEFT|ALIGN_TOP);


//    dword pixmap1= clone_group("edm_pixmap", wnd, 0,24, 200,200, ALIGN_LEFT|ALIGN_TOP);
//    wnd_setprop(pixmap1, "filename", (prop_t)"data/edm/colorwheel.png");
//
//    dword pixmap2= clone_group("edm_pixmap", wnd, 5000,24, 5000,100, ALIGN_LEFT|ALIGN_TOP|ALIGN_BOTTOM|WREL|XREL);
//    wnd_setprop(pixmap2, "filename", (prop_t)"data/edm/explosion.png");


    dword ellipse= create_ellipse(wnd, 0,0, 5000,100, COL_ITEMHI, ALIGN_LEFT|ALIGN_BOTTOM|WREL, true,
				  0.5,0.5, 0.5,0.5);

    dblpos verts1[]= { 0.0,0.0, 1.0,0.25, 0.5,1.0 };
    dword poly1= create_poly(wnd, 5000,0, 5000,100, COL_ITEMHI, ALIGN_LEFT|ALIGN_BOTTOM|WREL|XREL, true,
			     3, verts1);

    dblpos verts2[]= { 0.25,0.0, 0.75,0.5, 0.25,0.75, 0.0,0.5 };
    dword poly2= create_poly(wnd, 5000,0, 5000,100, COL_TITLE, ALIGN_LEFT|ALIGN_BOTTOM|WREL|XREL, true,
			     4, verts2);
}





// ----------------------------- textinput -----------------------------

static int textinput_mouse(prop_t arg, primitive *self, int event, int x, int y, int btn)
{
    if(event==MOUSE_DOWN)
    {
	if(self->children) wnd_setkbdfocus(self->children->self->id);
    }
    return 0;
}

static int textinput_curseq;
static void textinput_cursortimer(prop_t arg, int idtimer, dword time)
{
    if((++textinput_curseq)&1) wnd_show(arg, true);
    else wnd_show(arg, false);
}

static int textinput_settimer(primitive *self)
{
    if(self->privdata) timer_kill(*(int*)(&self->privdata));
    textinput_curseq= 0;
    dword cur= wnd_walk(self->id, CHILD, SELF);
    dword tid= timer_create(textinput_cursortimer, cur, 300);
    self->privdata= (void*)tid;
    textinput_cursortimer(cur, tid, 0);
    return 0;
}

static int textinput_status(prop_t arg, struct primitive *self, int type)
{
    switch(type)
    {
	case STAT_GAINFOCUS:
	{
	    textinput_settimer(self);
	    break;
	}
	case STAT_LOSEFOCUS:
	{
	    if(self->privdata) timer_kill(*(int*)(&self->privdata));
	    dword cursor= wnd_walk(self->id, CHILD, SELF);
	    wnd_show(cursor, false);
	    break;
	}
	case STAT_CREATE:
	{
	    wnd_setalignment( self->id, HALIGN(wnd_getalignment(wnd_walk(self->id, PARENT, SELF)))|ALIGN_VCENTER );
	    break;
	}
    }

    cb_status cb;
    if( self->parent && (cb= (cb_status)wnd_getprop(self->parent->id, "status_cb")) )
	cb( arg, self->parent, type );
    return 0;
}

static prop_t textinput_props(prop_t arg, struct primitive *self,
			      int type, const char *name, int id, prop_t value)
{
    if(type==PROP_SET)
    {
	if(id==0)	// cursorpos
	{
	    char ch[TEXTINPUT_MAX];
	    if(int(value)<0) value= 0;
	    else if(value>TEXTINPUT_MAX-1) value= TEXTINPUT_MAX-1;
	    dword text= wnd_walk(self->id, CHILD, SELF);
	    dword cursor= wnd_walk(text, CHILD, SELF);
	    text_gettext(text, ch, TEXTINPUT_MAX);
	    int textlen= strlen(ch);
	    if((int)value>textlen) value= textlen;
	    ch[value]= 0;
	    int len= font_gettextwidth(FONT_DEFAULT, ch);
	    wnd_setx(cursor, len);
	}
	else if(!strcmp(name, "text"))
	{
	    dword text= wnd_walk(self->id, CHILD, SELF);
	    text_settext(text, (char*)value);
	    wnd_setwidth(text, font_gettextwidth(FONT_DEFAULT, (char*)value) + font_gettextwidth(FONT_DEFAULT, "_"));
	    wnd_setprop(self->id, "cursorpos", strlen((char*)value));
	}
    }
    return value;
}

static void textinput_keybd(prop_t arg, primitive *self, int isdown, int code, int chr)
{
    static char str[TEXTINPUT_MAX], s2[TEXTINPUT_MAX];
    primitive *parent= self->parent;
    char *newstr= 0;

//	printf("code: %d, chr: %d\n", code, chr);

    if(isdown)
    {
	textinput_settimer(self);

	text_gettext(self->id, str, TEXTINPUT_MAX);
	int pos= wnd_getprop(parent->id, "cursorpos");

	textinput_filter_fn ffn= (textinput_filter_fn)wnd_getprop(parent->id, "filter_fn");
	if(ffn)
	{
	    int oldpos= pos;
	    if( ffn(parent->id, str, pos, code, chr) )
	    {
		text_settext(self->id, str);
	    	wnd_setwidth(self->id, font_gettextwidth(FONT_DEFAULT, str));
		if(pos!=oldpos) wnd_setprop(parent->id, "cursorpos", pos);
		return;
	    }
	}

	if(isprint(chr) || chr==' ')
	{
	    int len= strlen(str);
	    if(len>TEXTINPUT_MAX-2) len= TEXTINPUT_MAX-2;
	    if(pos>len) pos= len;
	    else if(pos<0) pos= 0;
	    memcpy(s2, str, pos);
	    s2[pos]= chr;
	    strncpy(s2+pos+1, str+pos, TEXTINPUT_MAX-2-pos);
	    s2[TEXTINPUT_MAX-1]= 0;
	    text_settext(self->id, s2);
	    wnd_setprop(parent->id, "cursorpos", ++pos);
	    newstr= s2;
	}
	else if(chr=='\b')	// backspace
	{
	    int len= strlen(str);
	    if(len && pos)
	    {
		for(int i= pos; i<=len; i++) str[i-1]= str[i];
		text_settext(self->id, str);
		wnd_setprop(parent->id, "cursorpos", --pos);
		newstr= str;
	    }
	}
	else if(chr==127)	// del
	{
	    int len= strlen(str);
	    if(len)
	    {
		for(int i= pos; i<len; i++) str[i]= str[i+1];
		text_settext(self->id, str);
		newstr= str;
	    }
	}
	else if(chr==13)	// enter
	{
	    wnd_setkbdfocus(NOWND);
//	    dword cur= wnd_walk(self->id, CHILD, SELF);
//	    wnd_show(cur, false);
//	    wnd_setkbdfocus(self->id);
	}

	else if(code==113) //276)	// left
	{
	    wnd_setprop(parent->id, "cursorpos", --pos);
	}
	else if(code==114) //275)	// right
	{
	    wnd_setprop(parent->id, "cursorpos", ++pos);
	}
	else if(code==110) //278)	// home
	{
	    wnd_setprop(parent->id, "cursorpos", 0);
	}
	else if(code==115) //279)	// end
	{
	    wnd_setprop(parent->id, "cursorpos", TEXTINPUT_MAX);
	}

	if(newstr)
	{
//	    wnd_setwidth(self->id, font_gettextwidth(FONT_DEFAULT, newstr) + font_gettextwidth(FONT_DEFAULT, "_"));
	    textinput_on_change_fn cb;
	    if( (cb= (textinput_on_change_fn)wnd_getprop(parent->id, "on_change")) )
	    	cb( parent->id, newstr );
	}
    }
}

void create_textinput_group()
{
    dword grp= create_group("textinput");
    dword txt= create_text(grp, 2,0, 0,font_height(FONT_DEFAULT)+1, "", COL_ITEMTEXT, FONT_DEFAULT, ALIGN_RIGHT|ALIGN_VCENTER);
    dword cursor= create_text(txt, 0,0, 10,font_height(FONT_DEFAULT)+1, "_", COL_ITEMTEXT|TRANSL_3, FONT_DEFAULT, ALIGN_LEFT|ALIGN_VCENTER);
    wnd_show(cursor, false);
    wnd_set_zpos(cursor, Z_TOP);
    wnd_set_mouse_callback(grp, textinput_mouse, 0);
    wnd_set_props_callback(grp, textinput_props, 0);
    wnd_set_status_callback(txt, textinput_status, 0);
    wnd_set_kbd_callback(txt, textinput_keybd, 0);
    wnd_addprop(grp, "cursorpos", 0, PROP_DWORD);
    wnd_addprop(grp, "status_cb", 0, PROP_DWORD, "cb_status function pointer chain");
    wnd_addprop(grp, "on_change", 0, PROP_DWORD, "void (*on_change) (dword id, char *text); called after change.");
    wnd_addprop(grp, "filter_fn", 0, PROP_DWORD, "bool (*filter_fn) (dword id, char text[TEXTINPUT_MAX], int &cursorpos, int code, int chr); called before change; may modify text buffer and cursorpos. must return true if anything was modified.");
    wnd_addprop(grp, "text", (prop_t)"", PROP_STRING, "input text");
}



void init_defgroups()
{
    create_frame_groups();
    create_button_groups();
    create_scrollbox_group();
    create_listbox_group();
    create_desktopicon();
    create_titleframe();
    create_dropdown_group();
    create_tooltip_group();
    create_checkbox_group();
    create_textinput_group();
    create_multiline();
}



