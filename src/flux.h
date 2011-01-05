#ifndef WNDTEST_H
#define WNDTEST_H
#include "defs.h"
#include "sys.h"
#include "prop_list.h"

#ifdef ARM9

extern "C"
{
    void init_nds_fonts();
};

#endif //NDS


#define TEXTINPUT_MAX 1024	// maximum string length for single-line textinput
typedef bool (*textinput_filter_fn) (dword id, char text[TEXTINPUT_MAX], int &cursorpos, int code, int chr);
typedef void (*textinput_on_change_fn) (dword id, char *str);


struct pos
{
  int x, y;
};

typedef struct flux_rgba
{
  byte b, g, r, a;
} flux_rgba;

typedef struct rect
{
  int x, y, rgt, btm;

#ifdef __cplusplus
  void operator+= (rect& a)
  { x+= a.x; y+= a.y; rgt+= a.rgt; btm+= a.btm; }
  void operator-= (rect& a)
  { x-= a.x; y-= a.y; rgt-= a.rgt; btm-= a.btm; }
#endif //__cplusplus
} rect;


struct window_list
{
  struct primitive *self;
  struct window_list *prev, *next;
  struct rectlist *dirty_rects;
};

enum prim_type
{
  PT_GROUP= 0,
  PT_RECT,
  PT_TEXT,
  PT_POLY,
  PT_ELLIPSE,
  PT_ARC,
  PT_BITMAP,
  PT_FRAME
};

#define HALIGN(a)       ((a)&7)
#define VALIGN(a)       ((a)&(7<<3))


#define ALIGN_LEFT      (1<<0)
#define ALIGN_HCENTER   (1<<1)
#define ALIGN_RIGHT     (1<<2)

#define ALIGN_TOP       (1<<3)
#define ALIGN_VCENTER   (1<<4)
#define ALIGN_BOTTOM    (1<<5)


// Alignment Flags ob X/Y Position bzw Breite/Hoehe relativ zum Parent sind etc.
#define XREL            (1<<8)
#define YREL            (1<<9)
#define PREL            (XREL|YREL)
#define WREL            (1<<10)
#define HREL            (1<<11)
#define SREL            (WREL|HREL)


#define NOPARENT        0xFFFFFFFF
#define NOWND           0xFFFFFFFF

#define MAXSCALE        10000


#define INVISIBLE       0x10000000
#define TRANSL_0        INVISIBLE
#define TRANSL_1        0x02000000
#define TRANSL_2        0x04000000
#define TRANSL_3        0x08000000
#define TRANSL_NOPAINT  0x20000000
#define TRANSL_MASK     (TRANSL_1|TRANSL_2|TRANSL_3|INVISIBLE|TRANSL_NOPAINT)
#define SYSCOL(c)       (0x01000000|(c))
#define IS_SYSCOL(c)    ((c)&0x01000000)


#define MAX_DEFAULTFONTS        16

#define FONT_DEFAULT    ( (font *)0 )
#define FONT_ITEMS      ( (font *)1 )
#define FONT_CAPTION    ( (font *)2 )
#define FONT_SYMBOL     ( (font *)3 )
#define FONT_FIXED      ( (font *)4 )



#define NSYSCOLORS      32

enum syscols
{
  COL_BAKGND=   SYSCOL(1),      // Hintergrundfenster ("Desktop")
  COL_WINDOW=   SYSCOL(2),      // Fenster
  COL_TEXT=     SYSCOL(3),      // Normaler Text
  COL_ITEM=     SYSCOL(4),      // Buttons etc.
  COL_ITEMHI=   SYSCOL(5),      //      ''      beleuchtet
  COL_ITEMLO=   SYSCOL(6),      //      ''      abgedunkelt
  COL_ITEMTEXT= SYSCOL(7),
  COL_TITLE=    SYSCOL(8),      // Titelzeile
  COL_FRAMEHI=  SYSCOL(9),
  COL_FRAMELO=  SYSCOL(10),
};

void setsyscolor(int index, dword value);

#ifndef min
#define min(a,b) ((a)<(b)? (a): (b))
#define max(a,b) ((a)>(b)? (a): (b))
#endif

#define is_empty(_rect) ( (_rect)->rgt-(_rect)->x<=0 || (_rect)->btm-(_rect)->y<=0 )


typedef int (*cb_mouse)(prop_t arg, struct primitive *self,
                        int type, int x, int y, int btn);

enum mouse_btns
{
  MOUSE_BTN1= 1, MOUSE_BTNLEFT= 1,
  MOUSE_BTN2= 2, MOUSE_BTNRIGHT= 2,
  MOUSE_BTN3= 4, MOUSE_BTNMIDDLE= 4,
  MOUSE_BTN4= 8, MOUSE_BTNWHEELUP= 8,
  MOUSE_BTN5= 16, MOUSE_BTNWHEELDOWN= 16
};

enum mouse_cb_types
{
  MOUSE_DOWN,
  MOUSE_UP,
  MOUSE_IN,
  MOUSE_OVER,
  MOUSE_OUT
};


typedef prop_t (*cb_props)(prop_t arg, struct primitive *self,
			    int type, const char *name, int id, prop_t value);


typedef int (*cb_status)(prop_t arg, struct primitive *self,
                         int type);

enum status_cb_types
{
    STAT_CREATE= 0,
    STAT_DESTROY,
    STAT_SHOW,
    STAT_HIDE,
    STAT_GAINFOCUS,	// Keyboard
    STAT_LOSEFOCUS
};


typedef void (*cb_keybd)(prop_t arg, struct primitive *self,
			 int isdown, int scancode, int chr);

typedef void (*cb_paint)(prop_t arg, struct primitive *self, rect *abspos, const rectlist *dirty_rects);

enum z_pos
{
  Z_NORMAL= 0,
  Z_BOTTOM= 1,
  Z_TOP= 2
};

struct primitive: public rect
{
    enum prim_type type;
    struct primitive *parent;
    struct window_list *children;
    int alignment;        	// rechts/zentriert/links, oben/zentriert/unten
    dword id;

    struct
    {
	dword transparent: 1, 	// potentiell "durchsichtig"
	      is_frame: 1,    	// gehört zum Frame des Parents
	      z_pos: 2,       	// normal/topmost/bottommost
	      visible: 1,     	// im Moment sichtbar
	      resizable: 1,   	// man kann mit rechter maustaste größe ändern
	      sticky: 1;      	// immer sichtbar
    } flags;

    rect rcnonframe;      	// Bereich in der Mitte, der nicht vom Rahmen
			  	// überdeckt ist

    struct prop_list *props;	// Property-Liste. Wird beim ersten wnd_prop_add()
				// initialisiert.

    int highest_propid;		// höchster property-id für prop_add()

    enum callback_index { CB_MOUSE= 0, CB_PROPS, CB_STATUS, CB_KEYBD, CB_PAINT /*, CB_SIZE*/ };

    union {
	struct { void (*func)(); prop_t arg; }
	    cb_array[1];

    	struct			// Order matters!
    	{
	    cb_mouse mouse;
	    prop_t mouse_arg;
	    cb_props props;
	    prop_t props_arg;
	    cb_status status;
	    prop_t status_arg;
	    cb_keybd keybd;
	    prop_t keybd_arg;
	    cb_paint paint;
	    prop_t paint_arg;
    	} callbacks;
    };

    void *privdata;       	// Private Daten
};

struct prim_rect: public primitive
{
  int color;
};

struct prim_text: public primitive
{
  int color;
  char *text;
  struct font *font;
};

struct prim_group: public prim_rect
{
  char *name;
};

struct prim_bitmap: public primitive
{
  flux_rgba *pixels;
};

struct prim_frame: public primitive
{
  int framewidth;
  dword col_left, col_top, col_right, col_btm;
};


enum wnd_relation
{ SELF= 0, PREV, NEXT, CHILD, PARENT, FIRST, LAST };


struct shape_scanline
{
  int x1, x2;
  int opacity;
};

struct fltpos
{
  float x, y;
};

/*
struct dblpos
{
  double x, y;
};
*/
#define dblpos fltpos

struct prim_shape: public primitive
{
  int color;
  dword filled: 1;

  pos cache_size;
  int n_outlinedots;
  shape_scanline *scanline_cache;
  pos *outline_cache;
};

struct prim_shape_poly: public prim_shape
{
  //~ int color;
  int nverts;
  fltpos *vertexes;
};

struct prim_shape_ellipse: public prim_shape
{
  //~ int color;
  float cx, cy, rx, ry;
  dword filled: 1;
};

struct prim_shape_arc: public prim_shape
{
  //~ int color;
  float cx, cy, rx, ry;
  float w1, w2;
  dword outlines: 1;
};


struct scanline_cb_struct
{
  shape_scanline *scanlines;
  pos *outlinedots;
  int width, height;
  dword color;
  bool fill;
  int n_outlinedots;
};


struct mouse
{
    double x, y;
    int btns;
};

struct rectlist
{
  rect *self;
  struct rectlist *prev, *next;
};


struct font
{
  byte *data;
  int width, height;
  int widths[224];
  int refcount;
  byte privdata[32];	// system dependent - GL code stores texture number here
};



void wlist_move(window_list *wnd, window_list *dest);
void wlist_free(window_list *list);
window_list *wlist_find(window_list *start, dword id);
window_list *wlist_end(window_list *start);
primitive *findprim(dword id);

void redraw_rect(rect *rc);
void redraw_cursor();

bool ptinrect(int x, int y, rect& rc);
int subtract_rect(rect r1, rect r2, rect *pchildrc);
rectlist *rectlist_subtract_rect(rectlist *rcs, rect *rc);
void rectlist_free(rectlist *head);

window_list *grplist_find(window_list *start, const char *name);
void get_child_rel_pos(primitive *child, rect *prntrc, rect *dest);
void prim_get_abspos(primitive *child, rect *dest, bool clip_to_parent);
void calc_nonframe_rect(primitive *frameroot, rect& dest);
void dump_wlist(window_list *src, int indent);
void recalc_frame(primitive *frame, primitive *parent);
primitive *find_prim_pos(int x, int y, bool mustbevisible= false);
void prim_destroy(primitive *prim);

window_list *prim_create(window_list *prev, int structsize, bool is_child,
                         int x,int y, int rgt,int btm, int align);

window_list *prim_create_rect(window_list *prev, bool is_child,
                              int x,int y, int width,int height, int col, int align);

window_list *prim_create_text(window_list *prev, bool is_child,
                              int x,int y, int width,int height,
                              const char *text, dword col, font *fnt, int align);

window_list *prim_create_bitmap(window_list *prev, bool is_child,
                                int x,int y, int width,int height, int align);

window_list *prim_create_frame(window_list *prev, bool is_child,
                               int x,int y, int width,int height,
                               int framewidth,
                               dword col_left, dword col_top,
                               dword col_right, dword col_btm,
                               int align);

window_list *prim_create_poly(window_list *prev, bool is_child,
                              int x,int y, int width,int height, int col, int align,
                              bool filled, int nverts, dblpos *verts);

dword create_rect(dword parent, int x,int y, int width,int height, int col, int align= 0);
dword create_bitmap(dword parent, int x,int y, int width,int height, int align= 0);
dword create_text(dword parent, int x,int y, int width,int height, const char *text, int col, font *fnt, int align= 0);
dword create_poly(dword parent, int x,int y, int width,int height, int col, int align, bool filled, int nverts, dblpos *verts);
dword create_ellipse(dword parent, int x,int y, int width,int height, int col, int align, bool filled, double cx, double cy, double rx, double ry);
dword create_frame(dword parent, int x,int y, int width,int height, int framewidth, dword col_left, dword col_top, dword col_right, dword col_btm, int align= 0);
dword create_arc(dword parent, int x,int y, int width,int height, int col, int align,
                 bool filled, bool lines,
                 double cx,double cy, double rx,double ry,
                 double a1, double a2);

void unhash_id(dword id);

int wnd_get_abspos(dword id, rect *pos);
int wnd_getx(dword id);
int wnd_gety(dword id);
int wnd_getw(dword id);
int wnd_geth(dword id);
int wnd_getrelx(dword id);
int wnd_getrely(dword id);
int wnd_getrelw(dword id);
int wnd_getrelh(dword id);
void wnd_setpos(dword id, int x, int y);
dword wnd_getalignment(dword id);
bool wnd_setalignment(dword id, dword align);
void wnd_setsize(dword id, int width, int height);
void wnd_totop(dword id);
dword wnd_walk(dword id, wnd_relation rel0, ...);
bool wnd_isvisible(dword id);
bool wnd_isresizable(dword id);
bool wnd_setresizable(dword id, bool resizable);


bool text_gettext(dword id, char *text, int bufsize);
bool text_settext(dword id, const char *text);

bool rect_setcolor(dword id, dword color);
void frame_setcolors(dword id, dword col_left, dword col_top, dword col_right, dword col_btm);



void wnd_destroy(dword id);
void wnd_close(dword id);
void wnd_minimize(dword id);
void wnd_set_zpos(dword id, int state);
bool wnd_set_stickybit(dword id, bool sticky);
bool wnd_issticky(dword id);
void wnd_setx(dword id, int x);
void wnd_sety(dword id, int y);
void wnd_setwidth(dword id, int width);
void wnd_setheight(dword id, int height);
void wnd_totop(dword id);
int poly_setverts(dword wnd, int index, int n, ...);
int poly_setallverts(dword id, int n, fltpos *verts);


int wnd_addprop(dword id, const char *name, prop_t value, int type, const char *desc= "");
#define wnd_prop_add wnd_addprop
prop_t wnd_getprop(dword id, const char *name);
prop_t wnd_setprop(dword id, const char *name, prop_t value);
int wnd_getproptype(dword id, const char *name);

bool wnd_show(dword id, bool visible);
dword clone_group(const char *name, dword hparent, int x= 0, int y= 0,
                  int w= MAXSCALE, int h= MAXSCALE, int align= ALIGN_LEFT|ALIGN_TOP|WREL|HREL);
dword get_group_id(const char *name);
dword clone_frame(const char *name, dword hparent);
void wnd_setisize(dword id, int iwidth, int iheight);
void wnd_setiheight(dword id, int iheight);

int mouse_x(void);
int mouse_y(void);
int mouse_btns(void);

primitive *find_mouse_handler(int x, int y, rect *pos= 0);


dword create_group(const char *name);
bool wnd_set_mouse_capture(dword id);
bool wnd_set_props_callback(dword id, cb_props cb, prop_t arg);
bool wnd_set_mouse_callback(dword id, cb_mouse cb, prop_t arg);
bool wnd_set_status_callback(dword id, cb_status cb, prop_t arg);
bool wnd_set_paint_callback(dword id, cb_paint cb, prop_t arg);
bool wnd_set_kbd_callback(dword id, cb_keybd cb, prop_t arg);

void wnd_setkbdfocus(dword wnd);
dword wnd_getkbdfocus();



typedef void (*timer_func)(prop_t arg, int idtimer, dword time);
void run_timers();
int timer_create(timer_func tick_func, prop_t tick_arg, dword interval);
int timer_kill(int id);




extern struct mouse mouse;
extern struct window_list *windows;
extern struct window_list *groups;
extern dword id_mousehook;
extern dword bpp;
extern window_list *windows, *groups;
extern primitive *cur_mouse_handler;




//        ---------------------------- tests/default groups ----------------------------


typedef void (*btn_cbclick)(int id, bool btndown, int whichbtn);
typedef void (*scrlbox_onchange)(int id, int pos);
typedef void (*listbox_onselect)(int id, int item, int dblclick);


void create_scrollbox_group();
void create_dropdown_group();
void create_listbox_group();


void unuso_calc();
void filesel();
void video_dlg(dword parent);
void analog_clock(int x, int y, int width, int height);


void init_defgroups();
void create_frame_groups();
void create_button_groups();
void create_scrollbox_group();
void create_listbox_group();
void create_desktopicon();
void create_titleframe();
void create_dropdown_group();
void create_tooltip_group();
void create_checkbox_group();
void create_textinput_group();
void create_multiline();

bool flux_init();
bool flux_screenresize(int scr_w, int scr_h);	// calls flux_init() if necessary
void flux_tick();
void flux_shutdown();

void filesel();
void video_dlg(dword parent);
void create_clock();


//        ---------------------------- DEP ----------------------------

struct viewport: public rect
{
  void *data;
};


typedef void (*mouse_event_fn) (double x, double y, int btn);
typedef bool (*keyboard_event_fn) (bool down, int scancode, int ch);
typedef bool (*clip_rect_fn)(rect *rc, const rect *clip);


#ifndef BUILD_DEP

extern "C" struct viewport viewport;
//extern "C" struct primitive *backdrop;
extern "C" dword syscol_table[NSYSCOLORS];

void flux_mouse_event(double x, double y, int btn);
void flux_mouse_move_event(double xrel, double yrel);
void flux_mouse_button_event(int btn, bool down);
bool flux_keyboard_event(bool down, int scancode, int chr);	// returns true if event was eaten

bool clip_rect(rect *rc, const rect *clp);

bool dep_prim_redraw(primitive *p, rect *_abspos, rect *upd);

#endif //BUILD_DEP


int polyscanline_cb_left(struct scanline_cb_struct *arg, int x, int y);
int polyscanline_cb_right(struct scanline_cb_struct *arg, int x, int y);
int scanline_callback(struct scanline_cb_struct *arg, int x, int y);

void aq_exec();

extern "C"
{
    void draw_text(struct font *font, char *text, int sx,int sy, rect clp, dword col);
    void font_dep_update(font *_font);
    void font_dep_free(font *font);
    void put_alpha_map(dword color, byte *bmap, int bw,int bh, rect srcpos, int xdest,int ydest);
    void update_rect(const rect *r);
    int flux_setvideomode(int width, int height, int bpp, bool use_accel= true);
    bool bitmap_setpixels(dword id, flux_rgba *pixels, int swidth,int sheight, rect *rcs, int xd, int yd);
    dword map_color(dword c);
    void fill_rect(rect *r, dword color);
    void hline(int x1, int y, int x2, dword color);
    void fill_scanlines(shape_scanline *scanlines, rect clip, rect abspos, dword color);
    void plot_outline(pos *outline, int n, dword color, rect clip, pos p);
    void blt_rgba(flux_rgba *_src, int src_w, int src_h, rect rc, int dst_x, int dst_y);
    void paint_cursor(rect pos);
    void update_mouse(struct mouse *m);
    void next_event();
    void set_mouse_pos(int x, int y);
    bool dep_init(dword *syscol_table, struct primitive **backdrop, mouse_event_fn mouse_event, keyboard_event_fn keyboard_event, clip_rect_fn clip_rect);
    void video_cleanup();
    void setpixel(int x, int y, byte r, byte g, byte b);
    void getpixel(int x, int y, int *r, int *g, int *b);
    void setpixeli(int x, int y, dword color, rect clip);
    void load_cursor();

    extern font *def_fonts[MAX_DEFAULTFONTS];
    font *font_load_raw(char *filename, int fw,int fh, bool fixed= false);
    font *_font_getloc(font *f);
    void font_ref(font *f);
    void font_deref(font *&f);
    void font_free(font *f);
    int font_gettextwidth(font *font, const char *text);
    int font_gettextheight(font *font, const char *text);
    int font_width(font *font);
    int font_height(font *font);
    void font_antialias(font *font, int f, int f2= 0);
    void load_default_fonts();
};



#endif
