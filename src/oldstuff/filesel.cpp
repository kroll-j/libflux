#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "flux.h"
#include "prop_list.h"
#include "flux-edm.h"


#undef WIDTH
#undef HEIGHT
#define WIDTH   290
#define HEIGHT  204


static int filesel_select(const dirent *entry)
{
  if(entry->d_name[0]=='.' && strcmp(entry->d_name, ".."))
    return false;
  return true;
}

static int filesel_compare(dirent **file1, dirent **file2)
{
  struct stat stat1, stat2;

  char *fname1= (*file1)->d_name, *fname2= (*file2)->d_name;

  if(stat(fname1, &stat1)==-1)
  {
    if(stat(fname2, &stat2)==-1)
      return strcasecmp(fname1, fname2);
    stat1.st_mode= 0;
  }
  if(stat(fname2, &stat2)==-1)
  {
    stat2.st_mode= 0;
  }

  if(S_ISDIR(stat1.st_mode) && !S_ISDIR(stat2.st_mode))
    return -1;
  if(S_ISDIR(stat2.st_mode) && !S_ISDIR(stat1.st_mode))
    return 1;

  return strcasecmp(fname1, fname2);
}


static void filesel_fill_listbox(dword listbox, char *dirname)
{
  struct dirent **namelist;

  if(chdir(dirname)==-1)
  {
    char ch[1024];
    sprintf(ch, "[chdir %s failed]", dirname);
    wnd_setprop(listbox, "append_item", (prop_t)"/.");
    wnd_setprop(listbox, "append_item", (prop_t)ch);
    return;
  }

  int n= -1;
  //~ int n= scandir(".", &namelist, filesel_select,
                 //~ (int(*)(const void*, const void *))filesel_compare);

  if(n<0)
  {
    char ch[1024];
    sprintf(ch, "[Couldn't read directory %s]", dirname);
    wnd_setprop(listbox, "append_item", (prop_t)"/.");
    wnd_setprop(listbox, "append_item", (prop_t)ch);
    return;
  }

  for(int i= 0; i<n; i++)
  {
    struct stat stat1;
    if(!stat(namelist[i]->d_name, &stat1) && stat1.st_mode&S_IFDIR)
    {
      char *str= (char *)malloc(strlen(namelist[i]->d_name)+2);
      sprintf(str, "/%s", namelist[i]->d_name);
      wnd_setprop(listbox, "append_item", (prop_t)str);
      free(str);
    }
    else
      wnd_setprop(listbox, "append_item", (prop_t)namelist[i]->d_name);

    free(namelist[i]);
  }


  char newdir[1024];
  getcwd(newdir, 1023);

  dword dropdown= wnd_walk(listbox, NEXT, NEXT, NEXT, SELF);
  wnd_setprop(dropdown, "index", 0);
  char *text= (char *)wnd_getprop(dropdown, "item_text");
  if(text && !strcmp(newdir, text)) return;
  wnd_setprop(dropdown, "insert_item", (prop_t)newdir);
  wnd_setprop(dropdown, "sel_index", 0);
}


static void filesel_scroller_change(dword id, int pos)
{
  wnd_setprop(wnd_walk(id, NEXT, SELF), "scroll_pos", pos);
}

static void filesel_onselect(dword id, int item, int dblclick)
{
  dword scroller= wnd_walk(id, PREV, SELF);
  int pos= item * MAXSCALE / wnd_getprop(id, "n_items");
  wnd_setprop(scroller, "knobpos", pos);

  wnd_setprop(id, "index", item);
  char *text= (char *)wnd_getprop(id, "item_text");
  text_settext(wnd_walk(id, NEXT, SELF), text);

  if(dblclick)
  {
    if(text[0]=='/')    // Verzeichnis
    {
      wnd_setprop(id, "clear", 0);
      filesel_fill_listbox(id, text+1);
      wnd_setprop(wnd_walk(id, PREV, SELF), "knobpos", 0);
    }
  }
}


static int filesel_cb_status(prop_t arg, struct primitive *self, int type)
{
  switch(type)
  {
    case STAT_CREATE:
      break;

    case STAT_DESTROY:
      break;
  }
}


// Callback fuer History dropdown liste
static void filesel_cb_dropdown(dword id, int idx)
{
  char cwd[1024];
  getcwd(cwd, 1023);

  wnd_setprop(id, "index", idx);
  char *seltext= (char *)wnd_getprop(id, "item_text");

//  if(seltext && strcmp(seltext, cwd)!=0)
  if(idx!=0)
  {
    for(int i= 0; i<=idx; i++) wnd_setprop(id, "delete_item", 0);

    dword idlist= wnd_walk(id, PREV, PREV, PREV, SELF);
    wnd_setprop(idlist, "clear", 0),
    filesel_fill_listbox(idlist, seltext);
    wnd_setprop(wnd_walk(idlist, PREV, SELF), "knobpos", 0);
  }
}


void filesel()
{
  dword wnd= create_rect(NOPARENT, 100,100, WIDTH,0, COL_WINDOW);
  wnd_prop_add(wnd, "history", 0, PROP_DWORD);
  wnd_set_status_callback(wnd, filesel_cb_status, 0);

  dword frame= clone_frame("titleframe", wnd);


  dword scroller= clone_group("scrollbox", wnd, 0,20, 12,32,
                              ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);

  wnd_setprop(scroller, "on_change", (prop_t)filesel_scroller_change);


  dword listbox= clone_group("listbox", wnd, 0,20, 14,32,
                             ALIGN_LEFT|ALIGN_RIGHT|ALIGN_TOP|ALIGN_BOTTOM);

  wnd_setprop(listbox, "on_select", (prop_t)filesel_onselect);


  dword text= create_text(wnd, 5,32-15-3, MAXSCALE,15, "",
                          COL_ITEMTEXT, FONT_ITEMS, ALIGN_LEFT|ALIGN_BOTTOM|WREL);

  clone_group("sunkenframe0", wnd, 0,32-15-2, MAXSCALE,15, ALIGN_LEFT|ALIGN_BOTTOM|WREL);


  dword history= clone_group("dropdown", wnd, 64+8,3, 0,15, ALIGN_LEFT|ALIGN_RIGHT);
  wnd_setprop(history, "on_select", (prop_t)filesel_cb_dropdown);


  wnd_setisize(wnd, WIDTH,HEIGHT);

  filesel_fill_listbox(listbox, ".");
}
