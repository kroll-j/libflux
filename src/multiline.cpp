#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include "flux.h"
#include "sys.h"
#include "prop_list.h"

#ifdef FLUX_EDM
#include "pch.h"
#include "engine.h"
#endif

#define BUFPOW	16



struct multiline_privdata
{
    int textlen;
    int bufsize;
    char *buf;
};


static void realloc_buf(multiline_privdata *pd)
{
    int newlen= ((pd->textlen>>BUFPOW)+1) << BUFPOW;
    if(newlen!=pd->bufsize) pd->buf= (char*)realloc(pd->buf, newlen);
}

static prop_t ml_props(prop_t arg, primitive *self, int type, const char *name, int propid, prop_t value)
{
    multiline_privdata *pd= (multiline_privdata*)self->privdata;

    switch(propid)
    {
	case 0:	// text
	{
	    if(type==PROP_GET)
		return (prop_t)pd->buf;
	    else
	    {
		pd->textlen= strlen((char*)value);
		realloc_buf(pd);
		strcpy(pd->buf, (char*)value);
		return (prop_t)pd->buf;
	    }
	}
    }
    return 0;
}

static void ml_keybd(prop_t arg, primitive *self, int isdown, int code, int chr)
{
}

static int ml_status(prop_t arg, primitive *self, int type)
{
    if(type==STAT_CREATE)
    {
	multiline_privdata *pd= (multiline_privdata*)calloc(1, sizeof(multiline_privdata));
	pd->buf= strdup( "" );
	self->privdata= (void*)pd;
    }
    else if(type==STAT_DESTROY)
    {
	multiline_privdata *pd= (multiline_privdata*)self->privdata;
	if(pd->buf) free(pd->buf);
    }
    return 0;
}

static void ml_paint(prop_t arg, primitive *self, rect *abspos, const rectlist *dirty_list)
{
#ifdef FLUX_EDM
#error FIXME: convert update rect to rectlist
    int w= abspos->rgt - abspos->x, h= abspos->btm - abspos->y;
    multiline_privdata *pd= (multiline_privdata*)self->privdata;
    rect *c= upd? upd: abspos;
    int fonth= font_height(FONT_DEFAULT);

    cliptext(c->x, c->y, c->rgt, c->btm);

    fill_rect(c, 0x000000|TRANSL_2);

    glColor3f(1,1,1);
    draw_text_setup();

    int linestart= 0;
    int y= abspos->y;
    while(linestart < pd->textlen)
    {
	int lastfit= linestart, nextw= linestart;

	do {
	    lastfit= nextw;
	    nextw= nextword(pd->buf, nextw);
	    for(int i= lastfit; i<=nextw; i++) if(pd->buf[i]=='\n') { nextw= i; break; }
	    while( nextw>lastfit+1 && isspace(pd->buf[nextw-1]) ) nextw--;
	} while( nextw>linestart && pd->buf[nextw] && pd->buf[lastfit]!='\n' &&
		 text_width(pd->buf+linestart, nextw-linestart)<w );

	if(pd->buf[linestart]==' ') linestart++;

	//~ conoutf("%d %d %d", text_width(pd->buf+linestart, lastfit-linestart), self->x, self->rgt);

	if(lastfit<=linestart) lastfit= linestart+1;
	else
	{
	    char str[lastfit-linestart+2];
	    strn0cpy(str, pd->buf+linestart, lastfit-linestart+1);
	    draw_text_ns(str, abspos->x, y);
	    y+= fonth;
	}

	linestart= lastfit;
    }

    draw_text_end();
#else
    printf("multiline painting not yet implemented correctly for this platform\n");
#endif
}


void create_multiline()
{
    dword grp= create_group("multiline");
    wnd_set_props_callback(grp, ml_props, 0);
    wnd_set_status_callback(grp, ml_status, 0);
    wnd_set_kbd_callback(grp, ml_keybd, 0);
    wnd_set_paint_callback(grp, ml_paint, 0);
    wnd_addprop(grp, "text", (prop_t)"", PROP_STRING);
}











