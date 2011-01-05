#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "defs.h"
#include "flux.h"
#include "prop_list.h"
#include "action_queue.h"
#include "sys.h"
#include "hash.h"

#ifdef FLUX_EDM
#	include "flux-edm.h"
#	define REDRAW_EVERY_FRAME
#elif defined FLUX_NDS
#	include <nds.h>
#	define REDRAW_EVERY_FRAME
#endif

#define REDRAW_EVERY_FRAME

#ifndef logmsg
#define logmsg printf
#endif

#include "line.c"


/* stuff to be dynamically loaded from 'plugin' .so / .dll, currently linked statically
DEFINE_PLUGIN(dep)
  PLUGIN_FUNCTION_NP(void, draw_text) (font *font, char *text, int sx,int sy, rect clp, dword col)
  PLUGIN_FUNCTION_NP(void, font_dep_update) (font *_font)
  PLUGIN_FUNCTION_NP(void, font_dep_free) (font *font)
  PLUGIN_FUNCTION_NP(void, put_alpha_map) (dword color, byte *bmap, int bw,int bh, rect srcpos, int xdest,int ydest)
  PLUGIN_FUNCTION_NP(void, update_rect) (const rect *r)
  PLUGIN_FUNCTION_NP(int, setvideomode) (int width, int height, int bpp, bool use_accel)
  PLUGIN_FUNCTION_NP(bool, bitmap_setpixels) (dword id, rgba *pixels, int swidth,int sheight, rect *rcs, int xd, int yd)
  PLUGIN_FUNCTION_NP(dword, map_color) (dword c)
  PLUGIN_FUNCTION_NP(void, fill_rect) (rect *r, dword color)
  PLUGIN_FUNCTION_NP(void, hline) (int x1, int y, int x2, dword color)
  PLUGIN_FUNCTION_NP(void, fill_scanlines) (shape_scanline *scanlines, rect clip, rect abspos, dword color)
  PLUGIN_FUNCTION_NP(void, plot_outline) (pos *outline, int n, dword color, rect clip, pos p)
  PLUGIN_FUNCTION_NP(void, blt_rgba) (rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
  PLUGIN_FUNCTION_NP(void, paint_cursor) (rect pos)
  PLUGIN_FUNCTION_NP(void, update_mouse) (struct mouse *m)
  PLUGIN_FUNCTION_NP(void, next_event) ()
  PLUGIN_FUNCTION_NP(void, set_mouse_pos) (int x, int y)
  PLUGIN_FUNCTION_NP(bool, dep_init) (dword *syscol_table, struct primitive **backdrop, mouse_event_fn mouse_event, keyboard_event_fn keyboard_event, clip_rect_fn clip_rect)
  PLUGIN_FUNCTION_NP(void, video_cleanup) ()
  PLUGIN_FUNCTION_NP(void, setpixel) (int x, int y, byte r, byte g, byte b)
  PLUGIN_FUNCTION_NP(void, getpixel) (int x, int y, int *r, int *g, int *b)
  PLUGIN_FUNCTION_NP(void, setpixeli) (int x, int y, dword color, rect clip)
  PLUGIN_VARIABLE_NP(struct viewport, viewport, viewport)
END_PLUGIN(dep)
*/

// global vars

dword bpp= 16;

bool quit= false;



dword syscol_table[NSYSCOLORS]=
{
    0x202020,
    0x505050,
    0xFFFFFF,
    0x606060,
    0x707070,
    0x505050,
    0xF0F0F0,
    0x808080,
    0xFFFFFF,
    0x000000
};
/*
{
  0x404040|TRANSL_0,    // COL_BAKGND=   SYSCOL(1),
  0x306050|TRANSL_2,    // COL_WINDOW=   SYSCOL(2),
  0xFFFFFF,             // COL_TEXT=     SYSCOL(3),
  0x406068|TRANSL_2,    // COL_ITEM=     SYSCOL(4),
  0x507078|TRANSL_2,    // COL_ITEMHI=   SYSCOL(5),
  0x305058|TRANSL_2,    // COL_ITEMLO=   SYSCOL(6),
  0xD0E0D0,             // COL_ITEMTEXT= SYSCOL(7),
  0x508070,    			// COL_TITLE=    SYSCOL(8),
  0xFFFFFF,             // COL_FRAMEHI=  SYSCOL(9),
  0x000000              // COL_FRAMELO=  SYSCOL(10),
};
*/


font *def_fonts[MAX_DEFAULTFONTS];


struct mouse mouse;

struct primitive *backdrop;

struct window_list *windows= 0;
struct window_list *groups= 0;


int_hash<window_list> window_hash;
int_hash<window_list> group_hash;


dword id_mousehook= 0;


int mouse_x(void)
{
    return (int)(mouse.x+0.5);
}

int mouse_y(void)
{
    return (int)(mouse.y+0.5);
}

int mouse_btns(void)
{
    return mouse.btns;
}


void flashrc(rect *rc, int delay= 25)
{
    fill_rect(rc, 0xFF0000);
    update_rect(rc);
    sys_msleep(delay);
    redraw_rect(rc);
    update_rect(rc);
}


dword unique_id()
{
    static dword id;
    return ++id;
}

void unhash_id(dword id)
{
    if( window_hash.del(id)<0 || group_hash.del(id)<0 )
        logmsg("prim_destroy: id %d not in any hash!\n", id);
}

window_list *wlist_end(window_list *start)
{
    window_list *ret= start;
    if(ret) while(ret->next) ret= ret->next;
    return ret;
}

window_list *wlist_top(window_list *start)
{
    window_list *ret= start;

    if(ret)
    {
        while(ret->next && ret->next->self->flags.z_pos!=Z_TOP)
            ret= ret->next;
    }

    return ret;
}

window_list *wlist_bottom(window_list *start)
{
    window_list *ret= start;

    if(ret)
    {
        while(ret->prev) ret= ret->prev;
        while(ret->next && ret->next->self->flags.z_pos==Z_BOTTOM) ret= ret->next;
    }

    return ret;
}


dword wstat_o;
dword wstat_n;

void dump_wstat()
{
    logmsg("wlist_find stats: O: %d, nsrch: %d, O/nsrch: %.2f\n",
           wstat_o, wstat_n, (double)wstat_o/wstat_n);
    wstat_o= wstat_n= 0;
}


window_list *wlist_find(window_list *start, dword id)
{
    if(id==NOWND || !start) return 0;

    if( start==windows )
        return window_hash.lookup(id);
    else if( start==groups )
        return group_hash.lookup(id);

    else if( window_hash.lookup(start->self->id) )
        return window_hash.lookup(id);
    else if( group_hash.lookup(start->self->id) )
        return group_hash.lookup(id);


    logmsg("wlist_find: warning: 'start' is neither window list nor group list...\n");

    window_list *wnd= start;
    window_list *pchild;

    if(wnd && wnd->self) do
        {
            if(id==wnd->self->id)
            {
                wstat_n++;
                return wnd;
            }

            if( (pchild= wnd->self->children) &&
                    (id >= pchild->self->id) &&    // Die IDs der Kinder sind immer groeszer als der ID des parent
                    (pchild= wlist_find(pchild, id)) )
                return pchild;

            wstat_o++;

        }
        while( (wnd= wnd->next) );

    return 0;
}

primitive *findprim(dword id)
{
    window_list *w= wlist_find(groups, id);
    if(!w) w= wlist_find(windows, id);
    return (w? w->self: 0);
}


void wlist_free(window_list *lst)
{
    window_list *src= lst, *next;

    while(src)
    {
        if(src->self->children)
        {
            wlist_free(src->self->children);
            src->self->children= 0;
        }

        prim_destroy(src->self);
        free(src->self);

        next= src->next;
        src->self= 0;
        src->next= src->prev= 0;
        free(src);

        src= next;
    }
}


bool wlist_clone(window_list *parent, window_list *lst, bool is_frame= false)
{
    window_list *src= lst;
    window_list *newwnd;
    primitive *srcp, *dstp;

    do
    {
        srcp= src->self;

        switch(src->self->type)
        {
        case PT_RECT:
            newwnd= prim_create_rect(parent, true, srcp->x,srcp->y,
                                     srcp->rgt-srcp->x,srcp->btm-srcp->y,
                                     ((prim_rect*)srcp)->color, srcp->alignment);
            break;

        case PT_TEXT:
            newwnd= prim_create_text(parent, true, srcp->x,srcp->y,
                                     srcp->rgt-srcp->x,srcp->btm-srcp->y,
                                     ((prim_text*)srcp)->text, ((prim_text*)srcp)->color,
                                     ((prim_text*)srcp)->font, srcp->alignment);
            break;

        case PT_FRAME:
        {
            prim_frame *self= (prim_frame *)srcp;
            newwnd= prim_create_frame(parent, true, self->x, self->y,
                                      self->rgt-self->x, self->btm-self->y,
                                      self->framewidth,
                                      self->col_left, self->col_top,
                                      self->col_right, self->col_btm,
                                      self->alignment);
            break;
        }

        case PT_POLY:
        {
            prim_shape_poly *self= (prim_shape_poly *)srcp;
            newwnd= prim_create_poly(parent, true, self->x, self->y,
                                     self->rgt-self->x, self->btm-self->y,
                                     self->color, self->alignment, self->filled,
                                     self->nverts, self->vertexes);
            break;
        }

        default:
            logmsg("wlist_clone: unknown type %d!\n", src->self->type);
            return false;
        }

        dstp= newwnd->self;

        dstp->callbacks= srcp->callbacks;

        dstp->rcnonframe= srcp->rcnonframe;

        if(srcp->props)
            dstp->props= props_clone(srcp->props),
                  dstp->highest_propid= srcp->highest_propid;

        //~ dstp->visible= srcp->visible;
        //~ if(is_frame || src->self->is_frame)
        //~ dstp->is_frame= 1;
        dstp->flags= srcp->flags;
        if(is_frame) dstp->flags.is_frame= 1;

        if(dstp->callbacks.status)
            dstp->callbacks.status(dstp->callbacks.status_arg, dstp, STAT_CREATE);

        if(srcp->children)
            wlist_clone(newwnd, srcp->children, is_frame);

        if(dstp->callbacks.props)
        {
            for(int i= 0; i<dstp->props->size; i++)
            {
                dstp->callbacks.props(dstp->callbacks.props_arg, dstp, PROP_ADD,
                                      dstp->props->by_id[i]->name,
                                      i, dstp->props->by_id[i]->ivalue);
            }
        }

        if(dstp->callbacks.status)
            dstp->callbacks.status(dstp->callbacks.status_arg, dstp, STAT_SHOW);

    }
    while( (src= src->next) );

    return true;
}



window_list *grplist_find(window_list *start, const char *name)
{
    window_list *wnd= start;
    prim_group *pg;

    if(!start || !name) return 0;

    if(wnd->self) do
        {
            pg= (prim_group *)wnd->self;

            if(pg->type==PT_GROUP && pg->name && !strcmp(name, pg->name))
                return wnd;

        }
        while( (wnd= wnd->next) );

    return 0;
}




// callbacks aufrufen und fenster-listen enumerieren allgemein

typedef void (*wlist_enum_func)(primitive *p, prop_t arg);

void call_status_cb(primitive *p, prop_t arg)
{
    if(p->callbacks.status) p->callbacks.status(p->callbacks.status_arg, p, arg);
}

void wlist_enum(window_list *lst, wlist_enum_func func, prop_t func_arg)
{
    window_list *w= lst->self->children; //wlist_end(lst->self->children);

    while(w)
    {
        primitive *self= w->self;
        window_list *c= self->children;

        while(c)
        {
            wlist_enum(c, func, func_arg);
            c= c->next;
        }

        func(self, func_arg);

        w= w->next;
    }

    func(lst->self, func_arg);
}




dword create_group(const char *name)
{
    if(get_group_id(name)!=NOWND) return NOWND;

    window_list *g, *tmp;
    dword id= unique_id();

    if(!groups->self)
    {
        groups->self= (primitive *)calloc(1, sizeof(prim_group));
        g= groups;
    }
    else
    {
        tmp= groups->next;
        g= groups->next= (window_list *)calloc(1, sizeof(window_list));
        g->prev= groups;
        g->next= tmp;
        if(tmp) tmp->prev= g;
        g->self= (primitive *)calloc(1, sizeof(prim_group));
    }

    g->self->type= PT_GROUP;
    ((prim_group *)g->self)->name= strdup(name);
    ((prim_group *)g->self)->color= INVISIBLE;
    g->self->id= id;
    g->self->rgt= g->self->btm= 100;
    g->self->flags.visible= 1;
    g->self->flags.resizable= 1;

    group_hash.add(id, g);

    return id;
}

dword get_group_id(const char *name)
{
    window_list *grp;
    if( !(grp= grplist_find(groups, name)) )
        return NOWND;
    return grp->self->id;
}

dword clone_group(const char *name, dword hparent, int x, int y,
                  int w, int h, int align)
{
    window_list *group, *grproot;
    primitive *srcp, *dstp;

    // Gruppe in der Gruppenliste suchen
    if( !name || !(group= grplist_find(groups, name)) )
    {
        logmsg("clone_group: groupname %s not found!\n", name);
        return NOWND;
    }

    prim_rect *base_prim= (prim_group*)group->self;

    if(hparent==NOPARENT)
    {
        // Basis-Rechteck
        grproot= prim_create_rect(wlist_top(windows), false,
                                  x,y, w,h, base_prim->color, align);
        grproot->self->flags= group->self->flags;
        //~ logmsg("%d %d %d %d", group->self->flags.transparent, group->self->flags.is_frame, group->self->flags.z_pos, group->self->flags.visible);
        // wenn color&INVISIBLE und flags.transparent==false, kann man
        // "durch das fenster auf den hintergrund durchkucken", lustiger effekt
        // vllt als "TRANSL_SEETHROUGH" einbauen?
        grproot->self->flags.transparent= true;
    }
    else
    {
        window_list *parent;

        // Parent in Fenster- und Gruppenliste suchen
        if( !(parent= wlist_find(windows, hparent)) &&
                !(parent= wlist_find(groups, hparent)) )
            return 0xFFFFFFFF;

        // Basis-Rechteck
        grproot= prim_create_rect(parent, true,
                                  x,y, w,h, base_prim->color, align);
    }

    srcp= group->self;
    dstp= grproot->self;

    // Callbacks kopieren
    dstp->callbacks= srcp->callbacks;

    // Non-Frame-Rechteck kopieren
    dstp->rcnonframe= srcp->rcnonframe;

    // Property-Liste klonen
    if(srcp->props)
        dstp->props= props_clone(srcp->props),
              dstp->highest_propid= srcp->highest_propid;

    // Status-Callback: STAT_CREATE
    if(dstp->callbacks.status)
        dstp->callbacks.status(dstp->callbacks.status_arg, dstp, STAT_CREATE);

    // Kindfenster der Gruppe klonen
    if(srcp->children)
        wlist_clone(grproot, srcp->children, srcp->flags.is_frame);

    // Property-Callback für alle Props aufrufen
    if(dstp->callbacks.props)
    {
        for(int i= 0; i<dstp->props->size; i++)
        {
            dstp->callbacks.props(dstp->callbacks.props_arg, dstp, PROP_ADD,
                                  dstp->props->by_id[i]->name,
                                  i, dstp->props->by_id[i]->ivalue);
        }
    }

    //~ if(wlist_find(windows, grproot->self->id)) wlist_enum(grproot, call_status_cb, STAT_SHOW);

#ifndef REDRAW_EVERY_FRAME
    // Fenster Neuzeichnen
    rect rc;
    prim_get_abspos(dstp, &rc, true);
    redraw_rect(&rc);
    update_rect(&rc);
#endif

//  wnd_forceredraw(dstp->id, 0);

    return dstp->id;
}

void recalc_frame(primitive *frame, primitive *parent)
{
    rect rcinner= { 0,0, 0,0 };
    if(frame) prim_get_abspos(frame, &rcinner, false);
    rect rcouter= rcinner;
    rect rcold= rcinner;

    if(frame) calc_nonframe_rect(frame, rcinner);

    parent->rcnonframe.x= rcinner.x-rcold.x;
    parent->rcnonframe.y= rcinner.y-rcold.y;
    parent->rcnonframe.rgt= rcinner.rgt-rcold.rgt;
    parent->rcnonframe.btm= rcinner.btm-rcold.btm;

    wnd_setpos(parent->id, rcouter.x,rcouter.y);
    wnd_setsize(parent->id, rcouter.rgt-rcouter.x,rcouter.btm-rcouter.y);
}

dword clone_frame(const char *name, dword hparent)
{
    window_list *group;   // die group in der grouplist
    window_list *parent;  // das parent in der window- oder grouplist
    window_list *grproot;

    if( !name || !(group= grplist_find(groups, name)) )
    {
        logmsg("clone_frame: groupname %s not found!\n", name);
        return 0xFFFFFFFF;
    }

    if( !(parent= wlist_find(windows, hparent)) &&
            !(parent= wlist_find(groups, hparent)) )
        return 0xFFFFFFFF;

    grproot= prim_create_rect(parent, true, 0,0, MAXSCALE,MAXSCALE,
                              INVISIBLE, ALIGN_LEFT|ALIGN_TOP|WREL|HREL);

    grproot->self->flags.is_frame= 1;
    grproot->self->callbacks= group->self->callbacks;

    if(group->self->props)
        grproot->self->props= props_clone(group->self->props),
                       grproot->self->highest_propid= group->self->highest_propid;

    // Status-Callback: STAT_CREATE
    if(grproot->self->callbacks.status)
        grproot->self->callbacks.status(grproot->self->callbacks.status_arg,
                                        grproot->self, STAT_CREATE);

    if(group->self->children)
    {
        wlist_clone(grproot, group->self->children, true);

        recalc_frame(grproot->self, parent->self);
    }

    // Property-Callback für alle Props aufrufen
    if(grproot->self->callbacks.props)
    {
        for(int i= 0; i<grproot->self->props->size; i++)
        {
            grproot->self->callbacks.props(grproot->self->callbacks.props_arg,
                                           grproot->self, PROP_ADD,
                                           grproot->self->props->by_id[i]->name,
                                           i, grproot->self->props->by_id[i]->ivalue);
        }
    }

#ifndef REDRAW_EVERY_FRAME
    rect rc;
    prim_get_abspos(grproot->self, &rc, true);
    redraw_rect(&rc);
    update_rect(&rc);
#endif

    return grproot->self->id;
}


static char sizeof_prop_t_is_wrong[ 1 - 2*(sizeof(prop_t)<sizeof(void*)) ];


/* int wnd_addprop(dword id, char *name, prop_t value, int type)
 *
 * Fügt eine Property mit namen NAME und Wert VALUE zur property-Liste
 * des Fensters ID hinzu. Gibt den ID der neuen Property zurück, oder 0,
 * wenn ein Fehler aufgetreten ist (zu wenig Speicher).
 *
 */
int wnd_addprop(dword id, const char *name, prop_t value, int type, const char *desc)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    primitive *p= wnd->self;

    if(!p->props)
    {
        if( !(p->props= (prop_list *)calloc(1, sizeof(prop_list))) ||
                !props_init(p->props) )
            return 0;
    }

    if(!props_add(p->props, name, p->highest_propid, value, type, desc))
        return 0;

    if(p->callbacks.props)
        value= p->callbacks.props(p->callbacks.props_arg, p, PROP_ADD, name,
                                  p->highest_propid, value);

    return ++p->highest_propid;
}


/* dword wnd_getprop(dword id, char *name)
 *
 * Gibt den Wert der property NAME aus der Liste des Fensters ID zurück,
 * oder 0, wenn die property nicht gefunden wurde.
 *
 */
prop_t wnd_getprop(dword id, const char *name)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd)
    {
        /*logmsg("prop_get: %d not found\n", id);*/ return 0;
    }

    primitive *prim= wnd->self;
    property *prop;

    if(!prim->props)
    {
        /*logmsg("%d: no props\n", id);*/ return 0;
    }

    if( !(prop= props_find_byname(prim->props, name)) )
    {
        /*logmsg("%d: prop '%s' not found\n", id, name);*/ return 0;
    }

    if(prim->callbacks.props)
        return prim->callbacks.props(prim->callbacks.props_arg, prim, PROP_GET, name,
                                     prop->id, prop->ivalue);
    else
        return prop->ivalue;
}

/* dword wnd_setprop(dword id, char *name, dword value)
 *
 * Setzt den Wert der property NAME aus der Liste des Fensters ID auf VALUE.
 * Gibt nicht-0 zurück, wenn die Property gesetzt wurde, oder 0, wenn die
 * property nicht gefunden wurde.
 *
 */
prop_t wnd_setprop(dword id, const char *name, prop_t value)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    primitive *prim= wnd->self;
    property *prop;

    if( !prim->props || !(prop= props_find_byname(prim->props, name)) )
    {
        dbgw("setprop: no such property '%s'\n", name);
        //~ stacktrace(true);
        return 0;
    }

    if(prim->callbacks.props)
        value= prim->callbacks.props(prim->callbacks.props_arg, prim, PROP_SET, name,
                                     prop->id, value);

    if(prop->type==PROP_DWORD)
        prop->ivalue= value;
    else if(prop->type==PROP_STRING)
    {
        free(prop->pvalue);
        prop->pvalue= strdup((char*)value);
    }
    else
    {
        logmsg("wnd_setprop: unknown prop type %d\n", prop->type);
        return 0;
    }

    return prop->ivalue;
}

int wnd_getproptype(dword id, const char *name)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    primitive *prim= wnd->self;
    property *prop;

    if( !prim->props || !(prop= props_find_byname(prim->props, name)) )
    {
        //~ logmsg("getproptype: no such property '%s'\n", name);
        return -1;
    }

    return prop->type;
}


bool wnd_set_status_callback(dword id, cb_status cb, prop_t arg)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->callbacks.status= cb;
    wnd->self->callbacks.status_arg= arg;

    cb(arg, wnd->self, STAT_CREATE);

    return true;
}

bool wnd_set_kbd_callback(dword id, cb_keybd cb, prop_t arg)
{
    primitive *p= findprim(id);
    if(!p) return false;

    p->callbacks.keybd= cb;
    p->callbacks.keybd_arg= arg;

    return true;
}

bool wnd_set_stickybit(dword id, bool sticky)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->flags.sticky= sticky? 1: 0;

    return true;
}

bool wnd_issticky(dword id)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    return wnd->self->flags.sticky;
}

bool wnd_set_props_callback(dword id, cb_props cb, prop_t arg)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->callbacks.props= cb;
    wnd->self->callbacks.props_arg= arg;
    return true;
}

bool wnd_set_mouse_callback(dword id, cb_mouse cb, prop_t arg)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->callbacks.mouse= cb;
    wnd->self->callbacks.mouse_arg= arg;
    return true;
}

bool wnd_set_paint_callback(dword id, cb_paint cb, prop_t arg)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->callbacks.paint= cb;
    wnd->self->callbacks.paint_arg= arg;
    return true;
}


// Alle Maus-Callbacks auf ein Fenster umleiten (ausschalten mit 0 in id)
bool wnd_set_mouse_capture(dword id)
{
    if(!id || id==NOWND)
    {
        id_mousehook= 0;
        return true;
    }

    window_list *wnd= wlist_find(windows, id);
    if(!wnd) return false;

    if(!wnd->self->callbacks.mouse) return false;

    id_mousehook= id;
    return true;
}

bool wnd_show(dword id, bool visible)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;
    primitive *self= wnd->self;

    if( visible!=(bool)self->flags.visible )
    {
        self->flags.visible= (visible? 1: 0);

        wlist_enum(wnd, call_status_cb, visible? STAT_SHOW: STAT_HIDE);

#ifndef REDRAW_EVERY_FRAME
        rect pos;
        prim_get_abspos(self, &pos, true);
        redraw_rect(&pos);
        update_rect(&pos);
#endif
    }

    return true;
}

// Fenster mit bestimmtem Verhaeltnis zu einem anderen Fenster finden
// z. B. das 1. Kind des 3. Kindes mit (CHILD, NEXT, NEXT, CHILD, THIS)
// Gibt den gefundenen Fenster-ID zurueck,
// 0xFFFFFFFF bei Fehler (id nicht gefunden)
dword wnd_walk(dword id, wnd_relation rel0, ...)
{
    window_list *wnd= wlist_find(windows, id), *nextwnd;
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return 0xFFFFFFFF;

    int rel= rel0;
    va_list ap;
    va_start(ap, rel0);
    nextwnd= wnd;

    // Parameter durchgehen
    while(wnd)
    {
        switch(rel)
        {
            // Dieses Fenster
        case SELF:
            va_end(ap);
            return wnd->self->id;

            // vorheriges
        case PREV:
            nextwnd= wnd->prev;
            break;

            // naechstes
        case NEXT:
            nextwnd= wnd->next;
            break;

            // erstes child
        case CHILD:
            nextwnd= wnd->self->children;
            break;

            // parent
        case PARENT:
        {
            // parent-primitive
            primitive *parent= wnd->self->parent;

            // wenn vorhanden
            if(parent)
            {
                // in Fenster- und Gruppenliste suchen
                nextwnd= wlist_find(windows, parent->id);
                if(!nextwnd) nextwnd= wlist_find(groups, parent->id);
            }
            // sonst 0
            else
                nextwnd= 0;

            break;
        }

        case FIRST:
            while(nextwnd->prev) nextwnd= nextwnd->prev;
            break;

        case LAST:
            while(nextwnd->next) nextwnd= nextwnd->next;
            break;

            // unbekannte relation, Fehler
        default:
            logmsg("wnd_walk(): unknown wnd_relation %d (vararg list not terminated?)\n", rel);
            return 0xFFFFFFFF;
        }

        rel= va_arg(ap, int);
        wnd= nextwnd;
    }

    // Fehler, Fenster mit diesem Verhaeltnis nicht vorhanden
    va_end(ap);
    return 0xFFFFFFFF;
}

// Fenster sichtbar?
bool wnd_isvisible(dword id)
{
    window_list *wlist= windows;
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id), wlist= groups;
    if(!wnd) return false;

    while(wnd)
    {
        if(!wnd->self->flags.visible) return false;
        wnd= (wnd->self->parent? wlist_find(wlist, wnd->self->parent->id): 0);
    }

    return true;
}

bool wnd_isresizable(dword id)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    return wnd->self->flags.resizable;
}

bool wnd_setresizable(dword id, bool resizable)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->flags.resizable= resizable;
    return true;
}

bool wnd_setalignment(dword id, dword align)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    wnd->self->alignment= align;
    return true;
}


dword wnd_getalignment(dword id)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    return wnd->self->alignment;
}




/*
   Kind K eines Primitives P ist Teil des linken Frames von P, wenn
   K und alle Parents bis zu P:
    - Das is_frame Bit gesetzt haben
    - Links ausgerichtet sind, nicht rechts oder zentriert
    - Keine relative Breite oder X-Position haben

   Aequivalentes gilt fuer Teile des oberen, rechten und unteren Frames.
*/

#define FRAME_LEFT      (WREL|XREL|ALIGN_RIGHT|ALIGN_HCENTER)
#define FRAME_RIGHT     (WREL|XREL|ALIGN_LEFT|ALIGN_HCENTER)
#define FRAME_TOP       (HREL|YREL|ALIGN_BOTTOM|ALIGN_VCENTER)
#define FRAME_BTM       (HREL|YREL|ALIGN_TOP|ALIGN_VCENTER)

bool is_frame_part(primitive *parent, primitive *child, int frameflags)
{
    while(child!=parent)
    {
        if( !child->flags.is_frame || child->alignment&frameflags )
            return false;

        child= child->parent;
    }

    return true;
}



void calc_frame_size(primitive *frameparent, window_list *children, rect *dest)
{
    window_list *child= children;

    while(child)
    {
        primitive *c= child->self;

        if(c->type==PT_FRAME)
        {
            prim_frame *frame= (prim_frame *)c;
            if(dest->x < frame->framewidth) dest->x= frame->framewidth;
            if(dest->rgt > -frame->framewidth) dest->rgt= -frame->framewidth;
            if(dest->y < frame->framewidth) dest->y= frame->framewidth;
            if(dest->btm > -frame->framewidth) dest->btm= -frame->framewidth;
        }

        else if( is_frame_part(frameparent, c, FRAME_LEFT) && c->rgt > dest->x )
            dest->x= c->rgt;

        else if( is_frame_part(frameparent, c, FRAME_RIGHT) && c->rgt > -dest->rgt )
            dest->rgt= -c->rgt;

        else if( is_frame_part(frameparent, c, FRAME_TOP) && c->btm > dest->y )
            dest->y= c->btm;

        else if( is_frame_part(frameparent, c, FRAME_BTM) && c->btm > -dest->btm )
            dest->btm= -c->btm;

        if(c->children) calc_frame_size(frameparent, c->children, dest);
        child= child->next;
    }
}


void calc_nonframe_rect(primitive *frameparent, rect& dest)
{
    rect tmp= { 0,0, 0,0 };
    calc_frame_size(frameparent, frameparent->children, &tmp);

    prim_get_abspos(frameparent, &dest, false);
    dest+= tmp;
}



int wnd_getx(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        rect rc;
        prim_get_abspos(wnd->self, &rc, false);
        return rc.x;
    }

    return 0;
}

int wnd_gety(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        rect rc;
        prim_get_abspos(wnd->self, &rc, false);
        return rc.y;
    }

    return 0;
}

int wnd_getrelx(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd) return wnd->self->x;

    return 0;
}

int wnd_getrely(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd) return wnd->self->y;

    return 0;
}

int wnd_getrelw(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd) return wnd->self->rgt-wnd->self->x;

    return 0;
}

int wnd_getrelh(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd) return wnd->self->btm-wnd->self->y;

    return 0;
}


int wnd_getalign(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd) return wnd->self->alignment;
    return 0;
}


int wnd_getw(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        rect rc;
        prim_get_abspos(wnd->self, &rc, false);
        return rc.rgt-rc.x;
    }

    return 0;
}

int wnd_geth(dword id)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        rect rc;
        prim_get_abspos(wnd->self, &rc, false);
        return rc.btm-rc.y;
    }

    return 0;
}

int wnd_get_abspos(dword id, rect *pos)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        prim_get_abspos(wnd->self, pos, false);

        return 1;
    }

    return 0;
}

void prim_setcolor(primitive *prim, dword color)
{
    rect pos;
    ((prim_rect *)prim)->color= color;

#ifndef REDRAW_EVERY_FRAME
    prim_get_abspos(prim, &pos, true);
    redraw_rect(&pos);
    update_rect(&pos);
#endif
}

bool rect_setcolor(dword id, dword color)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    if( wnd->self->type!=PT_RECT && wnd->self->type!=PT_GROUP && wnd->self->type!=PT_TEXT ) return false;

    prim_setcolor(wnd->self, color);

    return true;
}

bool text_setcolor(dword id, dword color)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    if(wnd->self->type!=PT_TEXT) return false;

    prim_setcolor(wnd->self, color);

    return true;
}


bool text_settext(dword id, const char *text)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    prim_text *self= (prim_text *)wnd->self;

    if(self->type!=PT_TEXT) return false;

    if(self->text) free(self->text);

    self->text= strdup(text);

#ifndef REDRAW_EVERY_FRAME
    if(self->flags.visible)
    {
        rect pos;
        prim_get_abspos(wnd->self, &pos, true);
        redraw_rect(&pos);
        update_rect(&pos);
    }
#endif

    return true;
}

bool text_gettext(dword id, char *text, int bufsize)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);
    if(!wnd) return false;

    prim_text *self= (prim_text *)wnd->self;

    strncpy(text, self->text, bufsize-1);
    text[bufsize-1]= 0;

    return true;
}

void checkpos(primitive *p)
{
    int xd= 0, yd= 0;
#define MINMARGIN 16
    if(!p->parent)
    {
    	if( (HALIGN(p->alignment)==ALIGN_LEFT|ALIGN_RIGHT) ||
			(VALIGN(p->alignment)==ALIGN_TOP|ALIGN_BOTTOM) ||
			(p->alignment&(XREL|YREL|WREL|HREL)) )
			return;

        if(p->x>viewport.rgt-MINMARGIN) xd= (viewport.rgt-MINMARGIN)-p->x;
        if(p->y>viewport.btm-MINMARGIN) yd= (viewport.btm-MINMARGIN)-p->y;
        if(p->rgt<MINMARGIN) xd= MINMARGIN-p->rgt;
        if(p->btm<MINMARGIN) yd= MINMARGIN-p->btm;
        p->x+= xd;
        p->rgt+= xd;
        p->y+= yd;
        p->btm+= yd;
    }
}

void wnd_setpos(dword id, int x, int y)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;

        if(self->x==x && self->y==y) return;

        rect rc1, rc2;

        prim_get_abspos(self, &rc1, true);

        self->rgt= x + self->rgt-self->x;
        self->btm= y + self->btm-self->y;
        self->x= x;
        self->y= y;

        checkpos(self);

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &rc2, true);

            int i, n;
            rect subrcs[4];

            redraw_rect(&rc1);

            if( (n= subtract_rect(rc2, rc1, subrcs)) )
            {
                for(i= n-1; i>=0; i--)
                    redraw_rect(&subrcs[i]);

                for(i= n-1; i>=0; i--)
                    update_rect(&subrcs[i]);
            }
            else
            {
                redraw_rect(&rc2);
                update_rect(&rc2);
            }

            update_rect(&rc1);
        }
#endif
    }
}


void wnd_sety(dword id, int y)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;

        if(self->y==y) return;

        rect rc1, rc2;

        prim_get_abspos(self, &rc1, true);

        self->btm= y + self->btm-self->y;
        self->y= y;

        checkpos(self);

        if(self->flags.visible)
        {
            prim_get_abspos(self, &rc2, true);

            int i, n;
            rect subrcs[4];

#ifndef REDRAW_EVERY_FRAME
            redraw_rect(&rc1);

            if( (n= subtract_rect(rc2, rc1, subrcs)) )
            {
                for(i= n-1; i>=0; i--)
                    redraw_rect(&subrcs[i]);

                for(i= n-1; i>=0; i--)
                    update_rect(&subrcs[i]);
            }
            else
            {
                redraw_rect(&rc2);
                update_rect(&rc2);
            }

            update_rect(&rc1);
#endif
        }
    }
}

void wnd_setx(dword id, int x)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;

        if(self->x==x) return;

        rect rc1, rc2;

        prim_get_abspos(self, &rc1, true);

        self->rgt= x + self->rgt-self->x;
        self->x= x;

        checkpos(self);

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &rc2, true);

            int i, n;
            rect subrcs[4];

            redraw_rect(&rc1);

            if( (n= subtract_rect(rc2, rc1, subrcs)) )
            {
                for(i= n-1; i>=0; i--)
                    redraw_rect(&subrcs[i]);

                for(i= n-1; i>=0; i--)
                    update_rect(&subrcs[i]);
            }
            else
            {
                redraw_rect(&rc2);
                update_rect(&rc2);
            }

            update_rect(&rc1);
        }
#endif
    }
}

void wnd_setisize(dword id, int iwidth, int iheight)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;
        rect pos1, pos2;

        prim_get_abspos(self, &pos1, true);

        self->rgt= self->x + iwidth + self->rcnonframe.x-self->rcnonframe.rgt;
        self->btm= self->y + iheight + self->rcnonframe.y-self->rcnonframe.btm;

        checkpos(self);

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &pos2, true);

            rect rmax= { min(pos1.x,pos2.x), min(pos1.y,pos2.y),
                         max(pos1.rgt,pos2.rgt), max(pos1.btm,pos2.btm)
                       };

            redraw_rect(&rmax);
            update_rect(&rmax);
        }
#endif
    }
}

void wnd_setsize(dword id, int width, int height)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;
        rect pos1, pos2;

        prim_get_abspos(self, &pos1, true);

        self->rgt= self->x+width;  // +self->rcnonframe.x
        self->btm= self->y+height; // +self->rcnonframe.y

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &pos2, true);

            rect rmax= { min(pos1.x,pos2.x), min(pos1.y,pos2.y),
                         max(pos1.rgt,pos2.rgt), max(pos1.btm,pos2.btm)
                       };

            redraw_rect(&rmax);
            update_rect(&rmax);
        }
#endif
    }
}


void wnd_setwidth(dword id, int width)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;
        rect pos1, pos2;

        prim_get_abspos(self, &pos1, true);

        self->rgt= self->x+width;

        checkpos(self);

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &pos2, true);

            rect rmax= { min(pos1.x,pos2.x), min(pos1.y,pos2.y),
                         max(pos1.rgt,pos2.rgt), max(pos1.btm,pos2.btm)
                       };

            redraw_rect(&rmax);
            update_rect(&rmax);
        }
#endif
    }
}

void wnd_setheight(dword id, int height)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;
        rect pos1, pos2;

        prim_get_abspos(self, &pos1, true);

        self->btm= self->y+height;

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &pos2, true);

            rect rmax= { min(pos1.x,pos2.x), min(pos1.y,pos2.y),
                         max(pos1.rgt,pos2.rgt), max(pos1.btm,pos2.btm)
                       };

            redraw_rect(&rmax);
            update_rect(&rmax);
        }
#endif
    }
}


void wnd_setiheight(dword id, int height)
{
    window_list *wnd= wlist_find(windows, id);

    if(wnd)
    {
        primitive *self= wnd->self;
        rect pos1, pos2;

        prim_get_abspos(self, &pos1, true);

        self->btm= self->y+height + self->rcnonframe.y-self->rcnonframe.btm;;

        checkpos(self);

#ifndef REDRAW_EVERY_FRAME
        if(self->flags.visible)
        {
            prim_get_abspos(self, &pos2, true);

            rect rmax= { min(pos1.x,pos2.x), min(pos1.y,pos2.y),
                         max(pos1.rgt,pos2.rgt), max(pos1.btm,pos2.btm)
                       };

            redraw_rect(&rmax);
            update_rect(&rmax);
        }
#endif
    }
}


void wnd_set_zpos(dword id, int state)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);

    if(wnd)
    {
        window_list *dest;
        switch(state)
        {
        case Z_TOP:
            dest= wlist_end(wnd);
            break;
        case Z_NORMAL:
            dest= wlist_top(wnd);
            break;
        case Z_BOTTOM:
            dest= wlist_bottom(wnd);
            break;
        default:
            return;
        }

        wlist_move(wnd, dest);
        wnd->self->flags.z_pos= state;
    }
}

void wnd_totop(dword id)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) wnd= wlist_find(groups, id);

    if(wnd)
    {
        window_list *dest;
        switch(wnd->self->flags.z_pos)
        {
        case Z_TOP:
            dest= wlist_end(wnd);
            break;
        case Z_NORMAL:
            dest= wlist_top(wnd);
            break;
        case Z_BOTTOM:
            dest= wlist_bottom(wnd);
            break;
        }

        wlist_move(wnd, dest);
    }
}

void wlist_move(window_list *wnd, window_list *dest)
{
    if(wnd && wnd!=dest)
    {
//    logmsg("wlist_move wnd= %p dest= %p\n", wnd, dest);
        if(wnd!=dest)
        {
            if(wnd->self->parent && !wnd->prev)
                wnd->self->parent->children= wnd->next;

            if(wnd->prev) wnd->prev->next= wnd->next;
            if(wnd->next) wnd->next->prev= wnd->prev;

            wnd->next= dest->next;
            wnd->prev= dest;
            dest->next= wnd;
            if(wnd->next) wnd->next->prev= wnd;

#ifndef REDRAW_EVERY_FRAME
            redraw_rect(wnd->self);
            update_rect(wnd->self);
#endif
        }
    }
}




void wnd_destroy(dword id)
{
    if(!id) return;
    window_list *wnd= wlist_find(windows, id), *prev= 0, *next= 0;
    if(!wnd) wnd= wlist_find(groups, id);

    if(wnd)
    {
        primitive *self= wnd->self;

        // stat_destroy aufrufen
        wlist_enum(wnd, call_status_cb, STAT_DESTROY);

        rect upd;
        prim_get_abspos(self, &upd, true);

        prev= wnd->prev;
        next= wnd->next;

        if(wnd->self->children)
        {
            wlist_free(wnd->self->children);
            wnd->self->children= 0;
        }

        if(wnd->self->parent && wnd->self->parent->children==wnd)
            wnd->self->parent->children= next;

        if(prev) prev->next= next;
        if(next) next->prev= prev;


        // Dieser Code macht folgendes:
        // Wenn der Kopf der Fenster- oder Gruppenliste zerstoert worden ist
        //   Kopf= Kopf->Next
        //   Wenn Kopf==0
        //     Kopf= Neues-Dummy-Fenster    (damit der Kopf nie NULL ist)
        //   Endewenn
        // Endewenn
        // Hmmmmmm...... Zu viel Kaffee?
        window_list **head;
        if( wnd==*(head= &windows) || wnd==*(head= &groups) )
        {
            *head= next;
            if(!*head)
            {
                (*head)= (window_list *)calloc(1, sizeof(window_list));
                (*head)->self= (primitive *)calloc(1, sizeof(prim_rect));
                (*head)->self->type= PT_RECT;
                (*head)->self->id= NOWND;
            }
            /*
                  logmsg("head destroyed\n");
                  logmsg("List: %08X ", *head);
                  if(*head) logmsg("Prev: %08X Self: %08X Next: %08X\n", (*head)->prev, (*head)->self, (*head)->next);
                  else logmsg("\n");
            */
        }

        prim_destroy(wnd->self);
        free(wnd->self);
        free(wnd);

#ifndef REDRAW_EVERY_FRAME
        redraw_rect(&upd);
        update_rect(&upd);
#endif
    }
}

void wnd_close(dword id)
{
    aq_push(AQ_W_CLOSE, id);
}

void wnd_minimize(dword id)
{
    aq_push(AQ_W_MINIMIZE, id);
}



void wnd_forceredraw(dword id, rect *rc)
{
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) return;
    primitive *prim= wnd->self;
    rect pos;

    prim_get_abspos(prim, &pos, true);

    if(rc)
    {
        rc->x+= pos.x;
        rc->y+= pos.y;
        rc->rgt+= pos.x;
        rc->btm+= pos.y;
        clip_rect(&pos, rc);
    }

    redraw_rect(&pos);
    update_rect(&pos);
}

void prim_destroy(primitive *prim)
{
// @! Mach ich das hier oder wo anders?
//  if(prim->callbacks.status)
//    prim->callbacks.status(prim->callbacks.status_arg, prim, STAT_DESTROY);

    if(prim->props)
    {
        props_free(prim->props);
        prim->props= 0;
    }

    switch(prim->type)
    {
//      @!
    case PT_GROUP:
        free( ((prim_group*)prim)->name );
        break;

    case PT_TEXT:
        free( ((prim_text*)prim)->text );
        font_deref( ((prim_text*)prim)->font );
        break;

    case PT_RECT:
        break;

    case PT_BITMAP:
        free( ((prim_bitmap *)prim)->pixels );
        break;

    case PT_FRAME:
        break;

    default:
//        logmsg("prim_destroy: unknown primitive type %d\n", prim->type);
        break;
    }

    unhash_id(prim->id);
}


window_list *prim_create(window_list *prev, int structsize, bool is_child,
                         int x,int y, int rgt,int btm, int align)
{
    primitive *prim= (primitive *)calloc(1, structsize);
    window_list *wnd= (window_list *)calloc(1, sizeof(window_list));
    window_list *next;

    if(!HALIGN(align)) align|= ALIGN_LEFT;
    if(!VALIGN(align)) align|= ALIGN_TOP;

    prim->x= x;
    prim->y= y;
    prim->rgt= rgt;
    prim->btm= btm;
    prim->alignment= align;
    prim->flags.visible= true;
    prim->flags.resizable= true;

    wnd->self= prim;

    if(prev)
    {
        prim->id= unique_id();

        if(is_child)
        {
            prim->parent= prev->self;

            if(!prev->self->children)
            {
                prev->self->children= wnd;
                next= 0;
            }
            else
            {
                window_list *inspos= wlist_top(prev->self->children);
                next= inspos->next;
                wnd->prev= inspos;
                wnd->next= inspos->next;
                inspos->next= wnd;
            }
        }
        else
        {
            next= prev->next;
            wnd->prev= prev;
            wnd->next= prev->next;
            prev->next= wnd;
        }

        if(next) next->prev= wnd;

        if( window_hash.lookup(prev->self->id) )
            window_hash.add(prim->id, wnd);
        else if( group_hash.lookup(prev->self->id) )
            group_hash.add(prim->id, wnd);
        else
            logmsg("prim_create: prev id %d not in window list nor group list!\n",
                   prev->self->id);
    }

    return wnd;
}



window_list *prim_create_rect(window_list *prev, bool is_child,
                              int x,int y, int width,int height, int col, int align)
{
    window_list *wnd= prim_create(prev, sizeof(prim_rect), is_child,
                                  x,y, x+width,y+height, align);
    prim_rect *prim= (prim_rect*)wnd->self;

    prim->type= PT_RECT;
    prim->color= col;

    dword c= col|map_color(col);
    if( c==INVISIBLE || (c&TRANSL_MASK) )
        prim->flags.transparent= 1;

    return wnd;
}

window_list *prim_create_text(window_list *prev, bool is_child,
                              int x,int y, int width,int height,
                              const char *text, dword col, font *fnt, int align)
{
    window_list *wnd= prim_create(prev, sizeof(prim_text), is_child,
                                  x,y, x+width,y+height, align);

    prim_text *prim= (prim_text*)wnd->self;

    prim->type= PT_TEXT;
    prim->color= col;
    prim->flags.transparent= 1;
    prim->text= strdup(text);
    prim->font= fnt;
    font_ref(fnt);

    return wnd;
}

window_list *prim_create_bitmap(window_list *prev, bool is_child,
                                int x,int y, int width,int height, int align)
{
    if(width<0 || height<0) return 0;

    window_list *wnd= prim_create(prev, sizeof(prim_bitmap), is_child,
                                  x,y, x+width,y+height, align);
    prim_bitmap *prim= (prim_bitmap*)wnd->self;

    prim->type= PT_BITMAP;
    if( !(prim->pixels= (flux_rgba *)calloc(width*height*4, 1)) )
        return 0;

    return wnd;
}

window_list *prim_create_frame(window_list *prev, bool is_child,
                               int x,int y, int width,int height,
                               int framewidth,
                               dword col_left, dword col_top,
                               dword col_right, dword col_btm,
                               int align)
{
    window_list *wnd= prim_create(prev, sizeof(prim_frame), is_child,
                                  x,y, x+width,y+height, align);
    prim_frame *prim= (prim_frame *)wnd->self;

    prim->type= PT_FRAME;
    prim->framewidth= framewidth;
    prim->col_left= col_left;
    prim->col_top= col_top;
    prim->col_right= col_right;
    prim->col_btm= col_btm;
    prim->flags.is_frame= true;

//  dword c= col_left|col_top|col_right|col_btm;
//  c|= map_color(c);
//  if( c==INVISIBLE || (c&TRANSL_MASK) )
//    prim->transparent= 1;

    prim->flags.transparent= 1;

    return wnd;
}




dword create_rect(dword parent, int x,int y, int width,int height, int col, int align)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_rect(prev, false, x,y,width,height, col, align);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_rect: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }
        wnd= prim_create_rect(prev, true, x,y,width,height, col, align);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;
}

dword create_text(dword parent, int x,int y, int width,int height, const char *text, int col, font *fnt, int align)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_text(prev, false, x,y,width,height, text, col, fnt, align);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_text: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }
        wnd= prim_create_text(prev, true, x,y,width,height, text, col, fnt, align);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;
}

dword create_bitmap(dword parent, int x,int y, int width,int height, int align)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_bitmap(prev, false, x,y,width,height, align);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_bitmap: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }
        wnd= prim_create_bitmap(prev, true, x,y,width,height, align);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;
}

dword create_frame(dword parent, int x,int y, int width,int height,
                   int framewidth,
                   dword col_left, dword col_top,
                   dword col_right, dword col_btm,
                   int align)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_frame(prev, false, x,y,width,height, framewidth,
                               col_left, col_top, col_right, col_btm, align);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_rect: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }

        wnd= prim_create_frame(prev, true, x,y,width,height, framewidth,
                               col_left, col_top, col_right, col_btm, align);
        prev->self->rcnonframe.x= max(prev->self->rcnonframe.x, framewidth);
        prev->self->rcnonframe.y= max(prev->self->rcnonframe.y, framewidth);
        prev->self->rcnonframe.rgt= min(prev->self->rcnonframe.rgt, -framewidth);
        prev->self->rcnonframe.btm= min(prev->self->rcnonframe.btm, -framewidth);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;
}


void frame_setcolors(dword id, dword col_left, dword col_top, dword col_right, dword col_btm)
{
    bool redraw= true;

    window_list *wnd= wlist_find(windows, id);
    if(!wnd)
    {
        if( !(wnd= wlist_find(groups, id)) ) return;
        redraw= false;
    }

    if(wnd->self->type!=PT_FRAME) return;

    prim_frame *self= (prim_frame *)wnd->self;
    if( self->col_left==col_left && self->col_top==col_top &&
            self->col_right==col_right && self->col_btm==col_btm )
        return;

    self->col_left= col_left;
    self->col_top= col_top;
    self->col_right= col_right;
    self->col_btm= col_btm;


    rect abspos;
    prim_get_abspos(self, &abspos, false);

    rect rcl= { abspos.x, abspos.y, abspos.x+self->framewidth, abspos.btm };
    rect rct= { abspos.x, abspos.y, abspos.rgt, abspos.y+self->framewidth };
    rect rcr= { abspos.rgt-self->framewidth, abspos.y, abspos.rgt, abspos.btm };
    rect rcb= { abspos.x, abspos.btm-self->framewidth, abspos.rgt, abspos.btm };

#ifndef REDRAW_EVERY_FRAME
    redraw_rect(&rcl);
    redraw_rect(&rct);
    redraw_rect(&rcr);
    redraw_rect(&rcb);
    update_rect(&rcl);
    update_rect(&rct);
    update_rect(&rcr);
    update_rect(&rcb);
#endif
}

void frame_setsize(dword id, int size)
{
}


window_list *prim_create_poly(window_list *prev, bool is_child,
                              int x,int y, int width,int height, int col, int align,
                              bool filled, int nverts, dblpos *verts)
{
    window_list *wnd= prim_create(prev, sizeof(prim_shape_poly), is_child,
                                  x,y, x+width,y+height, align);
    prim_shape_poly *prim= (prim_shape_poly*)wnd->self;

    prim->type= PT_POLY;
    prim->flags.transparent= 1;
    prim->color= col;
    prim->filled= filled;
    prim->nverts= nverts;
    prim->vertexes= (fltpos *)calloc(nverts, sizeof(fltpos));

    for(int i= 0; i<nverts; i++)
    {
        prim->vertexes[i].x= verts[i].x;
        prim->vertexes[i].y= verts[i].y;
        if(prim->vertexes[i].x<0) prim->vertexes[i].x= 0;
        if(prim->vertexes[i].y<0) prim->vertexes[i].y= 0;
        if(prim->vertexes[i].x>1) prim->vertexes[i].x= 1;
        if(prim->vertexes[i].y>1) prim->vertexes[i].y= 1;
    }

    return wnd;
}

dword create_poly(dword parent, int x,int y, int width,int height, int col,
                  int align, bool filled, int nverts, dblpos *verts)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_poly(prev, false, x,y,width,height, col, align, filled, nverts, verts);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_rect: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }
        wnd= prim_create_poly(prev, true, x,y,width,height, col, align, filled, nverts, verts);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;


    /*
      dword id= unique_id();
      window_list *newwnd= (window_list *)calloc(1, sizeof(window_list));
      window_list *ins_pos, *next= 0;
      window_list *prnt= 0;
      bool in_group;
      rect upd;

      if(parent==NOPARENT)
      {
        ins_pos= wlist_top(windows);
        ins_pos->next= newwnd;
        in_group= false;
      }
      else
      {
        if( (prnt= wlist_find(windows, parent)) )
          in_group= false;
        else if( (prnt= wlist_find(groups, parent)) )
          in_group= true;
        else
        {
          logmsg("(!) create_shape_fpoly: parent-id %d not found\n", parent);
          return 0xFFFFFFFF;
        }

        if(!prnt->self->children)
        {
          prnt->self->children= newwnd;
          ins_pos= 0;
        }
        else
        {
          ins_pos= wlist_top(prnt->self->children);
          next= ins_pos->next;
          ins_pos->next= newwnd;
        }
      }

      newwnd->self= (prim_shape_poly *)calloc(1, sizeof(prim_shape_poly));
      newwnd->self->x= x;
      newwnd->self->y= y;
      newwnd->self->rgt= x+width;
      newwnd->self->btm= y+height;
      newwnd->self->alignment= align;
      newwnd->self->id= id;
      newwnd->self->parent= (prnt? prnt->self: 0);
      newwnd->self->type= PT_POLY;

      prim_shape_poly *self= (prim_shape_poly *)newwnd->self;
      self->color= col;
      self->filled= filled;
      self->nverts= nverts;
      self->vertexes= (fltpos *)calloc(nverts, sizeof(fltpos));

      int i;
      va_list ap;
      va_start(ap, nverts);

      logmsg("create_poly(%d,", nverts);
      for(i= 0; i<nverts; i++)
      {
        double x= va_arg(ap, double);
        double y= va_arg(ap, double);
        self->vertexes[i].x= x;
        self->vertexes[i].y= y;

        logmsg(" (%.2f %.2f)", x, y);
      }
      logmsg(");\n");

      newwnd->next= next; //0;
      newwnd->prev= ins_pos;

      newwnd->self->transparent= 1;

      if(!in_group)
      {
        prim_get_abspos(newwnd->self, &upd);
        redraw_rect(&upd);
        update_rect(&upd);
      }

      return id;
    */
}


window_list *prim_create_ellipse(window_list *prev, bool is_child,
                                 int x,int y, int width,int height, int col, int align,
                                 bool filled, double cx, double cy, double rx, double ry)
{
    window_list *wnd= prim_create(prev, sizeof(prim_shape_ellipse), is_child,
                                  x,y, x+width,y+height, align);
    prim_shape_ellipse *prim= (prim_shape_ellipse *)wnd->self;

    prim->type= PT_ELLIPSE;
    prim->color= col;
    prim->filled= filled;
    prim->cx= cx;
    prim->cy= cy;
    prim->rx= rx;
    prim->ry= ry;
    prim->flags.transparent= 1;

    return wnd;
}


dword create_ellipse(dword parent, int x,int y, int width,int height, int col, int align, bool filled, double cx, double cy, double rx, double ry)
{
    window_list *prev;
    window_list *wnd;
    bool redraw;

    if(parent==NOPARENT)
    {
        prev= wlist_top(windows);
        wnd= prim_create_ellipse(prev, false, x,y,width,height, col, align, filled, cx,cy, rx,ry);
        redraw= true;
    }
    else
    {
        if( (prev= wlist_find(windows, parent)) )
            redraw= true;
        else if( (prev= wlist_find(groups, parent)) )
            redraw= false;
        else
        {
            logmsg("create_ellipse: parent %d not found\n", parent);
            return 0xFFFFFFFF;
        }
        wnd= prim_create_ellipse(prev, true, x,y,width,height, col, align, filled, cx,cy, rx,ry);
    }

#ifndef REDRAW_EVERY_FRAME
    if(redraw)
    {
        rect abspos;
        prim_get_abspos(wnd->self, &abspos, true);
        redraw_rect(&abspos);
        update_rect(&abspos);
    }
#endif

    return wnd->self->id;

    /*
      dword id= unique_id();
      window_list *newwnd= (window_list *)calloc(1, sizeof(window_list));
      window_list *ins_pos, *next= 0;
      window_list *prnt= 0;
      bool in_group;
      rect upd;

      if(parent==NOPARENT)
      {
        ins_pos= wlist_top(windows);
        next= ins_pos->next;
        ins_pos->next= newwnd;
        in_group= false;
      }
      else
      {
        if( (prnt= wlist_find(windows, parent)) )
          in_group= false;
        else if( (prnt= wlist_find(groups, parent)) )
          in_group= true;
        else
        {
          logmsg("(!) create_shape_fpoly: parent-id %d not found\n", parent);
          return 0xFFFFFFFF;
        }

        if(!prnt->self->children)
        {
          prnt->self->children= newwnd;
          ins_pos= 0;
        }
        else
        {
          ins_pos= wlist_top(prnt->self->children);
          next= ins_pos->next;
          ins_pos->next= newwnd;
        }
      }

      newwnd->self= (prim_shape_ellipse *)calloc(1, sizeof(prim_shape_ellipse));
      newwnd->self->x= x;
      newwnd->self->y= y;
      newwnd->self->rgt= x+width;
      newwnd->self->btm= y+height;
      newwnd->self->alignment= align;
      newwnd->self->id= id;
      newwnd->self->parent= (prnt? prnt->self: 0);
      newwnd->self->type= PT_ELLIPSE;

      prim_shape_ellipse *self= (prim_shape_ellipse *)newwnd->self;
      self->color= col;
      self->filled= filled;
      self->cx= cx;
      self->cy= cy;
      self->rx= rx;
      self->ry= ry;

      newwnd->next= next; //0;
      newwnd->prev= ins_pos;

      newwnd->self->transparent= 1;

      if(!in_group)
      {
        prim_get_abspos(newwnd->self, &upd);
        redraw_rect(&upd);
        update_rect(&upd);
      }

      return id;
    */
}

dword create_arc(dword parent, int x,int y, int width,int height, int col, int align,
                 bool filled, bool lines,
                 double cx,double cy, double rx,double ry,
                 double w1, double w2)
{
    dword id= unique_id();
    window_list *newwnd= (window_list *)calloc(1, sizeof(window_list));
    window_list *ins_pos, *next= 0;
    window_list *prnt= 0;
    bool in_group;
    rect upd;

    if(parent==NOPARENT)
    {
        ins_pos= wlist_top(windows);
        next= ins_pos->next;
        ins_pos->next= newwnd;
        in_group= false;
    }
    else
    {
        if( (prnt= wlist_find(windows, parent)) )
            in_group= false;
        else if( (prnt= wlist_find(groups, parent)) )
            in_group= true;
        else
        {
            logmsg("(!) create_shape_fpoly: parent-id %d not found\n", parent);
            return 0xFFFFFFFF;
        }

        if(!prnt->self->children)
        {
            prnt->self->children= newwnd;
            ins_pos= 0;
        }
        else
        {
            ins_pos= wlist_top(prnt->self->children);
            next= ins_pos->next;
            ins_pos->next= newwnd;
        }
    }

    newwnd->self= (prim_shape_arc *)calloc(1, sizeof(prim_shape_arc));
    newwnd->self->x= x;
    newwnd->self->y= y;
    newwnd->self->rgt= x+width;
    newwnd->self->btm= y+height;
    newwnd->self->alignment= align;
    newwnd->self->id= id;
    newwnd->self->parent= (prnt? prnt->self: 0);
    newwnd->self->type= PT_ARC;

    prim_shape_arc *self= (prim_shape_arc *)newwnd->self;
    self->color= col;
    self->filled= filled;
    self->outlines= lines;
    self->cx= cx;
    self->cy= cy;
    self->rx= rx;
    self->ry= ry;
    self->w1= M_PI*2+M_PI - w1*M_PI*2/360;
    self->w2= M_PI*2+M_PI - w2*M_PI*2/360;

    newwnd->next= next; //0;
    newwnd->prev= ins_pos;

    newwnd->self->flags.transparent= 1;

    if(!in_group)
    {
        prim_get_abspos(newwnd->self, &upd, true);
        redraw_rect(&upd);
        update_rect(&upd);
    }

    return id;
}



int poly_setverts(dword id, int index, int n, ...)
{
    bool in_group= false;
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) in_group= true, wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    prim_shape_poly *prim= (prim_shape_poly *)wnd->self;
    if(prim->type!=PT_POLY)
        return 0;

    if(prim->nverts<index+n)
        return 0;


    va_list ap;
    va_start(ap, n);

    for(int i= index; i<index+n; i++)
    {
        prim->vertexes[i].x= va_arg(ap, double);
        prim->vertexes[i].y= va_arg(ap, double);
    }

    va_end(ap);


    if(!in_group && prim->flags.visible)
    {
        rect upd;

        prim->cache_size.x= 0;      // force redraw

        prim_get_abspos(prim, &upd, true);
        redraw_rect(&upd);
        update_rect(&upd);
    }


    return 1;
}


void invalidate_shape_cache(prim_shape *p)
{
    if(p->scanline_cache) free(p->scanline_cache);
    p->scanline_cache= 0;
    if(p->outline_cache) free(p->outline_cache);
    p->outline_cache= 0;
    p->n_outlinedots= p->cache_size.x= p->cache_size.y= 0;
}

int poly_setallverts(dword id, int n, fltpos *verts)
{
    bool in_group= false;
    window_list *wnd= wlist_find(windows, id);
    if(!wnd) in_group= true, wnd= wlist_find(groups, id);
    if(!wnd) return 0;

    prim_shape_poly *prim= (prim_shape_poly *)wnd->self;
    if(prim->type!=PT_POLY)
        return 0;

    prim->nverts= n;
    if(prim->vertexes) free(prim->vertexes);
    prim->vertexes= (fltpos *)calloc(n, sizeof(fltpos));

    memcpy(prim->vertexes, verts, n*sizeof(fltpos));

    invalidate_shape_cache(prim);      // force redraw

    if(!in_group && prim->flags.visible)
    {
        rect upd;
        prim_get_abspos(prim, &upd, true);
        redraw_rect(&upd);
        update_rect(&upd);
    }


    return 1;
}



dword find_window_pos(int x, int y)
{
    window_list *wnd= wlist_end(windows);
    primitive *self;

    while(wnd)
    {
        self= wnd->self;
        rect abspos;
        prim_get_abspos(self, &abspos, false);

        if( x>=abspos.x && x<=abspos.rgt &&
                y>=abspos.y && y<=abspos.btm &&
                self->flags.visible )
            return self->id;

        wnd= wnd->prev;
    }

    return NOWND;
}

primitive *find_prim_pos(int x, int y, bool mustbevisible)
{
    window_list *wnd= wlist_end(windows);
    rect pos;

    do
    {
        if(!mustbevisible || wnd->self->flags.visible)
        {
            prim_get_abspos(wnd->self, &pos, false);
            if( ptinrect(x, y, pos) ) return wnd->self;
        }
        wnd= wnd->prev;
    }
    while(wnd);

    return 0;
}


primitive *find_handler(primitive::callback_index which, int x, int y, primitive *parent= 0, rect *pos= 0)
{
    if( !parent && !(parent= find_prim_pos(x, y, true)) )
    {
        /*logmsg("no widget\n");*/ return 0;
    }

    rect abspos;
    if(parent->flags.visible)
    {
        if(parent->children)
        {
            window_list *wnd= wlist_end(parent->children);
            primitive *ret;

            do
            {
                primitive *self= wnd->self;

                if( (ret= find_handler(which, x, y, self, pos)) )
                    return ret;

                wnd= wnd->prev;
            }
            while(wnd);
        }


        if((int)which<0 || parent->cb_array[which].func)
        {
            prim_get_abspos(parent, &abspos, true);
            if(ptinrect(x,y, abspos))
            {
                if(pos) *pos= abspos;
                return parent;
            }
        }
    }

    return 0;
}

primitive *find_mouse_handler(int x, int y, rect *pos)
{
    if(id_mousehook)
    {
        window_list *handler= wlist_find(windows, id_mousehook);
        if(handler)
        {
            if(pos) prim_get_abspos(handler->self, pos, false);
            return handler->self;
        }
    }

    return find_handler(primitive::CB_MOUSE, x, y, 0, pos);
}


primitive *find_keybd_handler(int x, int y, rect *pos= 0)
{
    return find_handler(primitive::CB_KEYBD, x, y, 0, pos);
}




rectlist *rectlist_create(const rect *r)
{
	rectlist *l= (rectlist*)calloc(1, sizeof(rectlist));
	l->self= (rect*)malloc(sizeof(rect));
	memcpy(l->self, r, sizeof(rect));
	return l;
}

void rectlist_free(rectlist *head)
{
    if(!head) return;
    rectlist *lst= head, *tmp;

    while(lst)
    {
        if(lst->self) free(lst->self);
        tmp= lst->next;
        free(lst);
        lst= tmp;
    }
}

rectlist *rectlist_subtract_rect(rectlist *rcs, const rect *rc)
{
    int i, nrects= 0;
    rect tmp[4];
    rectlist *src= rcs;
    rectlist *head= (rectlist *)malloc(sizeof(rectlist)), *lst= head;
    head->self= 0;
    head->prev= head->next= 0;

    while(1)
    {
        if( (i= subtract_rect(*src->self, *rc, tmp)) )
        {
            for(i--; i>=0; i--)
            {
                lst->self= (rect *)malloc(sizeof(rect));
                lst->self->x= tmp[i].x;
                lst->self->y= tmp[i].y;
                lst->self->rgt= tmp[i].rgt;
                lst->self->btm= tmp[i].btm;

                lst->next= (rectlist *)malloc(sizeof(rectlist));
                lst->next->next= 0;
                lst->next->prev= lst;
                lst= lst->next;
                nrects++;
            }
        }
        else if(lst->prev)
        {
            lst= lst->prev;
            free(lst->next);
        }

        if(! (src= src->next) ) break;
    }

    if(head->self) return head;
    else
    {
        free(head);
        return 0;
    }
}

void rectlist_push_back(rectlist *l, const rect *r)
{
	while(l->next) l= l->next;
	l->next= rectlist_create(r);
	l->next->prev= l;
}

void rectlist_push_front(rectlist *&l, const rect *r)
{
	while(l->prev) l= l->prev;
	l->prev= rectlist_create(r); //(rectlist*)calloc(sizeof(rectlist));
	l->prev->next= l;
	l= l->prev;
}


int subtract_rect(rect r1, rect r2, rect *dest)
{
    rect *d= dest;
    int nrects= 0;

    if(!clip_rect(&r2, &r1)) return 0;

    if(r2.x>r1.x)
    {
        d->x= r1.x;
        d->rgt= r2.x;
        d->y= r2.y;
        d->btm= r2.btm;
        d++, nrects++;
    }
    if(r2.rgt<r1.rgt)
    {
        d->x= r2.rgt;
        d->rgt= r1.rgt;
        d->y= r2.y;
        d->btm= r2.btm;
        d++, nrects++;
    }
    if(r2.y>r1.y)
    {
        d->y= r1.y;
        d->btm= r2.y;
        d->x= r1.x;
        d->rgt= r1.rgt;
        d++, nrects++;
    }
    if(r2.btm<r1.btm)
    {
        d->y= r2.btm;
        d->btm= r1.btm;
        d->x= r1.x;
        d->rgt= r1.rgt;
        d++, nrects++;
    }

    return nrects;
}


bool ptinrect(int x, int y, rect& rc)
{
    return x>=rc.x && x<rc.rgt && y>=rc.y && y<rc.btm;
}


bool clip_rect(rect *rc, const rect *clp)
{
    if(!rc || !clp)
    {
        logmsg("clip_rect(%p, %p)!!\n", rc, clp);
        video_cleanup();
        exit(1);
    }

    if(rc->x < clp->x)
    {
        rc->x= clp->x;
        if(rc->rgt<clp->x)
            rc->rgt= clp->x;
    }
    if(rc->rgt > clp->rgt)
    {
        rc->rgt= clp->rgt;
        if(rc->x>clp->rgt)
            rc->x= clp->rgt;
    }
    if(rc->y < clp->y)
    {
        rc->y= clp->y;
        if(rc->btm<clp->y)
            rc->btm= clp->y;
    }
    if(rc->btm > clp->btm)
    {
        rc->btm= clp->btm;
        if(rc->y>clp->btm)
            rc->y= clp->btm;
    }

    return !is_empty(rc);
}

void get_child_rel_pos(primitive *child, rect *prntrc, rect *dest)
{
    rect cpos= { child->x, child->y, 0, 0 };


    if(child->alignment&XREL)
    {
        int width= child->rgt-cpos.x;
        cpos.x= ((prntrc->rgt-prntrc->x)*child->x) / MAXSCALE;
        cpos.rgt= cpos.x+width;
    }
    else cpos.x= child->x;

    if(child->alignment&YREL)
    {
        int height= child->btm-cpos.y;
        cpos.y= (prntrc->btm-prntrc->y)*child->y / MAXSCALE;
        cpos.btm= cpos.y+height;
    }
    else cpos.y= child->y;


    if(child->alignment&WREL)
    {
        if(child->alignment&XREL)
            // Damit XREL|WREL items miteinander abschliessen
            cpos.rgt= (prntrc->rgt-prntrc->x)*child->rgt / MAXSCALE;
        else
        {
            int scale= child->rgt-child->x;
            cpos.rgt= cpos.x + (prntrc->rgt-prntrc->x)*scale / MAXSCALE;
        }
    }
    else if(!cpos.rgt)
        cpos.rgt= child->rgt;

    if(child->alignment&HREL)
    {
        if(child->alignment&YREL)
            cpos.btm= (prntrc->btm-prntrc->y)*child->btm / MAXSCALE;
        else
        {
            int scale= child->btm-child->y;
            cpos.btm= cpos.y + (prntrc->btm-prntrc->y)*scale / MAXSCALE;
        }
    }
    else if(!cpos.btm)
        cpos.btm= child->btm;



    switch(HALIGN(child->alignment))
    {
    case ALIGN_RIGHT:
    {
        dest->x= prntrc->rgt-cpos.rgt;
        dest->rgt= prntrc->rgt-cpos.x;
        break;
    }

    case ALIGN_HCENTER:
    {
        dest->x= (((prntrc->x+prntrc->rgt) - (cpos.rgt-cpos.x)) >> 1) + cpos.x;
        dest->rgt= dest->x + (cpos.rgt-cpos.x);
        break;
    }

    case ALIGN_LEFT|ALIGN_RIGHT:
    {
        dest->x= prntrc->x+cpos.x;
        dest->rgt= prntrc->rgt-(cpos.rgt-cpos.x);
        break;
    }

    case ALIGN_HCENTER|ALIGN_RIGHT:
    {
        dest->x= (((prntrc->x+prntrc->rgt) - (cpos.rgt-cpos.x)) >> 1) + cpos.x;
        dest->rgt= prntrc->rgt-(cpos.rgt-cpos.x);
        break;
    }

    case ALIGN_LEFT|ALIGN_HCENTER:
    {
        dest->x= prntrc->x+cpos.x;
        dest->rgt= (((prntrc->x+prntrc->rgt) - (cpos.rgt-cpos.x)) >> 1) - (cpos.rgt-cpos.x);
        break;
    }

    default:
    {
        dest->x= prntrc->x+cpos.x;
        dest->rgt= prntrc->x+cpos.rgt;
        break;
    }
    }

    switch(VALIGN(child->alignment))
    {
    case ALIGN_BOTTOM:
    {
        dest->y= prntrc->btm-cpos.btm;
        dest->btm= prntrc->btm-cpos.y;
        break;
    }

    case ALIGN_VCENTER:
    {
        dest->y= (((prntrc->y+prntrc->btm) - (cpos.btm-cpos.y)) >> 1) + cpos.y;
        dest->btm= dest->y + (cpos.btm-cpos.y);
        break;
    }

    case ALIGN_TOP|ALIGN_BOTTOM:
    {
        dest->y= prntrc->y+cpos.y;
        dest->btm= prntrc->btm-(cpos.btm-cpos.y);
        break;
    }

    case ALIGN_VCENTER|ALIGN_BOTTOM:
    {
        dest->y= (((prntrc->y+prntrc->btm) - (cpos.btm-cpos.y)) >> 1) + cpos.y;
        dest->btm= prntrc->btm-(cpos.btm-cpos.y);
        break;
    }

    case ALIGN_TOP|ALIGN_VCENTER:
    {
        dest->y= prntrc->y+cpos.y;
        dest->btm= (((prntrc->y+prntrc->btm) - (cpos.btm-cpos.y)) >> 1) - (cpos.btm-cpos.y);
        break;
    }

    default:
    {
        dest->y= prntrc->y+cpos.y;
        dest->btm= prntrc->y+cpos.btm;
        break;
    }
    }
}



void prim_get_abspos(primitive *p, rect *dest, bool clip_to_parent)
{
    rect parent_pos;

    if(p->parent)
    {
        primitive *parent= p->parent;
        prim_get_abspos(parent, &parent_pos, clip_to_parent);

        if(!p->flags.is_frame)
        {
            parent_pos.x+= parent->rcnonframe.x;
            parent_pos.rgt+= parent->rcnonframe.rgt;
            parent_pos.y+= parent->rcnonframe.y;
            parent_pos.btm+= parent->rcnonframe.btm;
        }
    }
    else
    {
        parent_pos= viewport;
    }

    get_child_rel_pos(p, &parent_pos, dest);
    if(clip_to_parent && p->parent) clip_rect(dest, &parent_pos);
}



int scanline_callback(scanline_cb_struct *arg, int x, int y)
{
    int ndot= arg->n_outlinedots++;
    arg->outlinedots[ndot].x= x + 0x7FFF;
    arg->outlinedots[ndot].y= y + 0x10000;

    if(arg->fill)
    {
        if(y>>16 >= 0 && y>>16 < arg->height)
        {
            shape_scanline& dest= arg->scanlines[(y+0x7FFF>>16)];

            if(x<dest.x1)
                dest.x1= x + 0x7FFF;

            if(x>dest.x2)
                dest.x2= x - 0x7FFF;
        }
    }

    return ndot;
}


int polyscanline_cb_left(scanline_cb_struct *arg, int x, int y)
{
    int ndot= arg->n_outlinedots++;
    arg->outlinedots[ndot].x= x + 0x7FFF;
    arg->outlinedots[ndot].y= y + 0x10000;
	if(y>>16 >= 0 && y>>16 < arg->height)
    {
        shape_scanline& dest= arg->scanlines[(y+0x7FFF>>16)];

        if(x+0x7FFF<dest.x1)
            dest.x1= x + 0x7FFF;
    }
    return ndot;
}

int polyscanline_cb_right(scanline_cb_struct *arg, int x, int y)
{
    int ndot= arg->n_outlinedots++;
    arg->outlinedots[ndot].x= x + 0x7FFF;
    arg->outlinedots[ndot].y= y + 0x10000;
    if(y>>16 >= 0 && y>>16 < arg->height)
    {
        shape_scanline& dest= arg->scanlines[(y+0x7FFF>>16)];

        if(x-0x7FFF>dest.x2)
            dest.x2= x - 0x7FFF;
    }
    return ndot;
}


/*
 * Rechteck in primitive p neuzeichnen
 *
 * p: das primitive
 * abspos: die absolute Position (kann NULL sein, wird dann ermittelt)
 * upd: das Rechteck
 */
static void prim_redraw(primitive *p, rect *_abspos, rect *upd)
{
    if(dep_prim_redraw(p, _abspos, upd))
        return;

    rect abspos;
    dword col;

//    if(p->callbacks.paint)
//    {
//        if(_abspos) abspos= *_abspos;
//        else prim_get_abspos(p, &abspos, true);
//        p->callbacks.paint(p->callbacks.paint_arg, p, &abspos, upd);
//    }
//    else
	switch(p->type)
	{
        case PT_RECT:
        case PT_GROUP:
        {
            fill_rect( upd, map_color(((prim_rect *)p)->color) );
            break;
        }

        case PT_FRAME:
        {
            prim_frame *self= (prim_frame *)p;

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

            // links
            rect rc= { abspos.x, abspos.y, abspos.x+self->framewidth, abspos.btm };
            if( clip_rect(&rc, upd) )
                fill_rect(&rc, map_color(self->col_left));

            // oben
            rc.rgt= abspos.rgt;
            rc.btm= abspos.y+self->framewidth;
            if( clip_rect(&rc, upd) )
                fill_rect(&rc, map_color(self->col_top));

            // rechts
            rc.x= abspos.rgt-self->framewidth;
            rc.btm= abspos.btm;
            if( clip_rect(&rc, upd) )
                fill_rect(&rc, map_color(self->col_right));

            // unten
            rc.x= abspos.x;
            rc.y= abspos.btm-self->framewidth;
            rc.btm= abspos.btm;
            if( clip_rect(&rc, upd) )
                fill_rect(&rc, map_color(self->col_btm));

            break;
        }

        case PT_TEXT:
        {
            prim_text *self= (prim_text *)p;

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

            draw_text(_font_getloc(self->font), self->text, abspos.x,abspos.y, *upd,
                      self->color);

            break;
        }

        case PT_POLY:
        {
            prim_shape_poly *self= (prim_shape_poly *)p;
            col= map_color(self->color);

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

            int width= abspos.rgt-abspos.x;
            int height= abspos.btm-abspos.y;
            int i, x1,x2, y1,y2;

            clip_rect(upd, &viewport);


            shape_scanline *scanlines= self->scanline_cache;

            // refresh scanline cache if necessary
            if(self->cache_size.x!=width || self->cache_size.y!=height)
            {
                if(self->scanline_cache) free(self->scanline_cache);
                self->scanline_cache=
                    scanlines= (shape_scanline*)malloc(height*sizeof(shape_scanline));

                int ndots= 0; //= abspos.btm-abspos.y;

                // count number of outline dots
                for(i= 0; i<self->nverts-1; i++)
                {
                    x1= int( (self->vertexes[i].x  *width) );
                    x2= int( (self->vertexes[i+1].x*width) );
                    y1= int( (self->vertexes[i].y  *height) );
                    y2= int( (self->vertexes[i+1].y*height) );
                    ndots+= max( abs(x2-x1), abs(y2-y1) )+1;
                }

                x1= int( (self->vertexes[self->nverts-1].x*width) );
                x2= int( (self->vertexes[0].x*width) );
                y1= int( (self->vertexes[self->nverts-1].y*height) );
                y2= int( (self->vertexes[0].y*height) );
                ndots+= max( abs(x2-x1), abs(y2-y1) )+1;

                // reallocate outline dot cache
                if(self->outline_cache) free(self->outline_cache);
                self->outline_cache= (pos *)malloc(ndots*2*sizeof(pos));

                for(i= 0; i<height; i++)
                    scanlines[i].x1= 32000<<16, scanlines[i].x2= 0;

                scanline_cb_struct cb_arg= { scanlines, self->outline_cache,
                                             width, height,
                                             col, self->filled,
                                             0
                                           };

                for(i= 0; i<self->nverts-1; i++)
                {
                    x1= int( (self->vertexes[i].x  *(width-1) )*65536 );
                    x2= int( (self->vertexes[i+1].x*(width-1) )*65536 );
                    y1= int( (self->vertexes[i].y  *(height-1))*65536 );
                    y2= int( (self->vertexes[i+1].y*(height-1))*65536 );

                    x1&= ~0xFFFF;
                    y1&= ~0xFFFF;
                    x2&= ~0xFFFF;
                    y2&= ~0xFFFF;

                    cb_polyline(x1,y1, x2,y2,
                                (scanline_cb)polyscanline_cb_left,
                                (scanline_cb)polyscanline_cb_right,
                                (void*)&cb_arg);
                }

                x1= int( (self->vertexes[self->nverts-1].x  *(width-1) )*65536 );
                x2= int( (self->vertexes[0].x*(width-1) )*65536 );
                y1= int( (self->vertexes[self->nverts-1].y  *(height-1))*65536 );
                y2= int( (self->vertexes[0].y*(height-1))*65536 );

                x1&= ~0xFFFF;
                y1&= ~0xFFFF;
                x2&= ~0xFFFF;
                y2&= ~0xFFFF;

                cb_polyline(x1,y1, x2,y2,
                            (scanline_cb)polyscanline_cb_left,
                            (scanline_cb)polyscanline_cb_right,
                            (void*)&cb_arg);

                self->n_outlinedots= cb_arg.n_outlinedots;
                self->cache_size.x= width;
                self->cache_size.y= height;
            }

                  pos fpos= { abspos.x<<16, abspos.y<<16 };
                  plot_outline(self->outline_cache, self->n_outlinedots,
                               self->color, *upd, fpos);

//      logmsg("cache w: %d - cache h: %d, nverts: %d", self->cache_size.x, self->cache_size.y, self->nverts);

/*
            for(i= 0; i<self->nverts-1; i++)
            {
                x1= int( (self->vertexes[i].x  *(width-1) ) )+abspos.x;
                x2= int( (self->vertexes[i+1].x*(width-1) ) )+abspos.x;
                y1= int( (self->vertexes[i].y  *(height-1)) )+abspos.y;
                y2= int( (self->vertexes[i+1].y*(height-1)) )+abspos.y;

//@! 9. 10. 2003: Clipping wäre besser, funktioniert aber nicht
//        pos p1, p2;
//        clip_line(x1,y1, x2,y2, *upd, &p1, &p2);

//        line(x1+fpos.x,y1+fpos.y, x2+fpos.x,y2+fpos.y, self->color);
//        line(p1.x<<16,p1.y<<16, p2.x<<16,p2.y<<16, col);
//                line(x1<<16,y1<<16, x2<<16,y2<<16, col, *upd);
            }


            x1= int( (self->vertexes[self->nverts-1].x  *(width-1) ) )+abspos.x;
            x2= int( (self->vertexes[0].x*(width-1) ) )+abspos.x;
            y1= int( (self->vertexes[self->nverts-1].y  *(height-1)) )+abspos.y;
            y2= int( (self->vertexes[0].y*(height-1)) )+abspos.y;

            line(x1<<16,y1<<16, x2<<16,y2<<16, col, *upd);
*/

            if(self->filled)
                fill_scanlines(scanlines, *upd, abspos, col);

            break;
        }

        case PT_ELLIPSE:
        {
            prim_shape_ellipse *self= (prim_shape_ellipse *)p;
            col= map_color(self->color);

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

            int width= abspos.rgt-abspos.x;
            int height= abspos.btm-abspos.y;
            int i; //, x1,x2, y1,y2;

            clip_rect(upd, &viewport);

            shape_scanline *scanlines= self->scanline_cache;

            //~ logmsg("pt_ellipse (%d %d)-(%d %d)\n", upd->x,upd->y,upd->rgt,upd->btm);

            if(self->cache_size.x!=width || self->cache_size.y!=height)
            {
                //~ logmsg("ellipse: caching scanlines %dx%d", width, height);

                if(self->scanline_cache) free(self->scanline_cache);
                self->scanline_cache=
                    scanlines= (shape_scanline*)malloc(height*sizeof(shape_scanline));

                int ndots= int( max(self->rx*width, self->ry*height) * M_PI*2 ) + 4;
                if(self->outline_cache) free(self->outline_cache);
                self->outline_cache= (pos *)malloc(ndots*sizeof(pos));

                for(i= 0; i<height; i++)
                    scanlines[i].x1= 32000<<16, scanlines[i].x2= 0;

                scanline_cb_struct cb_arg= { scanlines, self->outline_cache,
                                             width, height,
                                             col, self->filled
                                           };

                self->n_outlinedots=
                    cb_ellipse(int(self->cx*width*65535)-32768,
                               int(self->cy*height*65535)-32768,
                               self->rx*width-1, self->ry*height-1,
                               (scanline_cb)scanline_callback, (void*)&cb_arg);

                self->cache_size.x= width;
                self->cache_size.y= height;
            }

            pos fpos= { abspos.x<<16, abspos.y<<16 };
            plot_outline(self->outline_cache, self->n_outlinedots, self->color,
                         *upd, fpos);

            if(self->filled)
                fill_scanlines(scanlines, *upd, abspos, col);

            /*    @! Alte Version?
                  shape_scanline scanlines[height];
                  for(i= 0; i<sizeof(scanlines)/sizeof(scanlines[0]); i++)
                    scanlines[i].x1= 32767<<16, scanlines[i].x2= 0;

                  scanline_cb_struct cb_arg= { scanlines, *upd, abspos,
                                               self->color, self->filled  };

                  cb_ellipse((abspos.x<<16)+int(self->cx*65536*width)-32768,
                             (abspos.y<<16)+int(self->cy*65536*height)-32768,
                             self->rx*width-1, self->ry*height-1,
                             (scanline_cb)scanline_callback, (void*)&cb_arg);

                  if(self->filled)
                    fill_scanlines(scanlines, *upd, abspos, self->color);
            */


            break;
        }

        case PT_ARC:
        {
            prim_shape_arc *self= (prim_shape_arc *)p;
            col= map_color(self->color);

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

            int width= abspos.rgt-abspos.x;
            int height= abspos.btm-abspos.y;
            int i; //, x1,x2, y1,y2;

            clip_rect(upd, &viewport);

            shape_scanline scanlines[height];
            for(i= 0; i<sizeof(scanlines)/sizeof(scanlines[0]); i++)
                scanlines[i].x1= 32000<<16, scanlines[i].x2= 0;

//      scanline_cb_struct cb_arg= { scanlines, *upd, abspos,
//                                   self->color, self->filled  };
            scanline_cb_struct cb_arg= { scanlines, self->outline_cache,
                                         width, height,
                                         col, self->filled
                                       };

            cb_arc((abspos.x<<16)+int(self->cx*65536*width)-32768,
                   (abspos.y<<16)+int(self->cy*65536*height)-32768,
                   self->rx*width-1, self->ry*height-1,
                   self->w1, self->w2,
                   self->outlines,
                   (scanline_cb)scanline_callback, (void*)&cb_arg);

            if(self->filled)
                fill_scanlines(scanlines, *upd, abspos, col);

            break;
        }

        case PT_BITMAP:
        {
            prim_bitmap *self= (prim_bitmap *)p;

            if(_abspos)
                abspos= *_abspos;
            else
                prim_get_abspos(p, &abspos, true);

//void blt_rgba(rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)

            clip_rect(upd, &viewport);

            rect rc= *upd;
            rc.x-= abspos.x;
            rc.rgt-= abspos.x;
            rc.y-= abspos.y;
            rc.btm-= abspos.y;
            blt_rgba(self->pixels, abspos.rgt-abspos.x,abspos.btm-abspos.y,
                     rc, upd->x,upd->y);
            break;
        }

        default:
        {
            fill_rect(upd, 0);
            logmsg("prim_redraw: unknown primitive type %d\n", p->type);
            break;
        }
    }
}

static void prim_redraw_rectlist(primitive *p, rect *_abspos, rectlist *dirty_list)
{
	rect abspos;
	if(!_abspos) { prim_get_abspos(p, &abspos, true); _abspos= &abspos; }

	if(p->callbacks.paint)
	{
        p->callbacks.paint(p->callbacks.paint_arg, p, _abspos, dirty_list);
		do {
//			fill_rect(dirty_list->self, 0xFF0000 | TRANSL_1);
		} while (dirty_list= dirty_list->next);
	}
	else
		do {
			prim_redraw(p, _abspos, dirty_list->self);
//			fill_rect(dirty_list->self, 0xFF0000 | TRANSL_1);
		} while (dirty_list= dirty_list->next);
}

void redraw_children(rect *prntrc, window_list *wnd, const rect *rc)
{
    window_list *child;

    if( (child= wnd->self->children) ) do
	{
		rect upd= *rc;
		primitive *pchild= child->self;
		rect childrc;

		rect prc= *prntrc;
		if(!pchild->flags.is_frame)
		{
			prc.x+= wnd->self->rcnonframe.x;
			prc.y+= wnd->self->rcnonframe.y;
			prc.rgt+= wnd->self->rcnonframe.rgt;
			prc.btm+= wnd->self->rcnonframe.btm;
		}
		get_child_rel_pos(pchild, &prc, &childrc);

		if(pchild->flags.visible & clip_rect(&upd, &childrc) & clip_rect(&upd, &prc))
		{
			prim_redraw(pchild, &childrc, &upd);

			redraw_children(&childrc, child, &upd);
		}

	} while( (child= child->next) );
}

static void recurse_redraw_rect(rect *rc, window_list *wnd)
{
    if(is_empty(rc)) return;

    // die fensterliste "von oben nach unten" durchgehen und alles nötige neu zeichnen
    do
    {
        rect upd= { rc->x,rc->y, rc->rgt,rc->btm };
        rect clp= { rc->x,rc->y, rc->rgt,rc->btm };
        primitive *self= wnd->self;

        rect abspos;
        prim_get_abspos(self, &abspos, true);

        // testen ob es überhaupt sichtbar und das gui aktiv ist
        if(self->flags.visible & (gui_active | self->flags.sticky) & clip_rect(&clp, &abspos))
        {
        	// bei transparenten fenstern muss zuerst der hintergrund neu gezeichnet werden
            if(self->flags.transparent && wnd->prev)
                recurse_redraw_rect(&upd, wnd->prev);

            // jetzt das primitive neu zeichnen
            prim_redraw(self, &abspos, &clp);

            // alle kinder neu zeichnen
            redraw_children(&abspos, wnd, &clp);

            // bei nicht-transparenten fenstern den bereich im rechteck, der nicht von
            // diesem fenster verdeckt wird, auch noch neu zeichnen
            if(!self->flags.transparent)
            {
                int i;
                rect rcs[4];
                if( (i= subtract_rect(upd, clp, rcs)) && wnd->prev )
                {
                    for(i--; i>=0; i--)
                        recurse_redraw_rect(&rcs[i], wnd->prev);
                }
            }

            break;
        }

    } while( (wnd= wnd->prev) );

    /*
      rect rc2= { rc->x,rc->y, rc->rgt,rc->btm };
      rect rc3= { mouse.x,mouse.y, mouse.x+16,mouse.y+16 };

      if(clip_rect(&rc2, &rc3))
      {
        clip_rect(&rc3, &viewport);
    //    clip_rect(&rc3, &video);
        paint_cursor(rc3);
      }
    */
}

static void wlist_add_dirty_rect(window_list *wl, const rect *rc)
{
	if(!wl->dirty_rects)
		wl->dirty_rects= rectlist_create(rc);
	else
		rectlist_push_back(wl->dirty_rects, rc);
}

static void mark_children(rect *prntrc, window_list *wnd, const rect *rc)
{
    window_list *child;

    if( (child= wnd->self->children) ) do
	{
		rect upd= *rc;
		primitive *pchild= child->self;
		rect childrc;

		rect prc= *prntrc;
		if(!pchild->flags.is_frame)
		{
			prc.x+= wnd->self->rcnonframe.x;
			prc.y+= wnd->self->rcnonframe.y;
			prc.rgt+= wnd->self->rcnonframe.rgt;
			prc.btm+= wnd->self->rcnonframe.btm;
		}
		get_child_rel_pos(pchild, &prc, &childrc);

		if(pchild->flags.visible & clip_rect(&upd, &childrc) & clip_rect(&upd, &prc))
		{
			wlist_add_dirty_rect(child, &upd);
			mark_children(&childrc, child, &upd);
		}

	} while( (child= child->next) );
}

static void mark_dirty_rects(rect *rc, window_list *wlist)
{
    if(is_empty(rc)) return;

    // die fensterliste "von oben nach unten" durchgehen und alles nötige neu zeichnen
    do {
        rect upd= { rc->x,rc->y, rc->rgt,rc->btm };
        rect clp= { rc->x,rc->y, rc->rgt,rc->btm };
        primitive *self= wlist->self;

        rect abspos;
        prim_get_abspos(self, &abspos, true);

        // testen ob es überhaupt sichtbar und das gui aktiv ist
        if(self->flags.visible & (gui_active | self->flags.sticky) & clip_rect(&clp, &abspos))
        {
        	// bei transparenten fenstern muss zuerst der hintergrund neu gezeichnet werden
            if(self->flags.transparent && wlist->prev)
                mark_dirty_rects(&upd, wlist->prev);

			wlist_add_dirty_rect(wlist, &clp);

			mark_children(&abspos, wlist, &clp);

            // bei nicht-transparenten fenstern den bereich im rechteck, der nicht von
            // diesem fenster verdeckt wird, auch noch neu zeichnen
            if(!self->flags.transparent)
            {
                int i;
                rect rcs[4];
                if( (i= subtract_rect(upd, clp, rcs)) && wlist->prev )
                {
                    for(i--; i>=0; i--)
                        mark_dirty_rects(&rcs[i], wlist->prev);
                }
            }
            break;
        }
    } while( (wlist= wlist->prev) );
}

static void redraw_dirty_rects(window_list *wl)
{
	do {
		if(wl->dirty_rects)
		{
			rect abspos;
			prim_get_abspos(wl->self, &abspos, true);
			prim_redraw_rectlist(wl->self, &abspos, wl->dirty_rects);
			rectlist_free(wl->dirty_rects);
			wl->dirty_rects= 0;
		}

		if(wl->self->children)
			redraw_dirty_rects(wl->self->children);
	} while( (wl= wl->next) );
}

void redraw_rect(rect *rc)
{
//  recurse_redraw_rect(rc, wlist_end(windows));

	mark_dirty_rects(rc, wlist_end(windows));
	redraw_dirty_rects(windows);
}


void redraw_cursor()
{
    static rect oldpos= { 0,0, 16,16 };

    //~ if(mouse.x==oldpos.x && mouse.y==oldpos.y)
    //~ return;

    rect pos= { mouse_x(),mouse_y(), mouse_x()+16,mouse_y()+16 };
    clip_rect(&pos, &viewport);

    //~ redraw_rect(&oldpos);

    paint_cursor(pos);

    //~ update_rect(&oldpos);
    //~ update_rect(&pos);

    oldpos= pos;
}




void dump_wlist(window_list *src, int indent= 0)
{
    window_list *lst= src;
    char ind[128];
    int i;
    int toplevel= 0;
    const char *typenames[]= { "PT_GROUP", "PT_RECT", "PT_TEXT", "PT_POLY",
                               "PT_ELLIPSE", "PT_ARC", "PT_BITMAP", "PT_FRAME"
                             };

#define INDENTLOG(str, etc...) for(i= 0; i<indent; i++) ind[i]= '.'; ind[i]= 0; logmsg("%s" str, ind, etc);

    if(!src)
    {
        INDENTLOG("dump_wlist: NULL pointer", 0);
        return;
    }

    do
    {
        logmsg("\n");

        INDENTLOG("(%3d, %3d) - (%3d, %3d)\n",
                  lst->self->x,lst->self->y, lst->self->rgt,lst->self->btm);

        rect& rcn= lst->self->rcnonframe;
        if(rcn.x||rcn.y||rcn.rgt||rcn.btm)
        {
            INDENTLOG("rcnonframe: (%3d, %3d) - (%3d, %3d)\n",
                      rcn.x,rcn.y, rcn.rgt,rcn.btm);
        }

        if(lst->self->props)
        {
            INDENTLOG("props: %08lX\n", (ulong)lst->self->props);
        }

        INDENTLOG("z_pos: %s\n", (lst->self->flags.z_pos==Z_NORMAL?"NORMAL":
                                  lst->self->flags.z_pos==Z_BOTTOM?"BOTTOM":
                                  lst->self->flags.z_pos==Z_TOP?"TOP": "???"));

        INDENTLOG("id: %d\n", lst->self->id);
        INDENTLOG("type: %s\n", typenames[lst->self->type]);
        if(lst->self->type==PT_GROUP)
        {
            INDENTLOG("group name: '%s'\n", ((prim_group *)lst->self)->name);
        }
        else if(lst->self->type==PT_RECT || lst->self->type==PT_TEXT)
        {
            INDENTLOG("color: #%08X\n", ((prim_rect *)lst->self)->color);
        }

        INDENTLOG("transparent: %s\n", lst->self->flags.transparent?"true":"false");
        INDENTLOG("is_frame: %s\n", lst->self->flags.is_frame?"true":"false");

        INDENTLOG("alignment: 0x%08X", lst->self->alignment);

        if(lst->self->callbacks.mouse)
        {
            INDENTLOG("cb_mouse: %08lX\n", (ulong)lst->self->callbacks.mouse);
        }

        INDENTLOG("this: %08lX\n", (ulong)lst);
        INDENTLOG("prev: %08lX\n", (ulong)lst->prev);
        INDENTLOG("next: %08lX\n", (ulong)lst->next);
        INDENTLOG("parent: %08lX\n", (ulong)lst->self->parent);
        fflush(stdout);

        if(lst->self->children)
        {
            logmsg("\n");
            INDENTLOG("children:\n", 0);
            fflush(stdout);
            dump_wlist(lst->self->children, indent+1);
        }

        if(lst->self->parent==(void*)0) toplevel++;

    }
    while( (lst= lst->next) );

    if(toplevel) logmsg("%d toplevel windows in list\n", toplevel);
#undef INDENT
}


void dumpwininfo(int x, int y)
{
    primitive *p= find_handler(primitive::CB_MOUSE, x, y);
    if(p) dump_wlist(wlist_find(windows, p->id));
}


void flashwnd(window_list *twnd)
{
    rect trc;

    prim_get_abspos(twnd->self, &trc, true);
    flashrc(&trc);
}




double get_time()
{
    return double(sys_msectime())/1000;
}





// Keyboard & Mouse handling



static dword curkbdhandler= NOWND;

void setkbdhandler(primitive *h)
{
    primitive *oldh;
    if( curkbdhandler!=NOWND && (oldh= findprim(curkbdhandler)) && oldh->callbacks.status )
        oldh->callbacks.status( oldh->callbacks.status_arg, oldh, STAT_LOSEFOCUS );
    if(h && h->callbacks.keybd)
    {
        curkbdhandler= h->id;
        if( h->callbacks.status ) h->callbacks.status( h->callbacks.status_arg, h, STAT_GAINFOCUS );
    }
    else curkbdhandler= NOWND;
}

primitive *getkbdhandler()
{
    return findprim(curkbdhandler);
}

void wnd_setkbdfocus(dword wnd)
{
    primitive *p= findprim(wnd);
    setkbdhandler(p);
}

dword wnd_getkbdfocus()
{
    primitive *h= getkbdhandler();
    return (h? h->id: NOWND);
}


/*
Unicode Umlaute
223 -> ß
228 -> ä
246 -> ö
252 -> ü
196 -> Ä
214 -> Ö
220 -> Ü
*/
bool flux_keyboard_event(bool down, int code, int character)
{
    //~ if(ch && down) logmsg("chr  %d -> %c", ch, ch&(1<<9)? '_': ch);
    //~ if(down && !ch) logmsg("code %d == %c", code, code);

    primitive *handler= getkbdhandler();
    if(handler && handler->callbacks.keybd)
    {
        handler->callbacks.keybd( handler->callbacks.keybd_arg, handler, down, code, character );
        return true;
    }
    return false;
}



primitive *cur_mouse_handler;

void flux_mouse_event(double x, double y, int btn)
{
    static struct mouse oldmouse;
    static dword oldhandler_id= 0;
    static dword moving_window= NOWND;
    static pos move_pos;
    static bool resizing= false;
    static bool moving= false;
    primitive *handler= 0;
    rect h_abspos;
    int event;

#ifdef FLUX_EDM
    extern void demo_event_fluxmouse(double x, double y, int btn);
    extern bool playingdemo();
    if(!playingdemo()) demo_event_fluxmouse(x, y, btn);
    else gui_active= true;
#endif

    mouse.x= x;
    mouse.y= y;
    mouse.btns= btn;
    int ix= mouse_x(), iy= mouse_y(); 	// rounded to int

    if( (oldmouse.x!=x || oldmouse.y!=y) && !resizing )
    {
        handler= find_mouse_handler(ix,iy, &h_abspos);

        if(oldhandler_id && (!handler || oldhandler_id!=handler->id))
        {
            rect oldhpos;
            window_list *oldh= wlist_find(windows, oldhandler_id);
            if(oldh)
            {
                prim_get_abspos(oldh->self, &oldhpos, true);
                oldh->self->callbacks.mouse(oldh->self->callbacks.mouse_arg,
                                            oldh->self, MOUSE_OUT,
                                            ix-oldhpos.x, iy-oldhpos.y, btn);
            }
        }

        if( handler )
        {
            if(oldhandler_id!=handler->id)
                event= MOUSE_IN;
            else
                event= MOUSE_OVER;
            handler->callbacks.mouse(handler->callbacks.mouse_arg,
                                     handler, event,
                                     ix-h_abspos.x, iy-h_abspos.y, btn);
        }

        oldhandler_id= (handler? handler->id: 0);
    }

    if(btn!=oldmouse.btns && !resizing)
    {
        if(btn>oldmouse.btns)
        {
            primitive *p= find_keybd_handler(ix, iy);
            if(p) setkbdhandler(p);
        }

        if( handler || (handler= find_mouse_handler(ix,iy, &h_abspos)) )
        {
            if(btn>oldmouse.btns) event= MOUSE_DOWN;
            else event= MOUSE_UP;

            handler->callbacks.mouse(handler->callbacks.mouse_arg,
                                     handler, event,
                                     ix-h_abspos.x, iy-h_abspos.y, btn);
        }
        else if(btn==MOUSE_BTN2) resizing= true;
//        else if(btn==MOUSE_BTN1) moving= true;
    }

    if(mouse.btns)
    {
        dword wnd= moving_window;
        if(wnd==NOWND)
            wnd= find_window_pos(ix, iy);
        if(wnd!=0 && wnd!=NOWND)
        {
            if(moving_window==NOWND) wnd_totop(wnd);

            else if( resizing &&
                     (mouse.x!=move_pos.x||mouse.y!=move_pos.y) &&	// mouse moved
                     !(wnd_getalign(wnd)&(WREL|HREL|XREL|YREL)) && 	// window has abs position
                     wnd_isresizable(wnd) )
            {
                int aln= wnd_getalign(wnd);
                int width= (aln&ALIGN_RIGHT? move_pos.x-ix: ix-move_pos.x) + wnd_getw(wnd);
                int height= (aln&ALIGN_BOTTOM? move_pos.y-iy: iy-move_pos.y) + wnd_geth(wnd);

                if(width<4) width= 4;
                if(height<4) height= 4;
                wnd_setsize(wnd, width, height);
            }

            else if( moving &&
                     (mouse.x!=move_pos.x||mouse.y!=move_pos.y) &&	// mouse moved
                     !(wnd_getalign(wnd)&(WREL|HREL|XREL|YREL)) )	// window has abs position
            {
                int aln= wnd_getalign(wnd);
                int x= ix-move_pos.x + wnd_getx(wnd);
                int y= iy-move_pos.y + wnd_gety(wnd);

                wnd_setpos(wnd, x, y);
			}

            move_pos.x= ix;
            move_pos.y= iy;
            moving_window= wnd;
        }
    }
    else
        moving_window= NOWND,
        resizing= moving= false;

    cur_mouse_handler= handler;
    oldmouse.x= x;
    oldmouse.y= y;
    oldmouse.btns= btn;
}


void flux_mouse_move_event(double xrel, double yrel)
{
    mouse.x+= xrel;
    mouse.y+= yrel;
    if(mouse.x<viewport.x) mouse.x= viewport.x;
    else if(mouse.x>viewport.rgt-1) mouse.x= viewport.rgt-1;
    if(mouse.y<viewport.y) mouse.y= viewport.y;
    else if(mouse.y>viewport.btm-1) mouse.y= viewport.btm-1;
    flux_mouse_event(mouse.x, mouse.y, mouse.btns);
}

void flux_mouse_button_event(int btn, bool down)
{
    if(down) mouse.btns|= 1<<btn;
    else mouse.btns&= ~(1<<btn);
    flux_mouse_event(mouse.x, mouse.y, mouse.btns);
}



void init_def_fonts()
{
    load_default_fonts();	// "dep" - noch überlegen wegen fonts allgemein
    /*
        font *f;

        f= font_load_raw("PSStock10x13-2.raw", 10, 13, false);
        //~ font_antialias(f, 128, 96);
        def_fonts[ prop_t(FONT_DEFAULT) ]= f;

        f= font_load_raw("PSStock10x13-2.raw", 10, 13, false);
        //~ font_antialias(f, 128, 96);
        def_fonts[ prop_t(FONT_ITEMS) ]= f;

        f= font_load_raw("PSStock10x13-2.raw", 10, 13, false);
        //~ font_antialias(f, 128, 96);
        def_fonts[ prop_t(FONT_CAPTION) ]= f;

        f= font_load_raw("smb8x8.raw", 8,8, true);
        def_fonts[ prop_t(FONT_SYMBOL) ]= f;

        f= font_load_raw("PSStock10x13-2.raw", 10,13, true);
        //~ font_antialias(f, 128, 96);
        def_fonts[ prop_t(FONT_FIXED) ]= f;
    */
}


void init_hashes()
{
    window_hash.construct();
    window_hash.add(0, windows);
    group_hash.construct();
    group_hash.add(1, groups);
}




void createtestwnd()
{
    dword wnd= create_rect(NOPARENT, 100,100, 0,0, COL_TITLE);

    dword frame= clone_frame("titleframe", wnd);
    wnd_setisize(wnd, 256,256);

    dword btn;

    for(int y= 0; y<8; y++)
    {
        for(int x= 0; x<8; x++)
        {
            btn= clone_group("button", wnd, (x*MAXSCALE)/8,(y*MAXSCALE)/8,
                             MAXSCALE/8,MAXSCALE/8, XREL|YREL|WREL|HREL);
        }
    }
}



void wndhash_delete_callback(int_hash<window_list> *hash, window_list *item)
{
    if(!(item->self->parent))
    {
        //~ logmsg("\tdestroying ID %4d\n", item->self->id);
//    dump_wlist(item, 0);
        wnd_destroy(item->self->id);
    }
}


void enum_groups()
{
    for(window_list *w= groups; w; w= w->next)
    {
        if(w->self->type==PT_GROUP)
        {
            prim_group *g= (prim_group *)w->self;
            logmsg("'%s'", g->name);
            if(g->props) logmsg(" properties: "), props_dump(g->props);
            else logmsg("\n");
        }
    }
}





static bool initialized;

void enum_groups();
void testdlg();

#ifdef FLUX_EDM
extern int ndrawtext;

void flux_tick()
{
    next_event();
    aq_exec();
    run_timers();

    glLoadIdentity();
    glOrtho(0, viewport.rgt-viewport.x, viewport.btm-viewport.y, 0, -1, 1);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glPointSize(1.0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    selectfont(font_small);

    extern bool depth_buffer_cleared;
    depth_buffer_cleared= false;

    redraw_rect(&viewport);
    draw_text_end();
    cliptext(0,0,0,0);
    update_rect(&viewport);
    if(gui_active) redraw_cursor();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    ndrawtext= 0;
}

#elif defined FLUX_NDS

void flux_tick()
{
    extern int last_z;

    last_z= 0;

    next_event();
    aq_exec();
    run_timers();
    glFlush(0);
}

#endif	// FLUX_EDM/FLUX_NDS

void setsyscolor(int index, dword value)
{
    int idx= (index & (~SYSCOL(0))) - 1;
    if(idx>=NSYSCOLORS) return;
    printf("idx: %d\n", idx);
    syscol_table[idx]= value;
}

bool flux_init()
{
    if(initialized) return true;

    windows= (window_list *)calloc(1, sizeof(window_list));
    backdrop= (prim_rect *)calloc(1, sizeof(prim_rect));
    backdrop->type= PT_RECT;
    backdrop->flags.visible= true;
    ((prim_rect*)backdrop)->color= COL_BAKGND;
    windows->self= backdrop;

    groups= (window_list *)calloc(1, sizeof(window_list));
    groups->self= (primitive *)calloc(1, sizeof(prim_rect));
    // @! Eigentlich sind in der Gruppenliste nur Gruppen, aber egal
    groups->self->type= PT_RECT;
    groups->self->id= 0xFFFFFFFF;

    dep_init(syscol_table, &backdrop, flux_mouse_event, flux_keyboard_event, clip_rect);

#if defined(FLUX_EDM)
    flux_setvideomode(scr_w, scr_h, 0);
#elif defined(FLUX_NDS)
    flux_setvideomode(256, 192, 0);
#endif

    init_hashes();

    load_cursor();

#ifdef FLUX_NDS
    init_nds_fonts();
#else
    init_def_fonts();
    init_defgroups();
//    enum_groups();
//    create_clock();
#endif

    initialized= true;
    return true;
}

bool flux_screenresize(int scr_w, int scr_h)
{
    if(!initialized && !flux_init()) return false;
    flux_setvideomode(scr_w, scr_h, 0);

    window_list *w= windows;
    while(w)
    {
        checkpos(w->self);
        w= w->next;
    }

    return true;
}

void flux_shutdown()
{
    if(!initialized) return;

    logmsg("flux: exiting\n");

    logmsg("destroying windows...\n");
    window_hash.enum_items( wndhash_delete_callback );
    logmsg("destroying groups...\n");
    group_hash.enum_items( wndhash_delete_callback );

    if(windows)
    {
        free(windows->self);
        free(windows);
        windows= 0;
    }
    if(groups)
    {
        free(groups->self);
        free(groups);
        groups= 0;
    }
    video_cleanup();

    window_hash.destroy();
    group_hash.destroy();

    initialized= false;
}


#ifdef STANDALONE_TEST

int main(int argc, char *argv[])
{
    flux_init();

    enum_groups();

    while(!quit)
    {
        next_event();
        aq_exec();
        run_timers();
    }

    flux_shutdown();
}

#endif

