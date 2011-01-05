#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#define BUILD_DEP
#include "flux.h"
#include "flux-edm.h"


// whether depth buffer has been cleared this frame. used by edm_md2model
bool depth_buffer_cleared= false;


/* 8-Bit Graustufen Cursor (im moment nicht benutzt) */
/*
#define _ 0,
#define O 64,
#define I 128,
#define U 192,
#define H 255,
byte cursor[]= {
 O O _ _ _ _ _ _ _ _ _ _ _ _ _ _
 O I O O _ _ _ _ _ _ _ _ _ _ _ _
 _ O U I O O _ _ _ _ _ _ _ _ _ _
 _ O I H U I O O _ _ _ _ _ _ _ _
 _ _ O U H H U I O O _ _ _ _ _ _
 _ _ O I H H H H U I O O _ _ _ _
 _ _ _ O U H H H H H U I O _ _ _
 _ _ _ O I H H H H U I O _ _ _ _
 _ _ _ _ O U H H H I O _ _ _ _ _
 _ _ _ _ O I H U I H I O _ _ _ _
 _ _ _ _ _ O U I O I H I O _ _ _
 _ _ _ _ _ O I O _ O I H I O _ _
 _ _ _ _ _ _ O _ _ _ O I H I O _
 _ _ _ _ _ _ _ _ _ _ _ O I H I O
 _ _ _ _ _ _ _ _ _ _ _ _ O I O _
 _ _ _ _ _ _ _ _ _ _ _ _ _ O _ _
 };
#undef _
#undef O
#undef I
#undef U
#undef H
*/
 
struct edm_font
{
  byte *data;
  int width, height;
  int widths[224];
  int refcount;
  union
  {
    byte privdata[32];
    struct
    {
      int gl_texnum;
      int gl_texwidth, gl_texheight;
    };
  };
};

static int last_gl_texnum;	// hoechste benutzte GL-Texture-Nummer

static Texture *mousecursor;

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
    if( !p->callbacks.paint &&
	(p->type==PT_GROUP ||
        (p->type==PT_RECT && (((prim_rect*)p)->color&INVISIBLE || ((prim_rect*)p)->color&TRANSL_NOPAINT))) )
    	return true;
    
    if(p->type==PT_TEXT)
	draw_text_setup();
    else
	draw_text_end(),
    	glDisable(GL_SCISSOR_TEST),
    	glDisable(GL_TEXTURE_2D);
    
    return false;
}


// ---------------- FONT STUFF -------------------


// leftover, currently not used in this implementation
void draw_char(font *_font, byte chr, int x,int y, rect clp, dword color)
{
    return;
    
    
    if(chr<32) return;
  edm_font *font= (edm_font *)_font;
  rect rc= { x,y, x+font->widths[chr-32],y+font->height };
  

  if(clip_rect(&rc, &clp))
  {
    dword col= (IS_SYSCOL(color)? syscol_table[color-1&31]: color);
    glColor3ub( (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF );

    int char_x= (chr-32)%15 * font->width,
        char_y= (chr-32)/15 * font->height;
    long double tx1= double(char_x + rc.x-x) / font->gl_texwidth,
    		tx2= double(char_x + rc.rgt-x) / font->gl_texwidth,
    		ty1= double(char_y + rc.y-y) / font->gl_texheight,
    		ty2= double(char_y + rc.btm-y) / font->gl_texheight;

    //~ if(video.glBegin_called)
	glEnd();

    //~ glBindTexture(GL_TEXTURE_2D, font->gl_texnum);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    
    glTexCoord2f(tx1, ty1); 	glVertex2i(rc.x, rc.y);
    glTexCoord2f(tx2, ty1); 	glVertex2i(rc.rgt, rc.y);
    glTexCoord2f(tx2, ty2); 	glVertex2i(rc.rgt, rc.btm);
    glTexCoord2f(tx1, ty2); 	glVertex2i(rc.x, rc.btm);
    
    glEnd();
    //~ video.glBegin_called= false;
    glDisable(GL_TEXTURE_2D);
  }
}

int ndrawtext;

extern "C" 
void draw_text(font *font, char *text, int sx,int sy, rect clp, dword col)
{
    if(!clip_rect(&clp, &viewport)) return;
    
    dword c= (IS_SYSCOL(col)? syscol_table[col-1&31]: col);
    glColor3ub( (c>>16)&0xFF, (c>>8)&0xFF, (c)&0xFF );
    
    //~ glEnable(GL_TEXTURE_2D);
    
    //~ if(viewport.rgt==scr_w && viewport.btm==scr_h)
    	//~ glScissor(clp.x,viewport.btm-clp.btm, clp.rgt-clp.x,clp.btm-clp.y);
    //~ else
    	//~ glScissor(clp.x*scr_w/viewport.rgt+1, (viewport.btm-clp.btm)*scr_h/viewport.btm+1, 
    	      	  //~ (clp.rgt-clp.x)*scr_w/viewport.rgt+1, (clp.btm-clp.y)*scr_h/viewport.btm+1);
    
    //~ glEnable(GL_SCISSOR_TEST);
    
    cliptext(clp.x, clp.y, clp.rgt, clp.btm);
    draw_text_ns(text, sx, sy);
    //~ draw_text(text, sx, sy);
    
    //~ glDisable(GL_SCISSOR_TEST);
    //~ glDisable(GL_TEXTURE_2D);
    
    ndrawtext++;
}


static int get_min_powerof2(int n)
{
  int i= 1;
  while(i<n) i<<= 1;
  return i;
}


// Wird aufgerufen wenn sich die Pixeldaten von dem Font geändert haben
// GL-Textur hochladen
extern "C" 
void font_dep_update(font *_font)
{
  edm_font *font= (edm_font *)_font;
  int texwidth= get_min_powerof2(font->width*15);
  int texheight= get_min_powerof2(font->height*15);
  byte *texdata= (byte *)malloc(texwidth*texheight);
  
  logmsg("GL: Uploading %dx%d font texture...\n", texwidth, texheight);
  
  // Font chars in brauchbare GL-Textur packen:
  // 15 x 15 chars in quadratischer Textur
  int c, x, y;
  byte *s= font->data, *d;
  for(c= 0; c<224; c++)
  {
    d= texdata + (c/15)*font->height*texwidth + (c%15)*font->width;
    for(y= font->height-1; y>=0; y--)
    {
      for(x= font->width-1; x>=0; x--)
      {
	d[x]= s[x];
      }
      s+= font->width;
      d+= texwidth;
    }
  }
  
  //~ glBindTexture(GL_TEXTURE_2D, ++last_gl_texnum); 
  //~ if(glGetError()) { log("couldn't bind texture %d\n", last_gl_texnum); return; }
  //~ glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texwidth,texheight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, texdata);
  //~ if(glGetError()) { log("gl_teximage failed\n"); return; }
  //~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);	//_TO_EDGE
  //~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  //~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //~ glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  //~ glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
  
  font->gl_texnum= last_gl_texnum;
  font->gl_texwidth= texwidth;
  font->gl_texheight= texheight;
  
  free(texdata);
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

/*
  //~ switch(video.bpp)
  //~ switch(video.sdl_front_surface->format->BitsPerPixel)
  switch(32)	// ***************************** FIXME *********************************
  {
    case 16:
      return (col>>19<<11 | (col&(63<<10))>>5 | (col&0xF7)>>3) | transl;

    case 24:
    case 32:
      return col | transl;
  }
*/
}

extern "C" 
void fill_rect(rect *r, dword color)
{
    int x, y;
    rect rc= { r->x,r->y, r->rgt,r->btm };

    if( (color&INVISIBLE) || (color&TRANSL_NOPAINT) ) return;
    if(!clip_rect(&rc, &viewport)) return;
  
    dword t= color&TRANSL_MASK;
    byte alpha= t? (t==TRANSL_3? 192: (t>>25)*64): 255;
    glColor4ub( (color>>16)&255, (color>>8)&255, (color)&255, alpha );
  
    //~ if(!video.glBegin_called)
    {
	glBegin(GL_QUADS);
    	//~ video.glBegin_called= true;
    }
  
    glVertex2i(rc.x, rc.y);
    glVertex2i(rc.rgt, rc.y);
    glVertex2i(rc.rgt, rc.btm);
    glVertex2i(rc.x, rc.btm);
  
    glEnd();
}

extern "C" 
void hline(int x1, int y, int x2, dword color)
{
  rect rc= { x1,y, x2,y+1 };
  fill_rect(&rc, color);	// inefficient..
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

const double fixed2float= 1.0L / 65536;

static void setpixeli_plain(int x, int y, byte colv[4])
{
    int xabs= x>>16, yabs= y>>16;
    
    int f, fx= x&0xFFFF, fy= y&0xFFFF;
    
    f= dword((0xFFFF-fx)*(0xFFFF-fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs, yabs);
    
    f= dword((fx)*(0xFFFF-fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs+1, yabs);
    
    f= dword((0xFFFF-fx)*(fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs, yabs+1);
    
    f= dword((fx)*(fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs+1, yabs+1);
}


extern "C" 
void setpixeli(int x, int y, dword color, rect clip)
{
    return;	// disabled
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip.x,viewport.btm-clip.btm, clip.rgt-clip.x,clip.btm-clip.y);
    glBegin(GL_POINTS);
    setpixeli_plain(x, y, (byte*)&color);
    glEnd();
    glDisable(GL_SCISSOR_TEST);
}

extern "C" 
void plot_outline(pos *outline, int n, dword color, rect clip, pos p)
{
    return;	// disabled
    dword col= (IS_SYSCOL(color)? syscol_table[(color-1)&31]: color);
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip.x,viewport.btm-clip.btm, clip.rgt-clip.x,clip.btm-clip.y);
    //~ glColor3ub( (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF );
    glBegin(GL_POINTS);
    byte colv[]= { (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF, 0 };
    for(int i= n-1; i>=0; i--)
    {
	int x= outline[i].x+p.x, y= outline[i].y+p.y;
	setpixeli_plain(x, y, colv);
    }
    glEnd();
    glDisable(GL_SCISSOR_TEST);
}


extern "C" 
void blt_rgba(flux_rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
{
}


extern "C"
void load_cursor()
{
    mousecursor= textureload("data/edm/cursor1.png", 0, 0, false);
}


extern "C" 
void paint_cursor(rect pos)
{
    if(is_empty(&pos)) return;
    
    int size= 32;
   
    if(cur_mouse_handler)
    {
	if(mouse.btns) size= 28, pos.x+= 1, pos.y+= 1, glColor3ub(192, 6, 22);
	else glColor3ub(255, 8, 32);
    }
    else glColor3ub(255, 255, 255);
    
    glColor3ub(255, 255, 255);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mousecursor->gl);
    
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0);		glVertex2i(pos.x, pos.y);
    glTexCoord2i(1, 0);		glVertex2i(pos.x+size, pos.y);
    glTexCoord2i(1, 1);		glVertex2i(pos.x+size, pos.y+size);
    glTexCoord2i(0, 1);		glVertex2i(pos.x, pos.y+size);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
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
    SDL_WarpMouse(x, y);
}


extern "C" 
bool dep_init(dword *syscol_table, struct primitive **backdrop, mouse_event_fn mouse_event, keyboard_event_fn keyboard_event, clip_rect_fn clip_rect)
{
  ::syscol_table= syscol_table;
  ::backdrop= backdrop;
  ::flux_mouse_event= mouse_event;
  ::flux_keyboard_event= keyboard_event;
  ::clip_rect= clip_rect;

  return true;
}


extern "C" 
void video_cleanup()
{
}






// Font-Sachen

font *font_load_raw(char *filename, int fw,int fh, bool fixed)
{
    // create dummy
    edm_font *f= (edm_font*)calloc(1, sizeof(edm_font));
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
    return text_width(text);
}

int font_gettextheight(font *font, char *text)
{
    int fh= getfontheight(), h= fh;

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





/* altes font-zeug
font *font_load_raw(char *filename, int fw,int fh, bool fixed= false)
{
  int i;
  FILE *f= fopen(filename, "rb");
  if(!f) return 0;

  font *ret= (font *)calloc(1, sizeof(font));

  ret->width= fw;
  ret->height= fh;
  ret->data= (byte *)calloc(1, fw*fh*224);

  fread(ret->data, 1, fw*fh*224, f);
  fclose(f);

  if(fixed)
  {
    for(i= 0; i<224; i++)
      ret->widths[i]= fw;
  }
  else
  {
    int x, y, cwidth;
    byte *chr= ret->data+fw*fh;
    for(i= 1; i<224; i++)
    {
      cwidth= 0;
      for(x= 0; x<fw; x++)
      {
        for(y= 0; y<fh; y++)
        {
          if(chr[y*fw+x]>128)
            cwidth= x;
        }
      }
      ret->widths[i]= (cwidth+2<fw? cwidth+2: fw);
      chr+= fw*fh;
    }
    ret->widths[0]= ret->widths['x'-32];
  }
  
  font_dep_update(ret);

  return ret;
}


// Gibt die wirkliche Speicherposition von f zurueck;
// f selbst oder def_fonts[(int)f]
font *_font_getloc(font *f)
{
  if((unsigned int)f<MAX_DEFAULTFONTS)
    return def_fonts[ dword(f) ];
  return f;
}


void font_ref(font *f)
{
  log("font %X ref'd ", f);
  f= _font_getloc(f);
  f->refcount++;
  log("(%d)\n", f->refcount);
}

void font_deref(font *&f)
{
  font *freal= _font_getloc(f);
  freal->refcount--;
  log("font %X deref'd (%d)\n", f, freal->refcount);
  if(freal->refcount<=0)
  {
    if(freal->refcount<0)
    {
      log("font %X has refcount %d!\n", f, freal->refcount);
      return;
    }
    log("freeing font %X (%X)\n", f, freal);
    font_free(freal);
//    free(freal);
//    f= 0;
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
  int i;
  int ret= 0, linewidth= 0;

  font= _font_getloc(font);

  for(i= 0; text[i]; i++)
  {
    if(text[i]=='\n')
    {
      if(linewidth>ret) ret= linewidth;
      linewidth= 0;
    }
    else if( byte(text[i]) >= 32 )
      linewidth+= font->widths[text[i]-32];
    else
      linewidth+= font->width;
  }

  return max(linewidth, ret);
}

int font_gettextheight(font *font, char *text)
{
  int i;
  int ret;

  font= _font_getloc(font);

  ret= font->height;

  for(i= 0; text[i]; i++)
    if(text[i]=='\n') ret+= font->height;

  return ret;
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
*/
