

extern "C" void setpixeli(int x, int y, dword color, rect clip);


#define dist(a, b) sqrt((a)*(a)+(b)*(b))

void line(int x1,int y1, int x2,int y2, dword color, rect clip)
{
  int xa, ya;
  dword x, y;
  int i, len, col= color;
  //~ int f1, f2;
  //~ int r= (col>>16)&0xFF, g= (col>>8)&0xFF, b= col&0xFF;
  //~ int r2, g2, b2;
  //~ int r3, g3, b3;
  //~ word *v;

  if((x2>>16==x1>>16) && (y2>>16==y1>>16))
    return;

  x= x1;
  y= y1;

  if(abs(x2-x1) > abs(y2-y1))           // X-Major
  {
    xa= (x2-x1>0? 1<<16: -1<<16);
    ya= int( double(y2-y1)*65536 / (double(x2-x1)) );
    if(x2-x1<0) ya= -ya;
    len= abs(x2-x1)>>16;

/*
    for(i= len-1; i>=0; i--)
    {
      f1= (y&0xFFFF);
      f2= 0xFFFF-f1;

      getpixel(x>>16, y>>16, &r2,&g2,&b2);

      r3= r+((r2-r)*f1>>16);
      g3= g+((g2-g)*f1>>16);
      b3= b+((b2-b)*f1>>16);
      setpixel(x>>16,y>>16, r3, g3, b3);

      getpixel(x>>16, (y>>16)+1, &r2,&g2,&b2);

      r3= r+((r2-r)*f2>>16);
      g3= g+((g2-g)*f2>>16);
      b3= b+((b2-b)*f2>>16);
      setpixel(x>>16, (y>>16)+1, r3, g3, b3);

      x+= xa;
      y+= ya;
    }
*/
  }
  else                                  // Y-Major
  {
    xa= int( double(x2-x1) / (double(y2-y1)/65536) );
    if(y2-y1<0) xa= -xa;
    ya= (y2-y1>0? 1: -1) << 16;
    len= abs(y2-y1)>>16;

/*
    for(i= len-1; i>=0; i--)
    {
      f1= (x&0xFFFF);
      f2= 0xFFFF-f1;

      getpixel(x>>16, y>>16, &r2,&g2,&b2);

      r3= r+((r2-r)*f1>>16);
      g3= g+((g2-g)*f1>>16);
      b3= b+((b2-b)*f1>>16);
      setpixel(x>>16,y>>16, r3, g3, b3);

      getpixel((x>>16)+1, y>>16, &r2,&g2,&b2);

      r3= r+((r2-r)*f2>>16);
      g3= g+((g2-g)*f2>>16);
      b3= b+((b2-b)*f2>>16);
      setpixel((x>>16)+1, y>>16, r3, g3, b3);

      x+= xa;
      y+= ya;
    }
*/
  }

  for(i= len-1; i>=0; i--)
  {
    setpixeli(x, y, col, clip);

    x+= xa;
    y+= ya;
  }
}


typedef void (*scanline_cb)(void *arg, int x, int y);

int cb_line(int x1,int y1, int x2,int y2, scanline_cb cb, void *cb_arg)
{
  int xa, ya;
  dword x, y;
  int i, len;
  //~ int f1, f2;
//  pos ret= { x1, y1 };

  if(x2==x1 && y2==y1)
    return 0;

  x= x1;
  y= y1;

  if(abs(x2-x1) > abs(y2-y1))
  {
    xa= (x2-x1>0? 1<<16: -1<<16);
    ya= int( double(y2-y1)*65536 / (double(x2-x1)) );
    if(x2-x1<0) ya= -ya;
    len= abs(x2-x1)>>16;
  }
  else
  {
    ya= (y2-y1>0? 1<<16: -1<<16);
    xa= int( double(x2-x1)*65536 / (double(y2-y1)) );
    if(y2-y1<0) xa= -xa;
    len= abs(y2-y1)>>16;
  }

  for(i= len-1; i>=0; i--)
  {
    cb(cb_arg, x, y);

    x+= xa;
    y+= ya;
  }

//  cb(cb_arg, x, y);

//  ret.x= x; ret.y= y;
//  return ret;
  return len+1;
}


int cb_polyline(int x1,int y1, int x2,int y2,
                scanline_cb cb_left, scanline_cb cb_right, void *cb_arg)
{
  int major= ( abs(y2-y1) > abs(x2-x1) );       // 0: X     1: Y
  int xdir= (x2>x1);                            // 0: Neg   1: Pos
  int ydir= (y2>y1);                            // 0: Neg   1: Pos

  scanline_cb cb;

  if(major)             // Y-Major
  {
    if(!ydir)           // Y-Dir NEG
      x1+= 1<<16, x2+= 1<<16,
      cb= cb_left;      // linke linie
    else
      cb= cb_right;     // rechte linie
  }
  else                  // X-Major
  {
    if(xdir)            // X-Dir POS
      y1+= 1<<16, y2+= 1<<16;

    if(!ydir)
      cb= cb_left;      // linke linie
    else
      cb= cb_right;     // rechte linie
  }

  return cb_line(x1,y1, x2,y2, cb, cb_arg);
}


int cb_ellipse(int cx, int cy, double rx, double ry, scanline_cb cb, void *cb_arg)
{
  double u= max(rx, ry) * M_PI*2;
  int i, x, y;

  for(i= int((u+2)/4); i>=0; i--)
  {
    x= cx + int(cos(M_PI*2*i/u)*rx*0x10000);
    y= cy + int(sin(M_PI*2*i/u)*ry*0x10000);

    cb(cb_arg, x, y);
    cb(cb_arg, cx*2-x, y);
    cb(cb_arg, x, cy*2-y);
    cb(cb_arg, cx*2-x, cy*2-y);
  }

  return int(u+2);
}

void cb_arc(int cx, int cy, double rx, double ry, double w1, double w2,
            bool do_lines, scanline_cb cb, void *cb_arg)
{
  double u= max(fabs(rx), fabs(ry)) * fabs(w2-w1);
  int i, x, y;

  if(do_lines)
  {
    x= cx + int(sin((w2-w1)+fabs(w1))*rx*0xFFFF);
    y= cy + int(cos((w2-w1)+fabs(w1))*ry*0xFFFF);
    cb_line(x,y, cx,cy, cb,cb_arg);
  }

  for(i= int(u); i>=0; i--)
  {
    x= cx + int(sin((w2-w1)*i/u+fabs(w1))*rx*0xFFFF);
    y= cy + int(cos((w2-w1)*i/u+fabs(w1))*ry*0xFFFF);

    cb(cb_arg, x, y);
  }

  if(do_lines)
  {
    x= cx + int(sin(fabs(w1))*rx*0xFFFF);
    y= cy + int(cos(fabs(w1))*ry*0xFFFF);
    cb_line(x,y, cx,cy, cb,cb_arg);
  }
}




/*
bool clip_line(int x1, int y1, int x2, int y2, rect clp, pos *_p1, pos *_p2)
{
  dword out1, out2;

  out1=  ( dword(clp.btm-y1)>>31 )<<0;
  out1|= ( dword(y1-clp.y)>>31   )<<1;
  out1|= ( dword(clp.rgt-x1)>>31 )<<2;
  out1|= ( dword(x1-clp.x)>>31   )<<3;

  out2=  ( dword(clp.btm-y2)>>31 )<<0;
  out2|= ( dword(y2-clp.y)>>31   )<<1;
  out2|= ( dword(clp.rgt-x2)>>31 )<<2;
  out2|= ( dword(x2-clp.x)>>31   )<<3;

  video.gl_currentcontext= 1;
  gl_setcontext(video.gl_vga_context);

  printf( // 8,video.btm-24,
            "out1: %c%c%c%c\n", ((out1>>3)&1)+'0', ((out1>>2)&1)+'0',
                              ((out1>>1)&1)+'0', ((out1>>0)&1)+'0');
  printf( // 8,video.btm-16,
            "out2: %c%c%c%c\n", ((out2>>3)&1)+'0', ((out2>>2)&1)+'0',
                              ((out2>>1)&1)+'0', ((out2>>0)&1)+'0');

  if(!(out1|out2))
  {
    printf( // 8,video.btm-8,
           "accept\n");
    _p1->x= x1<<16; _p1->y= y1<<16;
    _p2->x= x2<<16; _p2->y= y2<<16;
    return true;
  }

  else if(out1&out2)
  {
    printf( // 8,video.btm-8,
           "reject\n");
    return false;
  }

  else
  {
    printf( // 8,video.btm-8,
           "check \n");
    pos p1= { x1<<16, y1<<16 }, p2= { x2<<16, y2<<16 };
    pos ptmp;

    while(1)
    {
      printf( // 8,video.btm-24,
            "out1: %c%c%c%c\n", ((out1>>3)&1)+'0', ((out1>>2)&1)+'0',
                              ((out1>>1)&1)+'0', ((out1>>0)&1)+'0');
      printf( // 8,video.btm-16,
            "out2: %c%c%c%c\n", ((out2>>3)&1)+'0', ((out2>>2)&1)+'0',
                              ((out2>>1)&1)+'0', ((out2>>0)&1)+'0');

      if(out1&(1<<1))           // 1. Punkt oberhalb
      {
        if(y2==y1) return false;
        p1.x= (x2<<16) - ((x2-x1<<16) * (y2-clp.y) / (y2-y1));
        p1.y= clp.y<<16;
      }
      else if(out1&(1<<0))      // 1. Punkt unterhalb
      {
        if(y2==y1) return false;
        p1.x= (x2-x1<<16) * (clp.btm-y1) / (y2-y1) + (x1<<16);
        p1.y= clp.btm<<16;
      }
      else if(out1&(1<<2))      // 1. Punkt rechts
      {
        if(x2==x1) return false;
        p1.x= clp.rgt<<16;
        p1.y= (y2-y1<<16) * (clp.rgt-x1) / (x2-x1) + (y1<<16);
      }
      else if(out1&(1<<3))      // 1. Punkt links
      {
        if(x2==x1) return false;
        p1.x= clp.x<<16;
        p1.y= (y2-y1<<16) * (clp.x-x1) / (x2-x1) + (y1<<16);
      }

      else if(out2&(1<<1))      // 2. Punkt oberhalb
      {
        if(y2==y1) return false;
        p2.x= (x2<<16) - (x2-x1<<16) * (y2-clp.y) / (y2-y1);
        p2.y= clp.y;
      }
      else if(out2&(1<<0))      // 2. Punkt unterhalb
      {
        if(y2==y1) return false;
        p2.x= (x2-x1<<16) * (clp.btm-y1) / (y2-y1) + (x1<<16);
        p2.y= clp.btm<<16;
      }
      else if(out2&(1<<2))      // 2. Punkt rechts
      {
        if(x2==x1) return false;
        p2.x= clp.rgt<<16;
        p2.y= (y2-y1<<16) * (clp.rgt-x1) / (x2-x1) + (y1<<16);
      }
      else if(out2&(1<<3))      // 2. Punkt links
      {
        if(x2==x1) return false;
        p2.x= clp.x<<16;
        p2.y= (y2-y1<<16) * (clp.x-x1) / (x2-x1) + (y1<<16);
      }

#if 0
      out1=  ( dword((clp.btm<<16)-p1.y)>>31 )<<0;
      out1|= ( dword(p1.y-(clp.y<<16))>>31   )<<1;
      out1|= ( dword((clp.rgt<<16)-p1.x)>>31 )<<2;
      out1|= ( dword(p1.x-(clp.x<<16))>>31   )<<3;

      out2=  ( dword((clp.btm<<16)-p2.y)>>31 )<<0;
      out2|= ( dword(p2.y-(clp.y<<16))>>31   )<<1;
      out2|= ( dword((clp.rgt<<16)-p2.x)>>31 )<<2;
      out2|= ( dword(p2.x-(clp.x<<16))>>31   )<<3;
#endif

      out1=  ( dword(clp.btm-(p1.y>>16))>>31 )<<0;
      out1|= ( dword((p1.y>>16)-clp.y)>>31   )<<1;
      out1|= ( dword(clp.rgt-(p1.x>>16))>>31 )<<2;
      out1|= ( dword((p1.x>>16)-clp.x)>>31   )<<3;

      out2=  ( dword(clp.btm-(p2.y>>16))>>31 )<<0;
      out2|= ( dword((p2.y>>16)-clp.y)>>31   )<<1;
      out2|= ( dword(clp.rgt-(p2.x>>16))>>31 )<<2;
      out2|= ( dword((p2.x>>16)-clp.x)>>31   )<<3;


      if(!(out1|out2))
      {
        *_p1= p1, *_p2= p2;
        return true;
      }

      else if(out1&out2)
        return false;
    }
  }
}
*/



