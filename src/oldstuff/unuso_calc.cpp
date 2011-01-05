#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fluxws.h"
#include "prop_list.h"



// Das soll ein Taschenrechner werden,
// der ungefaehr so unbenutzbar ist wie GnomeCalc


extern "C" {
//char *float2bcd(double f);
}

enum actions
{
  a_ac= 0,      // AC
  a_add,        // Operatoren + - * /
  a_sub,
  a_mul,
  a_div,
  a_result      // =
};


typedef struct
{
  int lastop;
  double lastresult;
  bool clear;
  dword idtext;
} calc_struct;



double unuso_calc_do_op(char *num, int lastop, double lastresult)
{
  double operand= strtod(num, 0);

  switch(lastop)
  {
    case a_result:
    case a_ac:
      printf("    %G\n", operand);
//      printf("%s\n", float2bcd(operand));
      return operand;

    case a_add:
      printf("  + %G\n", operand);
      return lastresult+operand;
      break;

    case a_sub:
      return lastresult-operand;
      break;

    case a_mul:
      return lastresult*operand;
      break;

    case a_div:
      return lastresult/operand;
      break;
  }
}


void unuso_calc_btnclick(dword id, int mousebtn)
{
  if(!mousebtn) return;

  char *btntext= (char *)wnd_prop_get(id, "text");
  dword idmainwnd;
  if(!strcmp(btntext, "AC")) idmainwnd= wnd_walk(id, PARENT, SELF);
  else idmainwnd= wnd_walk(id, PARENT, PARENT, SELF);
  calc_struct *calc= (calc_struct *)wnd_prop_get(idmainwnd, "calc_struct");
  char numtext[1024]= "";

  text_gettext(calc->idtext, numtext, 128);


  if(btntext[0]>='0' && btntext[0]<='9' || btntext[0]=='.')
  {
    if(calc->clear)
    {
      if(btntext[0]=='.') text_settext(calc->idtext, "0.");
      else text_settext(calc->idtext, btntext);
      calc->clear= false;
      return;
    }

    if(btntext[0]=='0' && !strcmp(numtext, "0")) return;
    if(btntext[0]=='.' && strchr(numtext, '.')) return;
    if(numtext[0]=='0' && numtext[1]!='.' && btntext[0]!='.') numtext[0]= 0;

    strcat(numtext, btntext);
    text_settext(calc->idtext, numtext);
  }
  else if(!strcmp(btntext, "AC"))
  {
    text_settext(calc->idtext, "0");
    calc->lastresult= 0;
    calc->lastop= a_ac;
  }
  else
  {
    calc->clear= true;

    calc->lastresult= unuso_calc_do_op(numtext, calc->lastop, calc->lastresult);

    calc->lastop= (btntext[0]=='+'? a_add: btntext[0]=='-'? a_sub:
             btntext[0]=='*'? a_mul: btntext[0]=='/'? a_div: a_result);

    if(btntext[0]=='=')
    {
      int i;
      sprintf(numtext, "%f", calc->lastresult);
      for(i= strlen(numtext)-1; i && (numtext[i]=='0'||numtext[i]=='.'); i--)
        numtext[i]= 0;
      text_settext(calc->idtext, numtext);
    }
  }
}



#undef WIDTH
#undef HEIGHT
#define WIDTH           113
#define HEIGHT          140


void unuso_calc()
{
  // Hauptfenster
  dword idmainwnd= create_rect(NOPARENT, 100,100, WIDTH, 0, COL_WINDOW);
  calc_struct *calc= (calc_struct *)calloc(1, sizeof(calc_struct));
  wnd_prop_add(idmainwnd, "calc_struct", (dword)calc, PROP_DWORD);

  dword frame= clone_frame("titleframe", idmainwnd);
  wnd_prop_set(frame, "title", (dword)"Unus'O'Calc");


  dword btn;

  // AC
  btn= clone_group("button", idmainwnd, 0,4, 24,17, ALIGN_LEFT|ALIGN_TOP);
  wnd_prop_set(btn, "text", (dword)"AC");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);


  dword textframe=
    clone_group("sunkenframe0", idmainwnd, 28,5, 0,15, ALIGN_LEFT|ALIGN_RIGHT);

  create_rect(textframe, 0,0, MAXSCALE,MAXSCALE, 0x000000, WREL|HREL);

  dword idtext= create_text(textframe, 2,0, 1024,13, "0", 0x00FF00, FONT_FIXED);
  calc->idtext= idtext;


  // Nummernbuttons
  dword numpad= create_rect(idmainwnd, 0,32, 7300,0, INVISIBLE,
                            ALIGN_TOP|ALIGN_BOTTOM|WREL);

  for(int y= 0; y<3; y++)
  {
    for(int x= 0; x<3; x++)
    {
      char ch[2];
      btn= clone_group("button", numpad, x*MAXSCALE/3,y*MAXSCALE/4,
                       MAXSCALE/3,MAXSCALE/4, XREL|YREL|WREL|HREL);

      sprintf(ch, "%d", (2-y)*3+x+1);
      wnd_prop_set(btn, "text", (dword)ch);
      wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
    }
  }

  // 0 . =
  btn= clone_group("button", numpad, MAXSCALE*0/3,MAXSCALE*3/4,
                   MAXSCALE/3,MAXSCALE/4, XREL|YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"0");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
  btn= clone_group("button", numpad, MAXSCALE*1/3,MAXSCALE*3/4,
                   MAXSCALE/3,MAXSCALE/4, XREL|YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)".");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
  btn= clone_group("button", numpad, MAXSCALE*2/3,MAXSCALE*3/4,
                   MAXSCALE/3,MAXSCALE/4, XREL|YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"=");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);


  // Operatoren
  dword op_pad= create_rect(idmainwnd, 0,32, MAXSCALE/4,0, INVISIBLE,
                            ALIGN_TOP|ALIGN_BOTTOM|ALIGN_RIGHT|WREL);

  // + - * /
  btn= clone_group("button", op_pad, 0,MAXSCALE*0/4, MAXSCALE,MAXSCALE/4,
                   YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"+");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
  btn= clone_group("button", op_pad, 0,MAXSCALE*1/4, MAXSCALE,MAXSCALE/4,
                   YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"-");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
  btn= clone_group("button", op_pad, 0,MAXSCALE*2/4, MAXSCALE,MAXSCALE/4,
                   YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"*");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);
  btn= clone_group("button", op_pad, 0,MAXSCALE*3/4, MAXSCALE,MAXSCALE/4,
                   YREL|WREL|HREL);
  wnd_prop_set(btn, "text", (dword)"/");
  wnd_prop_set(btn, "on_click", (dword)unuso_calc_btnclick);

  wnd_setisize(idmainwnd, WIDTH, HEIGHT);
}

