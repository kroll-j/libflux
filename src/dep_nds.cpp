#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
//~ #include <memory.h>
#include <nds.h>

#define BUILD_DEP
#include "flux.h"


struct nds_texture
{
    int ID;		// glBindTexture
    int type;		// GL_TEXTURE_TYPE_ENUM
    void *pixels;	// pointer to pixel data in VRAM
    int pal_addr;	// palette offset in VRAM (if paletted)
    int width, height;
};

nds_texture alpha_tex;

struct nds_font
{
    byte *data;
    int width, height /* character height */;
    int widths[224];
    int refcount;
    union
    {
	byte privdata[32];
	struct
	{
	    nds_texture tex;
	    struct { u16 x; u16 y; } charpos[224];
	};
    };
};


struct video_info: public rect
{
};

struct viewport viewport;
struct video_info video;


static dword *syscol_table;
static struct primitive **backdrop;
static mouse_event_fn flux_mouse_event;
static keyboard_event_fn flux_keyboard_event;
static clip_rect_fn clip_rect;


// dep prim_redraw callback
// can return true if no additional drawing should be done.
bool dep_prim_redraw(primitive *p, rect *_abspos, rect *upd)
{
    return false;
    
    //~ if( !p->callbacks.paint &&
	//~ (p->type==PT_GROUP ||
        //~ (p->type==PT_RECT && (((prim_rect*)p)->color&INVISIBLE || ((prim_rect*)p)->color&TRANSL_NOPAINT))) )
    	//~ return true;
    
    //~ if(p->type==PT_TEXT)
	//~ draw_text_setup();
    //~ else
	//~ draw_text_end(),
    	//~ glDisable(GL_SCISSOR_TEST),
    	//~ glDisable(GL_TEXTURE_2D);
    
    //~ return false;
}



extern "C" 
void update_rect(const rect *r)
{
  //~ video.gl_buffer_dirty= true;
}

extern "C" 
int flux_setvideomode(int width, int height, int bpp, bool use_accel)
{
    video.x= video.y= viewport.x= viewport.y= (*backdrop)->x= (*backdrop)->y= 0;
    video.rgt= viewport.rgt= (*backdrop)->rgt= width;
    video.btm= viewport.btm= (*backdrop)->btm= height;
    
    
    return 1;
}



extern "C" 
bool bitmap_setpixels(dword id, flux_rgba *pixels, int swidth,int sheight, rect *rcs,
                      int xd, int yd)
{
  return true;
}


extern "C" 
void setpixel(int x, int y, byte r, byte g, byte b)
{
}

extern "C" 
void getpixel(int x, int y, int *r, int *g, int *b)
{
}

extern "C" 
dword map_color(dword c)
{
  dword col= (IS_SYSCOL(c)? syscol_table[c-1&31]: c);
  dword transl= ( col | c ) & TRANSL_MASK;
  
  return col | transl;
}

int last_z= 0;

extern bool do_texalpha;

extern "C" 
void fill_rect(rect *r, dword color)
{
    rect rc= { r->x,r->y, r->rgt,r->btm };

    if( (color&INVISIBLE) || (color&TRANSL_NOPAINT) ) return;
    if(!clip_rect(&rc, &viewport)) return;
  
    dword t= color&TRANSL_MASK;
    byte alpha= (t==TRANSL_3? 3: t==TRANSL_2? 2: t==TRANSL_1? 1: 4);
    
    glEnable(GL_BLEND);
    
    if(do_texalpha)
    {
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(0, alpha_tex.ID);
    	glTexCoord2t16((alpha<<5)-1, 0);
    }
    else
    {
	//~ glPolyFmt(POLY_ALPHA((alpha<<3)-1) | POLY_DECAL | POLY_CULL_NONE);
    	glPolyFmt(POLY_ALPHA((alpha<<3)-1) | POLY_CULL_NONE);
	glDisable(GL_TEXTURE_2D);
    }
    
    glColor3b( (color>>16)&255, (color>>8)&255, (color)&255 );
  
    glBegin(GL_QUADS);
    
    glVertex3v16(rc.x<<4,	rc.y<<4,	last_z);
    glVertex3v16(rc.rgt<<4,	rc.y<<4,	last_z);
    glVertex3v16(rc.rgt<<4,	rc.btm<<4,	last_z);
    glVertex3v16(rc.x<<4,	rc.btm<<4,	last_z);
    last_z++;	// hackish. how to turn the depth checking off??
  
    glEnd();
}

extern "C" 
void hline(int x1, int y, int x2, dword color)
{
  rect rc= { x1,y, x2,y+1 };
  fill_rect(&rc, color);
}


extern "C" 
void fill_scanlines(shape_scanline *scanlines, rect clip, rect abspos, dword color)
{
  int i, y1, y2, x1, x2;
  dword col= color; //map_color(color);
    
#if 0
  y1= max(0, clip.y-abspos.y);
  y2= min(abspos.btm-abspos.y, clip.btm-abspos.y);

  for(i= y1; i<y2; i++)
  {
    x1= scanlines[i].x1;
    x2= scanlines[i].x2;

    if(clip.x<<16 > x1) x1= clip.x<<16;
    if(clip.rgt<<16 < x2) x2= clip.rgt<<16;

    hline((x1>>16)+abspos.x, i+abspos.y, (x2>>16), col);
  }

#else

  y1= max(0, clip.y-abspos.y);
  y2= min(abspos.btm-abspos.y, clip.btm-abspos.y);

  for(i= y1; i<y2; i++)
  {
    x1= scanlines[i].x1 + (abspos.x<<16);
    x2= scanlines[i].x2 + (abspos.x<<16) + 65536;

    if(x2>x1)
    {
      if(x1 < clip.x<<16) x1= clip.x<<16;
      if(x2 > clip.rgt<<16) x2= clip.rgt<<16;

      hline(x1>>16, i+abspos.y, (x2>>16), col);
    }
  }
#endif
}

extern "C" 
void setpixeli(int x, int y, dword color, rect clip)
{
    return;	// disabled
}


extern "C" 
void plot_outline(pos *outline, int n, dword color, rect clip, pos p)
{
    return;	// disabled
}

extern "C" 
void blt_rgba(flux_rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
{
}


extern "C"
void load_cursor()
{
    // todo
}


extern "C" 
void paint_cursor(rect pos)
{
    //~ if(is_empty(&pos)) return;
    
    //~ int size= 32;
   
    //~ if(cur_mouse_handler)
    //~ {
	//~ if(mouse.btns) size= 28, pos.x+= 1, pos.y+= 1, glColor3b(192, 6, 22);
	//~ else glColor3b(255, 8, 32);
    //~ }
    //~ else glColor3b(255, 255, 255);
    
    //~ glColor3b(255, 255, 255);
    
    //~ glEnable(GL_TEXTURE_2D);
    //~ glBindTexture(GL_TEXTURE_2D, mousecursor->gl);
    
    //~ glBegin(GL_QUADS);
    //~ glTexCoord2i(0, 0);		glVertex2i(pos.x, pos.y);
    //~ glTexCoord2i(1, 0);		glVertex2i(pos.x+size, pos.y);
    //~ glTexCoord2i(1, 1);		glVertex2i(pos.x+size, pos.y+size);
    //~ glTexCoord2i(0, 1);		glVertex2i(pos.x, pos.y+size);
    //~ glEnd();
    
    //~ glDisable(GL_TEXTURE_2D);
}


extern "C" 
void screenshot(char *filename)
{
}


extern "C" 
void next_event()
{
}


extern "C" 
void set_mouse_pos(int x, int y)
{
    //SDL_WarpMouse(x, y);
}


extern "C" 
bool dep_init(dword *syscol_table, struct primitive **backdrop, mouse_event_fn mouse_event, keyboard_event_fn keyboard_event, clip_rect_fn clip_rect)
{
    ::syscol_table= syscol_table;
    ::backdrop= backdrop;
    ::flux_mouse_event= mouse_event;
    ::flux_keyboard_event= keyboard_event;
    ::clip_rect= clip_rect;
    
    glResetTextures();

    u8 teximg[8*8];
    u16 pal[32];
    
    for(int x= 0; x<8; x++)
	teximg[x]= 31 | (x<<5);
    for(int i= 0; i<32; i++)
	pal[i]= 0xFFFF; //(i)|(1<<5)|(i<<10)|(1<<15);
    
    glGenTextures(1, &alpha_tex.ID);
    glBindTexture(0, alpha_tex.ID);
    alpha_tex.width= alpha_tex.height= 8;
    alpha_tex.type= GL_RGB32_A3;
    glTexImage2D(0, 0, GL_RGB32_A3, TEXTURE_SIZE_8, TEXTURE_SIZE_8, 0, TEXGEN_TEXCOORD/*TEXGEN_OFF*/, teximg);
    alpha_tex.pal_addr= gluTexLoadPal(pal, 32, GL_RGBA);
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    return true;
}


extern "C" 
void video_cleanup()
{
}




font *font_load_raw(char *filename, int fw,int fh, bool fixed)
{
    // create dummy
    nds_font *f= (nds_font *)calloc(1, sizeof(nds_font));
    f->width= fw; f->height= fh;
    return (font*)f;
}


// Gibt die wirkliche Speicherposition von f zurueck;
// f selbst oder def_fonts[(int)f]
font *_font_getloc(font *f)
{
    if((prop_t)f<MAX_DEFAULTFONTS)
	return def_fonts[ prop_t(f) ];
    return f;
}


void font_ref(font *f)
{
  //~ log("font %X ref'd ", f);
  f= _font_getloc(f);
  f->refcount++;
  //~ log("(%d)\n", f->refcount);
}

void font_deref(font *&f)
{
  // font refcounts are not really used, fonts are managed by EDM
  font *freal= _font_getloc(f);
  freal->refcount--;
  //~ log("font %X deref'd (%d)\n", f, freal->refcount);
  if(freal->refcount<=0)
  {
    if(freal->refcount<0)
    {
      //~ log("font %X has refcount %d!\n", f, freal->refcount);
      return;
    }
    //~ log("freeing font %X (%X)\n", f, freal);
    font_free(freal);
  }
}


void font_free(font *f)
{
  f= _font_getloc(f);
  font_dep_free(f);
  free(f->data);
  f->width= f->height= 0;
}


int font_gettextwidth(font *font, char *text)
{
    // todo
    return 0; //text_width(text);
}

int font_gettextheight(font *font, char *text)
{
    int fh= font->height, h= fh;

    for(int i= 0; text[i]; i++) if(text[i]=='\n') h+= fh;

    return h;
}

int font_width(font *font)
{ return _font_getloc(font)->width; }

int font_height(font *font)
{ return _font_getloc(font)->height; }

void font_antialias(font *font, int f, int f2)
{
#define pix(x,y) data[(y)*w+(x)]
#define px2(x,y) data2[(y)*w+(x)]
  int i, x, y;
  byte *data= font->data;
  int w= font->width, h= font->height;
  byte *ndat= (byte *)malloc(w*h*224);
  byte *data2= ndat;
  memcpy(data2, data, w*h*224);

  font= _font_getloc(font);

  for(i= 0; i<224; i++)
  {
    for(y= 0; y<h; y++)
    {
      for(x= 0; x<w; x++)
      {
        if(!pix(x,y))
        {
          // links oben
          if(x>0&&y>0 && pix(x-1,y) && pix(x,y-1))
          {
            px2(x,y)= ( pix(x-1,y)+pix(x,y-1) ) * f >> 10;
            if(pix(x-2,y)) px2(x-1,y)= pix(x-1,y)-(px2(x-1,y)*f2>>9);
            if(pix(x,y-2)) px2(x,y-1)= pix(x,y-1)-(px2(x,y-1)*f2>>9);
          }

          // rechts oben
          else if(x<w-1&&y>0 && pix(x+1,y) && pix(x,y-1))
          {
            data2[y*w+x]= ( pix(x+1,y)+pix(x,y-1) ) * f >> 10;
            if(pix(x+2,y)) px2(x+1,y)= pix(x+1,y)-(px2(x+1,y)*f2>>9);
            if(px2(x,y-2)) px2(x,y-1)= pix(x,y-1)-(px2(x,y-1)*f2>>9);
          }

          // rechts unten
          else if(x<w-1&&y<h-1 && pix(x+1,y) && pix(x,y+1))
          {
            data2[y*w+x]= ( pix(x+1,y)+pix(x,y+1) ) * f >> 10;
            if(pix(x+2,y)) px2(x+1,y)= pix(x+1,y)-(px2(x+1,y)*f2>>9);
            if(pix(x,y+2)) px2(x,y+1)= pix(x,y+1)-(px2(x,y+1)*f2>>9);
          }

          // links unten
          else if(x>0&&y<h-1 && pix(x-1,y) && pix(x,y+1))
          {
            data2[y*w+x]= ( pix(x-1,y)+pix(x,y+1) ) * f >> 10;
            if(pix(x-2,y)) px2(x-1,y)= pix(x-1,y)-(px2(x-1,y)*f2>>9);
            if(pix(x,y+2)) px2(x,y+1)= pix(x,y+1)-(px2(x,y+1)*f2>>9);
          }
        }
      }
    }
    data+= font->width*font->height;
    data2+= font->width*font->height;
  }
  free(font->data);
  font->data= ndat;
#undef pix
#undef px2
  
  font_dep_update(font);
}

// Wird aufgerufen wenn sich die Pixeldaten von dem Font geändert haben
// GL-Textur hochladen
extern "C" 
void font_dep_update(font *_font)
{
}


// System-abhängige Daten (GL Textur) freigeben
extern "C" 
void font_dep_free(font *font)
{
  // ...
}

extern "C" 
void put_alpha_map(dword color, byte *bmap, int bw,int bh, rect srcpos, int xdest,int ydest)
{
}


#include "ndsfont_pcx_lz77.h"
#include "nds-fonttab.h"


nds_font *load_nds_font(bool fixed)
{
    static u8 *ndsfont_pcx;
    static sImage ndsfont_img;
    static nds_font font;
    int i;
    if(!ndsfont_pcx)
    {
	ndsfont_pcx= (u8*)malloc(16*1024);
	swiDecompressLZSSWram((void*)ndsfont_pcx_lz77, ndsfont_pcx);
	loadPCX(ndsfont_pcx, &ndsfont_img);
	
	u8 *p= ndsfont_img.image.data8;
	for(i= ndsfont_img.width*ndsfont_img.height-1; i>=0; i--)
	    p[i]|= 7;	// 'convert' to GL_RGB8_A5 texture format (3 bits color index, 5 bits alpha)
	for(i= 0; i<8; i++)
	    ndsfont_img.palette[i]= 0xFFFF;
	
	glGenTextures(1, &font.tex.ID);
	glBindTexture(0, font.tex.ID);
	font.tex.width= ndsfont_img.width;
	font.tex.height= ndsfont_img.height;
	font.tex.type= GL_RGB8_A5;
	glTexImage2D(0, 0, GL_RGB8_A5, TEXTURE_SIZE_128, TEXTURE_SIZE_256 /*tmp*/, 
		     0, TEXGEN_TEXCOORD/*TEXGEN_OFF*/, ndsfont_img.image.data8);
	font.tex.pal_addr= gluTexLoadPal(ndsfont_img.palette, 8, GL_RGBA);
    
	struct fonttab { u16 x1, y1, x2, y2; };
	fonttab *tab= (fonttab*)fontcoords;
	int tabsize= sizeof(fontcoords)/sizeof(fonttab);
	
	for(i= 0; i<tabsize; i++)
	{
	    font.charpos[i].x= tab[i].x1;
	    font.charpos[i].y= tab[i].y1;
	    font.widths[i]= (fixed? font.width: tab[i].x2-tab[i].x1-1);
	}
	font.width= font.widths['x'-33];
	font.height= 12;
    }
    
    nds_font *ret= (nds_font *)calloc(sizeof(nds_font), 1);
    *ret= font;
    return ret;
}



extern "C"
void init_nds_fonts()
{
    def_fonts[ prop_t(FONT_DEFAULT) ]= (font*)load_nds_font(false);
    def_fonts[ prop_t(FONT_ITEMS) ]= (font*)load_nds_font(false);
    def_fonts[ prop_t(FONT_CAPTION) ]= (font*)load_nds_font(false);
    def_fonts[ prop_t(FONT_SYMBOL) ]= (font*)load_nds_font(false);
    def_fonts[ prop_t(FONT_FIXED) ]= (font*)load_nds_font(true);
}



// ------------------------------------- text drawing -------------------------------------



void draw_char(int x,int y, int fx,int fy, int w,int h)
{
    glBegin(GL_QUADS);
    glTexCoord2t16(fx,		fy);	glVertex3v16(x,		y,	last_z);
    glTexCoord2t16(fx+w,	fy);	glVertex3v16(x+w,	y,	last_z);
    glTexCoord2t16(fx+w,	fy+h);	glVertex3v16(x+w,	y+h,	last_z);
    glTexCoord2t16(fx, 		fy+h);	glVertex3v16(x,		y+h,	last_z);
    glEnd();
    
    last_z++;
}



extern "C" 
void draw_text(font *font, char *text, int sx,int sy, rect clp, dword col)
{
    if(!clip_rect(&clp, &viewport)) return;
    
    nds_font *f= (nds_font*)font;
    
    glEnable(GL_BLEND);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(0, f->tex.ID);
    
    dword c= (IS_SYSCOL(col)? syscol_table[col-1&31]: col);
    glColor3b( (c>>16)&0xFF, (c>>8)&0xFF, (c)&0xFF );
    
    int i;
    int x= sx, y= sy;
    for(i= 0; text[i]; i++)
    {
	u8 c= text[i];
	if(c==' ')
	    x+= font->widths['x'-33];
	else if(c=='\n')
	    y+= font->height,
	    x= sx;
	else
	{
	    c-= 33;
	    int fx= f->charpos[c].x, fy= f->charpos[c].y;
	    int w= f->widths[c], h= f->height+1;
	    
	    rect r= { x, y, x+w, y+h };
	    if(clip_rect(&r, &clp))
	    {
	    	draw_char( r.x<<4,r.y<<4, fx+(r.x-x)<<4,fy+(r.y-y)<<4, r.rgt-r.x<<4,r.btm-r.y<<4 );
	    }
	    x+= font->widths[c];
	}
    }
}



















