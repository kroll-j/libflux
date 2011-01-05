#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include <GL/glx.h>
#include <X11/extensions/xf86vmode.h>
#define BUILD_DEP
#include "flux.h"
#include "plugin.h"



DEFINE_PLUGIN(GL)
  PLUGIN_FUNCTION(void, glBegin) (int)
  PLUGIN_FUNCTION(void, glEnd) (void)
  PLUGIN_FUNCTION(void GLAPIENTRY, glFinish) ( void )
  PLUGIN_FUNCTION(void GLAPIENTRY, glFlush) ( void )
  PLUGIN_FUNCTION(void GLAPIENTRY, glColor3ub) ( GLubyte red, GLubyte green, GLubyte blue )
  PLUGIN_FUNCTION(void GLAPIENTRY, glBindTexture) ( GLenum target, GLuint texture )
  PLUGIN_FUNCTION(void GLAPIENTRY, glEnable) ( GLenum cap )
  PLUGIN_FUNCTION(void GLAPIENTRY, glTexCoord2f) ( GLfloat s, GLfloat t )
  PLUGIN_FUNCTION(void GLAPIENTRY, glVertex2i) ( GLint x, GLint y )
  PLUGIN_FUNCTION(void GLAPIENTRY, glDisable) ( GLenum cap )
  PLUGIN_FUNCTION(GLenum GLAPIENTRY, glGetError) ( void )
  PLUGIN_FUNCTION(void GLAPIENTRY, glTexImage2D) ( GLenum target, GLint level,
                                    GLint internalFormat,
                                    GLsizei width, GLsizei height,
                                    GLint border, GLenum format, GLenum type,
                                    const GLvoid *pixels )
  PLUGIN_FUNCTION(void GLAPIENTRY, glTexParameterf) ( GLenum target, GLenum pname, GLfloat param )
  PLUGIN_FUNCTION(void GLAPIENTRY, glTexEnvf) ( GLenum target, GLenum pname, GLfloat param )
  PLUGIN_FUNCTION(GLXContext, glXCreateContext) (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct)
  PLUGIN_FUNCTION(Bool, glXMakeCurrent) (Display *dpy, GLXDrawable drawable, GLXContext ctx)
  PLUGIN_FUNCTION(void GLAPIENTRY, glOrtho) ( GLdouble left, GLdouble right,
                                 GLdouble bottom, GLdouble top,
                                 GLdouble near_val, GLdouble far_val )
  PLUGIN_FUNCTION(void GLAPIENTRY, glBlendFunc) ( GLenum sfactor, GLenum dfactor )
  PLUGIN_FUNCTION(void, glXSwapBuffers) (Display *dpy, GLXDrawable drawable)
  PLUGIN_FUNCTION(XVisualInfo*, glXChooseVisual) (Display *dpy, int screen, int *attribList)
  PLUGIN_FUNCTION(void GLAPIENTRY, glColor4ub) ( GLubyte red, GLubyte green,
                                    GLubyte blue, GLubyte alpha )
END_PLUGIN(GL)



// MERKEN:
// Bei Fehler
// "cannot handle TLS data":
// Verzeichnis /usr/lib/tls löschen oder umbenennen.





/* 8-Bit Graustufen Cursor */
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


struct gl_font
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

struct video_info: public rect
{
  Display *X_display;
  int X_scrnum;
  Window X_rootwnd;
  Window X_wnd;
  GLXContext glx_context;
  bool glBegin_called: 1,
       gl_buffer_dirty: 1,
       gl_doublebuf: 1;
  XF86VidModeModeInfo X_old_mode;
};


struct viewport viewport;
struct video_info video;


static dword *syscol_table;
static struct primitive **background;
static mouse_event_fn mouse_event;
static keyboard_event_fn keyboard_event;
static clip_rect_fn clip_rect;


// ---------------- FONT STUFF -------------------


void draw_char(font *_font, byte chr, int x,int y, rect clp, dword color)
{
  if(chr<32) return;
  gl_font *font= (gl_font *)_font;
  rect rc= { x,y, x+font->widths[chr-32],y+font->height };
  

  if(clip_rect(&rc, &clp))
  {
    dword col= (IS_SYSCOL(color)? syscol_table[color-1&31]: color);
    pglColor3ub( (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF );

    int char_x= (chr-32)%15 * font->width,
        char_y= (chr-32)/15 * font->height;
    long double tx1= double(char_x + rc.x-x) / font->gl_texwidth,
    		tx2= double(char_x + rc.rgt-x) / font->gl_texwidth,
    		ty1= double(char_y + rc.y-y) / font->gl_texheight,
    		ty2= double(char_y + rc.btm-y) / font->gl_texheight;

    if(video.glBegin_called) pglEnd();

    pglBindTexture(GL_TEXTURE_2D, font->gl_texnum);
    pglEnable(GL_TEXTURE_2D);
    pglBegin(GL_QUADS);
    
    pglTexCoord2f(tx1, ty1); 	pglVertex2i(rc.x, rc.y);
    pglTexCoord2f(tx2, ty1); 	pglVertex2i(rc.rgt, rc.y);
    pglTexCoord2f(tx2, ty2); 	pglVertex2i(rc.rgt, rc.btm);
    pglTexCoord2f(tx1, ty2); 	pglVertex2i(rc.x, rc.btm);
    
    pglEnd();
    video.glBegin_called= false;
    pglDisable(GL_TEXTURE_2D);
  }
}

extern "C" 
void draw_text(font *font, char *text, int sx,int sy, rect clp, dword col)
{
  rect rc;
  int i;
  int x= sx, y= sy;

  if(!clip_rect(&clp, &viewport))
    return;

  for(i= 0; text[i]; i++)
  {
    if(text[i]<32) switch(text[i])
    {
      case '\n':
        y+= font->height;
        x= sx;
    }
    else
    {
      draw_char(font, text[i], x,y, clp, col);
      x+= font->widths[text[i]-32];
    }
  }
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
  gl_font *font= (gl_font *)_font;
  int texwidth= get_min_powerof2(font->width*15);
  int texheight= get_min_powerof2(font->height*15);
  byte *texdata= (byte *)malloc(texwidth*texheight);
  
  printf("GL: Uploading %dx%d font texture...\n", texwidth, texheight);
  
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
  
  pglBindTexture(GL_TEXTURE_2D, ++last_gl_texnum); 
  if(pglGetError()) { printf("couldn't bind texture %d\n", last_gl_texnum); return; }
  pglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texwidth,texheight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, texdata);
  if(pglGetError()) { printf("gl_teximage failed\n"); return; }
  pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);	//_TO_EDGE
  pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  pglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
  
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


// DEP
extern "C" 
void put_alpha_map(dword color, byte *bmap, int bw,int bh, rect srcpos, int xdest,int ydest)
{
/*
  int xs, xd, y;
  byte *src= bmap+srcpos.y*bw+srcpos.x;
  byte *dst= (byte *)viewport.data + ((ydest*(viewport.rgt-viewport.x)+xdest)*video.bpp>>3);
  if(IS_SYSCOL(color)) color= syscol_table[color-1&31];
  int r1= color>>16, g1= (color>>8)&0xFF, b1= color&0xFF;  // Vordergrundfarbe
  int r2, g2, b2;                                          // Hintergrundfarbe
  int c;

  switch(video.bpp)
  {
    case 32:
    {
      for(y= srcpos.btm-srcpos.y; y; y--)
      {
        for(xs= srcpos.rgt-srcpos.x-1, xd= xs*4; xs>=0; xs--, xd-= 4)
        {
          if( (c= src[xs]) )
          {
            b2= dst[xd+0];
            g2= dst[xd+1];
            r2= dst[xd+2];
            b2= b2 + ((b1-b2)*c>>8);
            g2= g2 + ((g1-g2)*c>>8);
            r2= r2 + ((r1-r2)*c>>8);
            *(dword*)(dst+xd)= b2|(g2<<8)|(r2<<16);
          }
        }
        src+= bw;
        dst+= (viewport.rgt-viewport.x)*4;
      }
      break;
    }

    case 24:
    {
      for(y= srcpos.btm-srcpos.y; y; y--)
      {
        for(xs= srcpos.rgt-srcpos.x-1, xd= xs*3; xs>=0; xs--, xd-= 3)
        {
          if( (c= src[xs]) )
          {
            b2= dst[xd+0];
            g2= dst[xd+1];
            r2= dst[xd+2];
            b2= b2 + ((b1-b2)*c>>8);
            g2= g2 + ((g1-g2)*c>>8);
            r2= r2 + ((r1-r2)*c>>8);
            *(word*)(dst+xd)= b2|(g2<<8);
            dst[xd+2]= r2;
          }
        }
        src+= bw;
        dst+= (viewport.rgt-viewport.x)*3;
      }
      break;
    }

    case 16:
    {
      r1>>= 3;
      g1>>= 2;
      b1>>= 3;
      for(y= srcpos.btm-srcpos.y; y; y--)
      {
        for(xs= srcpos.rgt-srcpos.x-1, xd= xs*2; xs>=0; xs--, xd-= 2)
        {
          if( (c= src[xs]) )
          {

            b2= dst[xd]&31;
            g2= (*(word*)(dst+xd)>>5) & 63;
            r2= dst[xd+1]>>3;
            b2= b2 + ((b1-b2)*c>>8);
            g2= g2 + ((g1-g2)*c>>8);
            r2= r2 + ((r1-r2)*c>>8);
            *(word*)(dst+xd)= b2 | (g2<<5) | (r2<<11);
          }
        }
        src+= bw;
        dst+= (viewport.rgt-viewport.x)*2;
      }
      break;
    }
  }
*/
}

// DEP
extern "C" 
void update_rect(const rect *r)
{
  video.gl_buffer_dirty= true;
}



static bool set_X_vid_mode(int width, int height, int *realwidth, int *realheight)
{
  int best_fit, best_dist, dist, x, y;
  XF86VidModeModeInfo **vidmodes;
  int num_vidmodes;
  
  XF86VidModeGetAllModeLines(video.X_display, video.X_scrnum, &num_vidmodes, &vidmodes);
  if(!video.X_old_mode.hdisplay) video.X_old_mode= *vidmodes[0];
  
  best_dist= 10000000;
  best_fit= -1;
  
  for(int i= 0; i<num_vidmodes; i++)
  {
    if(width > vidmodes[i]->hdisplay ||
      height > vidmodes[i]->vdisplay)
     continue;

    x= width - vidmodes[i]->hdisplay;
    y= height - vidmodes[i]->vdisplay;
    dist= (x * x) + (y * y);
    if (dist < best_dist)
    {
      best_dist = dist;
      best_fit = i;
    }
  }

  if(best_fit != -1)
  {
    *realwidth= vidmodes[best_fit]->hdisplay;
    *realheight= vidmodes[best_fit]->vdisplay;

    // change to the mode
    XF86VidModeSwitchToMode(video.X_display, video.X_scrnum, vidmodes[best_fit]);

    //~ // @! Fensterkoordinaten auf 0 bei fullscreen...
    //~ win_x= win_y= 0;

    // Move the viewport to top left
    XF86VidModeSetViewPort(video.X_display, video.X_scrnum, 0, 0);
    return true;
  } 
  else
    return false;
}

static bool create_X_window(int width, int height, bool fullscreen, XVisualInfo *visinfo)
{
  XSetWindowAttributes attr;
  int mask;
  
  /* window attributes */
  attr.background_pixel= 0;
  attr.border_pixel= 0;
  attr.colormap= XCreateColormap(video.X_display, video.X_rootwnd, visinfo->visual, AllocNone);
  attr.event_mask= KeyPressMask|KeyReleaseMask |
		   ButtonPressMask|ButtonReleaseMask|PointerMotionMask|ButtonMotionMask |
		   VisibilityChangeMask|StructureNotifyMask;
  if (fullscreen)
  {
    mask= CWBackPixel|CWColormap|CWSaveUnder|CWBackingStore|CWEventMask|CWOverrideRedirect;
    attr.override_redirect= True;
    attr.backing_store= NotUseful;
    attr.save_under= False;
  }
  else
    mask= CWBackPixel|CWBorderPixel|CWColormap|CWEventMask;

  video.X_wnd= XCreateWindow(video.X_display, video.X_rootwnd, 0, 0, width, height,
			     0, visinfo->depth, InputOutput,
			     visinfo->visual, mask, &attr);
  
  XMapWindow(video.X_display, video.X_wnd);

  if(fullscreen)
  {
    XMoveWindow(video.X_display, video.X_wnd, 0, 0);
    XRaiseWindow(video.X_display, video.X_wnd);
    XWarpPointer(video.X_display, None, video.X_wnd, 0, 0, 0, 0, 0, 0);
    XFlush(video.X_display);
    // Move the viewport to top left
    XF86VidModeSetViewPort(video.X_display, video.X_scrnum, 0, 0);
  }
  
  XFlush(video.X_display);
}


// DEP
extern "C" 
int setvideomode(int width, int height, int bpp, bool use_accel)
{
  bool fullscreen= true;
  
  if(video.X_display) XCloseDisplay(video.X_display);

  if (!(video.X_display= XOpenDisplay(NULL)))
  {
    printf("Couldn't open the X display\n");
    return 0;
  }

  video.X_scrnum= DefaultScreen(video.X_display);
  video.X_rootwnd= RootWindow(video.X_display, video.X_scrnum);
  
  // Get video mode list
  int major= 0;
  int minor= 0;
  bool has_vidmode_ext;
  
  if(!XF86VidModeQueryVersion(video.X_display, &major, &minor))
    printf("XF88VidMoDeExTenSioN not present - can't use the screen. Making noises instead.\nYou have to live with that; it's called \"X\"\n"),
    has_vidmode_ext= false;
  else
    has_vidmode_ext= true;
  
  int attrib[] = { GLX_RGBA,
		   GLX_RED_SIZE, 1,
                   GLX_GREEN_SIZE, 1,
                   GLX_BLUE_SIZE, 1,
		   GLX_DOUBLEBUFFER,
    		   None };
  
  video.gl_doublebuf= true;
  
  XVisualInfo *visinfo= pglXChooseVisual(video.X_display, video.X_scrnum, attrib);
  if(!visinfo) { printf("couldn't get X visual info\n"); return 0; }
  
  create_X_window(width, height, has_vidmode_ext&&fullscreen /*fullscreen*/, visinfo);
  
  int real_width, real_height;
  if(has_vidmode_ext&&fullscreen)
  {
    if(!set_X_vid_mode(width, height, &real_width, &real_height))
    {
      printf("Error: Can't switch video mode\n");
      real_width= width;
      real_height= height;
    }
  }
  else
  {
    real_width= width;
    real_height= height;
  }
  
  video.glx_context= pglXCreateContext(video.X_display, visinfo, NULL, True);
  
  pglXMakeCurrent(video.X_display, video.X_wnd, video.glx_context);
  
  pglOrtho(0, width, height, 0, -1.0, 1.0); 
  pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  pglEnable(GL_BLEND);

  video.x= video.y= viewport.x= viewport.y= (*background)->x= (*background)->y= 0;
  video.rgt= viewport.rgt= (*background)->rgt= real_width;
  video.btm= viewport.btm= (*background)->btm= real_height;
  
  //~ XGrabPointer(video.X_display, video.X_wnd, False, 0, GrabModeAsync, None, None, None, CurrentTime);
  if(has_vidmode_ext&&fullscreen)
    XGrabPointer(video.X_display, video.X_wnd, True, 0, GrabModeAsync, GrabModeAsync, video.X_wnd, None, CurrentTime);
  
  return 1;
}



// DEP
extern "C" 
bool bitmap_setpixels(dword id, rgba *pixels, int swidth,int sheight, rect *rcs,
                      int xd, int yd)
{
/*
  window_list *wnd= wlist_find(windows, id);
  if(!wnd) wnd= wlist_find(groups, id);
  if(!wnd) return false;

  prim_bitmap *self= (prim_bitmap *)wnd->self;

  rect rc;
  if(rcs) rc= *rcs;
  else rc.x= rc.y= 0, rc.rgt= swidth, rc.btm= swidth;

  if(xd<0)
    rc.x-= xd,
    xd= 0;
  if(xd + (rc.rgt-rc.x) > self->rgt-self->x)
    rc.rgt= (xd + rc.rgt) - (self->rgt-self->x);

  if(yd<0)
    rc.y-= yd,
    yd= 0;
  if(yd + (rc.btm-rc.y) > self->btm-self->y)
    rc.btm= (yd + rc.btm) - (self->btm-self->y);

  rect clp= { 0,0, swidth,sheight };
  clip_rect(&rc, &clp);

  int x, y;
  rgba *src= pixels + rc.y*swidth + rc.x;
  rgba *dst= self->pixels + yd*(self->rgt-self->x) + xd;

  for(y= rc.btm-rc.y; y; y--)
  {
    for(x= rc.rgt-rc.x-1; x>=0; x--)
      *(dword *)(dst+x)= *(dword *)(src+x);

    src+= swidth;
    dst+= self->rgt-self->x;
  }

  if(self->visible)
  {
    rect pos;
    prim_get_abspos(wnd->self, &pos);
    redraw_rect(&pos);
    update_rect(&pos);
  }
*/
  return true;
}


// DEP
extern "C" 
void setpixel(int x, int y, byte r, byte g, byte b)
{
}

// DEP
extern "C" 
void getpixel(int x, int y, int *r, int *g, int *b)
{
}

// HALB DEP?
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

// DEP
extern "C" 
void fill_rect(rect *r, dword color)
{
  int x, y;
  rect rc= { r->x,r->y, r->rgt,r->btm };

  if( (color&INVISIBLE) || (color&TRANSL_NOPAINT) ) return;
  if(!clip_rect(&rc, &viewport)) return;
  
  //~ byte alpha= color&TRANSL_MASK? ((color&TRANSL_MASK)>>24)*64: 255;
  byte alpha= color&TRANSL_MASK? ((color>>25))*64: 255;
  //~ printf("alpha: %d\n", alpha);
  //~ if(alpha!=255) printf("alpha: %d\n", alpha);
  pglColor4ub( (color>>16)&255, (color>>8)&255, (color)&255, alpha );
  
  if(!video.glBegin_called)
  {
    pglBegin(GL_QUADS);
    video.glBegin_called= true;
  }
  
  pglVertex2i(rc.x, rc.y);
  pglVertex2i(rc.rgt, rc.y);
  pglVertex2i(rc.rgt, rc.btm);
  pglVertex2i(rc.x, rc.btm);
  
}

// DEP
extern "C" 
void hline(int x1, int y, int x2, dword color)
{
/*
  if(video.gl_currentcontext!=0)
    gl_setcontext(video.gl_backbuf_context);

  video.gl_currentcontext= 0;

  gl_hline(x1,y, x2-1, color);
*/

  rect rc= { x1,y, x2,y };
  fill_rect(&rc, color);

}



// DEP
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

// DEP
extern "C" 
void setpixeli(int x, int y, dword color, rect clip)
{
/*
  int f, fx= x&0xFFFF, fy= y&0xFFFF;
  int r, g, b;
  int r1= color>>16&0xFF;
  int g1= color>>8&0xFF;
  int b1= color&0xFF;

  if( ptinrect((x>>16),(y>>16), clip) )
  {
    f= dword((0xFFFF-fx)*(0xFFFF-fy)) >> 16; //(arg->fill? 15: 16);
    if(f>0xFFFF) f= 0xFFFF;
    getpixel( (x>>16),(y>>16), &r, &g, &b );
    setpixel( (x>>16),(y>>16), r+((r1-r)*f>>16),
                               g+((g1-g)*f>>16),
                               b+((b1-b)*f>>16) );
  }
  if( ptinrect((x>>16)+1,(y>>16), clip) )
  {
    f= dword((fx)*(0xFFFF-fy)) >> 16; //(arg->fill? 15: 16);
    if(f>0xFFFF) f= 0xFFFF;
    getpixel( (x>>16)+1,(y>>16), &r, &g, &b );
    setpixel( (x>>16)+1,(y>>16), r+((r1-r)*f>>16),
                                 g+((g1-g)*f>>16),
                                 b+((b1-b)*f>>16) );
  }
  if( ptinrect((x>>16),(y>>16)+1, clip) )
  {
    f= dword((0xFFFF-fx)*(fy)) >> 16; //(arg->fill? 15: 16);
    if(f>0xFFFF) f= 0xFFFF;
    getpixel( (x>>16),(y>>16)+1, &r, &g, &b );
    setpixel( (x>>16),(y>>16)+1, r+((r1-r)*f>>16),
                                 g+((g1-g)*f>>16),
                                 b+((b1-b)*f>>16) );
  }
  if( ptinrect((x>>16)+1,(y>>16)+1, clip) )
  {
    f= dword((fx)*(fy)) >> 16; //(arg->fill? 15: 16);
    if(f>0xFFFF) f= 0xFFFF;
    getpixel( (x>>16)+1,(y>>16)+1, &r, &g, &b );
    setpixel( (x>>16)+1,(y>>16)+1, r+((r1-r)*f>>16),
                                   g+((g1-g)*f>>16),
                                   b+((b1-b)*f>>16) );
  }
*/
}

// DEP
extern "C" 
void plot_outline(pos *outline, int n, dword color, rect clip, pos p)
{
  dword col= (IS_SYSCOL(color)? syscol_table[(color-1)&31]: color);
  for(int i= n-1; i>=0; i--)
  {
    setpixeli(outline[i].x+p.x,outline[i].y+p.y, col, clip);
  }
}


//~ // DEP
//~ void blt_rgba_16(rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
//~ {
  //~ int x, y;
  //~ int w= rc.rgt-rc.x, h= rc.btm-rc.y;
  //~ word *dst= (word *)viewport.data + dst_y*(viewport.rgt-viewport.x) + dst_x;
  //~ rgba *src= _src + rc.y*src_w + rc.x;

  //~ for(y= h-1; y>=0; y--)
  //~ {
    //~ x= w-2;

    //~ if((dst_x+w)&1)     // DWORD alignment sicherstellen
    //~ {
      //~ dword c= *(dword *)(src+x+1);
      //~ dst[x+1]= (c>>19<<11 | (c&(63<<10))>>5 | (c&0xF7)>>3);
      //~ x--;
    //~ }

    //~ for(; x>=0; x-= 2)  // Rest der Zeile, 2 Pixel pro Durchlauf
    //~ {
      //~ dword col0= *(dword *)(src+x), col1= *(dword *)(src+x+1);
      //~ dword c0, c1;
      //~ c0= (col0>>19<<11 | (col0&(63<<10))>>5 | (col0&0xF7)>>3);
      //~ c1= (col1>>19<<11 | (col1&(63<<10))>>5 | (col1&0xF7)>>3);
      //~ *(dword *)(dst+x)= (c0&0xFFFF)|(c1<<16);
    //~ }

    //~ if(x)               // letzter Pixel
    //~ {
      //~ dword c= *(dword *)src;
      //~ *dst= (c>>19<<11 | (c&(63<<10))>>5 | (c&0xF7)>>3);
    //~ }

    //~ src+= src_w;
    //~ dst+= viewport.rgt-viewport.x;
  //~ }
//~ }

//~ // DEP
//~ void blt_rgba_32(rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
//~ {
  //~ int x, y;
  //~ int w= rc.rgt-rc.x, h= rc.btm-rc.y;
  //~ dword *dst= (dword *)viewport.data + dst_y*(viewport.rgt-viewport.x) + dst_x;
  //~ dword *src= (dword *)_src + rc.y*src_w + rc.x;

  //~ for(y= h-1; y>=0; y--)
  //~ {
    //~ for(x= w-1; x>=0; x--)
      //~ dst[x]= src[x];
    //~ src+= src_w;
    //~ dst+= viewport.rgt-viewport.x;
  //~ }
//~ }

// DEP
extern "C" 
void blt_rgba(rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
{
  //~ if(video.bpp==16)
    //~ blt_rgba_16(_src, src_w, src_h, rc, dst_x, dst_y);
  //~ else if(video.bpp==32)
    //~ blt_rgba_32(_src, src_w, src_h, rc, dst_x, dst_y);
}



// DEP
extern "C" 
void paint_cursor(rect pos)
{
  int i, x, y;

  if(is_empty(&pos)) return;


/*
  switch(video.bpp)
  {
    case 8:
    {
      byte *v= (byte *)viewport.data+pos.btm*(viewport.rgt-viewport.x)+pos.x;
      for(y= pos.btm-pos.y-1; y>=0; y--)
      {
        for(x= pos.rgt-pos.x-1; x>=0; x--)
        {
          if(cursor[(y<<4)+x]) v[x]= cursor[(y<<4)+x];
        }
        v-= viewport.rgt-viewport.x;
      }
      break;
    }

    case 16:
    {
      word *v= (word *)viewport.data+(pos.btm-1)*(viewport.rgt-viewport.x)+pos.x;
      dword col;
      for(y= pos.btm-pos.y-1; y>=0; y--)
      {
        for(x= pos.rgt-pos.x-1; x>=0; x--)
        {
          if(cursor[(y<<4)+x])
          {
            col= cursor[(y<<4)+x];
            col= (col>>3<<11)|(col>>2<<5)|(col>>3);
            v[x]= col;
          }
        }
        v-= viewport.rgt-viewport.x;
      }
      break;
    }

    case 24:
    {
      byte *v= (byte *)viewport.data+((pos.btm-1)*(viewport.rgt-viewport.x)+pos.x)*3;
      for(y= pos.btm-pos.y-1; y>=0; y--)
      {
        for(x= pos.rgt-pos.x-1; x>=0; x--)
        {
          if(cursor[(y<<4)+x])
            v[x*3]=
            v[x*3+1]=
            v[x*3+2]= cursor[(y<<4)+x];
        }
        v-= (viewport.rgt-viewport.x)*3;
      }
      break;
    }

    case 32:
    {
      byte *v= (byte *)viewport.data+((pos.btm-1)*(viewport.rgt-viewport.x)+pos.x)*4;
      for(y= pos.btm-pos.y-1; y>=0; y--)
      {
        for(x= pos.rgt-pos.x-1; x>=0; x--)
        {
          if(cursor[(y<<4)+x])
            v[x*4]=
            v[x*4+1]=
            v[x*4+2]= cursor[(y<<4)+x];
        }
        v-= (viewport.rgt-viewport.x)*4;
      }
      break;
    }
  }
*/
}


// DEP?
extern "C" 
void screenshot(char *filename)
{
#ifdef PNG
  int i;
  FILE *f;
  void *rows[viewport.rgt-viewport.x];
  png_structp png_ptr;
  png_infop info_ptr;

  if(!(f= fopen(filename, "wb")))
    { printf("couldn't open output file %s\n", filename); return; }

  if(! (png_ptr= png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0)) )
    { fclose(f); puts("png save failed"); return; }

  if(! (info_ptr= png_create_info_struct(png_ptr)) )
    { fclose(f); png_destroy_write_struct(&png_ptr, 0); puts("png save failed"); return; }

  setjmp(png_ptr->jmpbuf);

  png_init_io(png_ptr, f);

  png_set_IHDR(png_ptr,info_ptr, viewport.rgt-viewport.x,viewport.btm-viewport.y,
               8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr,info_ptr);

  png_set_bgr(png_ptr);
  if(video.bpp==32) png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

  for(i= 0; i<viewport.btm-viewport.y; i++)
    rows[i]= (byte *)viewport.data +
             (i*(viewport.rgt-viewport.x)*video.bpp>>3);

  png_write_image(png_ptr, (png_byte **)rows);

  png_write_end(png_ptr,info_ptr);

  png_destroy_write_struct(&png_ptr,0);

  fclose(f);
  return;
#endif //PNG
}



// DEP
extern "C" 
void update_mouse(struct mouse *m)
{
  XEvent event;

  while (XPending(video.X_display))
  {
    XNextEvent(video.X_display, &event);
 
    switch(event.type)
    {
      case MotionNotify:
        m->x= event.xmotion.x;
        m->y= event.xmotion.y;
        break;

      case ButtonPress:
        m->btns|= event.xbutton.button==1? MOUSE_BTN1: 
	          event.xbutton.button==3? MOUSE_BTN2: MOUSE_BTN2;
        break;

      case ButtonRelease:
        m->btns&= 0xFFFF ^ ( event.xbutton.button==1? MOUSE_BTN1: 
			     event.xbutton.button==3? MOUSE_BTN2: MOUSE_BTN2 );
        break;
    }
  }
  
  pglEnd();
  pglFlush();
  pglXSwapBuffers(video.X_display, video.X_wnd);
  video.glBegin_called= false;
}


// DEP
extern "C" 
void next_event()
{
  XEvent event;
  static struct mouse m;

    XNextEvent(video.X_display, &event);
 
    switch(event.type)
    {
      case KeyPress:
      //~ case KeyRelease:
      {
            //~ if (in_state && in_state->Key_Event_fp)
                    //~ in_state->Key_Event_fp (XLateKey(&event.xkey), event.type == KeyPress);
        XKeyEvent *ev= (XKeyEvent *)&event;
	KeySym sym= XLookupKeysym(ev, 0);
	keyboard_event(event.type==KeyPress, ev->keycode, sym==NoSymbol? 0: sym&0xFF);
	//~ printf("%c", ev->keycode);
	break;
      }
    
      case MotionNotify:
        m.x= event.xmotion.x;
        m.y= event.xmotion.y;
        mouse_event(m.x, m.y, m.btns);
        break;

      case ButtonPress:
        m.btns|= event.xbutton.button==1? MOUSE_BTN1: 
	         event.xbutton.button==3? MOUSE_BTN2: MOUSE_BTN2;
        mouse_event(m.x, m.y, m.btns);
        break;

      case ButtonRelease:
        m.btns&= 0xFFFF ^ ( event.xbutton.button==1? MOUSE_BTN1: 
			    event.xbutton.button==3? MOUSE_BTN2: MOUSE_BTN2 );
        mouse_event(m.x, m.y, m.btns);
        break;
    }
    
    if(video.gl_buffer_dirty)
    {
      pglEnd();
      pglFlush();
      pglFinish();
      if(video.gl_doublebuf) pglXSwapBuffers(video.X_display, video.X_wnd);
      video.glBegin_called= video.gl_buffer_dirty= false;
    }
}


// DEP
extern "C" 
void set_mouse_pos(int x, int y)
{
  //~ SDL_WarpMouse(x, y);
  XWarpPointer(video.X_display, None, video.X_wnd, 0,0, 0,0, x, y);
}


extern "C" 
bool dep_init(dword *syscol_table, struct primitive **background, mouse_event_fn mouse_event, keyboard_event_fn keyboard_event, clip_rect_fn clip_rect)
{
  ::syscol_table= syscol_table;
  ::background= background;
  ::mouse_event= mouse_event;
  ::keyboard_event= keyboard_event;
  ::clip_rect= clip_rect;

  
  if(!load_plugin(plugin_GL, 0)) return false;
  
  return true;
}


// DEP
extern "C" 
void video_cleanup()
{
  XF86VidModeSwitchToMode(video.X_display, video.X_scrnum, &video.X_old_mode);
  XFlush(video.X_display);
}




