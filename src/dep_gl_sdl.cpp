#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <GL/gl.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <FreeImage.h>
#include "../data/gl_sdl/data.h"
#include "../data/gl_sdl/default_font.h"

#define BUILD_DEP
#include "flux.h"


static bool initialized;

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
      unsigned gl_texnum;
      int gl_texwidth, gl_texheight;
    };
  };
};

static int last_gl_texnum;	// hoechste benutzte GL-Texture-Nummer

//~ static Texture *mousecursor;

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

    //~ if(p->type==PT_TEXT)
	//~ draw_text_setup();
    //~ else
	//~ draw_text_end(),
    	//~ glDisable(GL_SCISSOR_TEST),
    	//~ glDisable(GL_TEXTURE_2D);

    return false;
}


// ---------------- FONT STUFF -------------------


void draw_char(font *_font, byte chr, int x,int y, rect clp)
{
    if(chr<32) return;

    gl_font *font= (gl_font *)_font;
    struct srect { short left, top, right, bottom; };
    srect tc= *(srect*)&default_font_texcoords[(chr-33)*4];
    rect rc= { x,y, x+tc.right-tc.left,y+font->height };
    rect rcorig= rc;

    if(clip_rect(&rc, &clp))
    {
	tc.left+= (rc.x-rcorig.x);
	tc.top+= (rc.y-rcorig.y);
	int r= tc.left + rc.rgt-rc.x,
	    b= tc.top + rc.btm-rc.y;

	glTexCoord2f(tc.left, tc.top); 		glVertex2i(rc.x, rc.y);
	glTexCoord2f(r, tc.top); 		glVertex2i(rc.rgt, rc.y);
	glTexCoord2f(r, b); 			glVertex2i(rc.rgt, rc.btm);
	glTexCoord2f(tc.left, b); 		glVertex2i(rc.x, rc.btm);
    }
}

extern "C"
void draw_text(font *font, char *text, int sx,int sy, rect clp, dword col)
{
    rect rc;
    int i;
    int x= sx, y= sy;
    gl_font *f= (gl_font*)font;

    if(!clip_rect(&clp, &viewport)) return;

    dword c= (IS_SYSCOL(col)? syscol_table[col-1&31]: col);
    glColor3ub( (c>>16)&0xFF, (c>>8)&0xFF, (c)&0xFF );

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, f->gl_texnum);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(1.0f/f->gl_texwidth, 1.0f/f->gl_texheight, 1.0f);

    glBegin(GL_QUADS);
    for(i= 0; text[i]; i++)
    {
	if(text[i]<33) switch(text[i])
	{
	    case '\n':
	    	y+= font->height;
	    	x= sx;
	    	break;
	    case ' ':
	    	x+= font->widths['x'-33];
	    	break;
	}
	else
	{
	    draw_char(font, text[i], x,y, clp);
	    x+= font->widths[text[i]-33];
	}
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}


static int get_min_powerof2(int n)
{
  int i= 1;
  while(i<n) i<<= 1;
  return i;
}


// Wird aufgerufen wenn sich die Pixeldaten eines Fonts geändert haben
// GL-Textur hochladen
extern "C"
void font_dep_update(font *_font)
{
    gl_font *font= (gl_font *)_font;
    if(glIsTexture(font->gl_texnum)) glDeleteTextures(1, &font->gl_texnum);
    glGenTextures(1, &font->gl_texnum);
    glBindTexture(GL_TEXTURE_2D, font->gl_texnum);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font->gl_texwidth,font->gl_texheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, font->data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
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
    if(!initialized) return 0;

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

    if(alpha==255) glDisable(GL_BLEND);
	else glEnable(GL_BLEND);

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
//  rect rc= { x1,y, x2,y+1 };
//  fill_rect(&rc, color);	// inefficient..
	y++;
	if(x1<0) x1= 0;
	glColor4ub(color&0xFF, color>>8&0xFF, color>>16&0xFF, 0xFF);
	glBegin(GL_LINES);
	glVertex2i(x1, y);
	glVertex2i(x2, y);
	glEnd();
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

    if(x2-x1 > 0x10000) //(x2&0xFFFF0000)>(x1&0xFFFF0000))
    {
      if(x1 < clip.x<<16) x1= clip.x<<16;
      if(x2 > clip.rgt<<16) x2= clip.rgt<<16;

      hline(short(x1>>16), i+abspos.y, short(x2>>16), col);
    }
  }
#endif
}

static const double fixed2float= 1.0L / 65536;

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

    f= dword((fx)*(fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs+1, yabs+1);

    f= dword((0xFFFF-fx)*(fy)) >> 16;
    colv[3]= f>0xFFFF? 0xFF: f>>8;
    glColor4ubv(colv);
    glVertex2i(xabs, yabs+1);
}


extern "C"
void setpixeli(int x, int y, dword color, rect clip)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip.x,viewport.btm-clip.btm, clip.rgt-clip.x,clip.btm-clip.y);
    glBegin(GL_POINTS);
    setpixeli_plain(x, y, (byte*)&color);
    glEnd();
    glDisable(GL_SCISSOR_TEST);
}

long outline_pixel_offset_x;

extern "C"
void plot_outline(pos *outline, int n, dword color, rect clip, pos p)
{
    dword col= (IS_SYSCOL(color)? syscol_table[(color-1)&31]: color);
    int old_scissor_box[4];
    int old_scissor_enabled;
    glGetIntegerv(GL_SCISSOR_TEST, &old_scissor_enabled);
    glGetIntegerv(GL_SCISSOR_BOX, old_scissor_box);
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip.x,clip.y, clip.rgt-clip.x,clip.btm-clip.y);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//    glColor3ub( (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF );
    glBegin(GL_POINTS);
    byte colv[]= { (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF, 0 };
    for(int i= n-1; i>=0; i--)
    {
		int x= outline[i].x+p.x, y= outline[i].y+p.y;
		setpixeli_plain(x+outline_pixel_offset_x, y, colv);
    }
    glEnd();
    if(!old_scissor_enabled) glDisable(GL_SCISSOR_TEST);
    glScissor(old_scissor_box[0], old_scissor_box[1], old_scissor_box[2], old_scissor_box[3]);
}


extern "C"
void blt_rgba(flux_rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y)
{
}

static GLuint load_texture(const char *filename, int *width= 0, int *height= 0)
{
    SDL_Surface *surface= IMG_Load(filename);
    if(!surface) return 0;
    GLuint name;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    int bpp= surface->format->BitsPerPixel;
    int format= (bpp==8? GL_LUMINANCE: bpp==16? GL_LUMINANCE_ALPHA: bpp==24? GL_RGB: GL_RGBA);
    int internalformat= (bpp==32? 4: bpp==8? 1: 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, surface->w,surface->h, 0,
				 format, GL_UNSIGNED_BYTE, surface->pixels);
	if(width) *width= surface->w;
	if(height) *height= surface->h;
	SDL_FreeSurface(surface);
    return name;
}


static GLuint load_texture_mem(const void *src, int memsize, int *width= 0, int *height= 0)
{
    SDL_Surface *surface= IMG_Load_RW(SDL_RWFromConstMem(src, memsize), 1);
    if(!surface) return 0;
    GLuint name;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    int bpp= surface->format->BitsPerPixel;
    int format= (bpp==8? GL_LUMINANCE: bpp==16? GL_LUMINANCE_ALPHA: bpp==24? GL_RGB: GL_RGBA);
    int internalformat= (bpp==32? 4: bpp==8? 1: 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, surface->w,surface->h, 0,
				 format, GL_UNSIGNED_BYTE, surface->pixels);
	if(width) *width= surface->w;
	if(height) *height= surface->h;
	SDL_FreeSurface(surface);
    return name;
}




static unsigned int mousecursor_gltex;

extern "C"
void load_cursor()
{
	mousecursor_gltex= load_texture_mem(default_cursor_png, default_cursor_png_size, false);
/*
    FIMEMORY *fimem= FreeImage_OpenMemory((BYTE*)default_cursor_png, default_cursor_png_size);
    FREE_IMAGE_FORMAT fif= FreeImage_GetFileTypeFromMemory(fimem, default_cursor_png_size);
    FIBITMAP *fibitmap= FreeImage_LoadFromMemory(fif, fimem);

    if(FreeImage_GetBPP(fibitmap)!=32)
    { FIBITMAP *conv= FreeImage_ConvertTo32Bits(fibitmap); FreeImage_Unload(fibitmap); fibitmap= conv; }

    int width= FreeImage_GetWidth(fibitmap);
    int height= FreeImage_GetHeight(fibitmap);
    byte *data= (byte*)malloc(width*height*4);

    byte *src= (byte*)FreeImage_GetBits(fibitmap)+FreeImage_GetPitch(fibitmap)*(FreeImage_GetHeight(fibitmap)-1);
    byte *dst= data;
    for(int y= FreeImage_GetHeight(fibitmap); y>0; y--)
    {
		memcpy(dst, src, FreeImage_GetWidth(fibitmap)*4);
		dst+= FreeImage_GetWidth(fibitmap)*4;
		src-= FreeImage_GetPitch(fibitmap);
    }

    FreeImage_Unload(fibitmap);
    FreeImage_CloseMemory(fimem);

    glGenTextures(1, &mousecursor_gltex);
    glBindTexture(GL_TEXTURE_2D, mousecursor_gltex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width,height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);	//_TO_EDGE
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    free(data);
*/
}


extern "C"
void paint_cursor(rect pos)
{
    if(is_empty(&pos)) return;

    int size= 32;

    //~ if(cur_mouse_handler)
    //~ {
	//~ if(mouse.btns) size= 28, pos.x+= 1, pos.y+= 1, glColor3ub(192, 6, 22);
	//~ else glColor3ub(255, 8, 32);
    //~ }
    //~ else glColor3ub(255, 255, 255);

    glColor3ub(255, 255, 255);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mousecursor_gltex);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    return (initialized= true);
}


extern "C"
void video_cleanup()
{
}




// Font-Sachen

gl_font *font_load_texture(void *buffer, int buffersize, int charheight, short *texcoord_table, int tablesize)
{
    gl_font *f= (gl_font*)calloc(1, sizeof(gl_font));

    f->width= f->height= charheight;

    SDL_Surface *surface= IMG_Load_RW(SDL_RWFromConstMem(buffer, buffersize), 1);
    if(!surface) { printf("couldn't load font texture\n"); return 0; }

    int bpp= surface->format->BitsPerPixel;
    if(bpp!=32) { printf("loading font texture: bad BitsPerPixel %d (should be 32)\n", bpp); return 0; }

    f->gl_texwidth= surface->w;
    f->gl_texheight= surface->h;
    f->data= (byte*)malloc(f->gl_texwidth*f->gl_texheight*4);

    uint8_t *src= (uint8_t *)surface->pixels;
    uint32_t *dst= (uint32_t *)f->data;
    for(int y= surface->h; y>0; y--)
    {
		memcpy(dst, src, surface->w*4);
		dst+= surface->w;
		src+= surface->pitch;
    }
	SDL_FreeSurface(surface);

    int i;
    for(i= 0; i<tablesize; i++)
	f->widths[i]= texcoord_table[i*4+2]-texcoord_table[i*4];
    for(; i<224; i++)
	f->widths[i]= 0;

    font_dep_update((font*)f);

    return f;
}


void load_default_fonts()
{
    def_fonts[ prop_t(FONT_DEFAULT) ]=
    def_fonts[ prop_t(FONT_ITEMS) ]=
    def_fonts[ prop_t(FONT_CAPTION) ]=
    def_fonts[ prop_t(FONT_SYMBOL) ]=
    def_fonts[ prop_t(FONT_FIXED) ]=
    	(font*)font_load_texture(default_font_png, default_font_png_size, 14, default_font_texcoords,
								sizeof(default_font_texcoords)/(sizeof(default_font_texcoords[0])*4));
}




font *font_load_raw(char *filename, int fw,int fh, bool fixed)
{
    //~ // create dummy
    //~ gl_font *f= (gl_font*)calloc(1, sizeof(gl_font));
    //~ f->width= fw; f->height= fh;
    //~ return (font*)f;
    return 0;
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


int font_gettextwidth(font *font, const char *text)
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
    else if( byte(text[i])==32 )
	linewidth+= font->widths['x'-33];
    else if( byte(text[i]) >= 33 )
      linewidth+= font->widths[text[i]-33];
    else
      linewidth+= font->width;
  }

  return max(linewidth, ret);
}

int font_gettextheight(font *font, const char *text)
{
    int fh= font_height(font), h= fh;

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





