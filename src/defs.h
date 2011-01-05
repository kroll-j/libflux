#ifndef DEFS_H
#define DEFS_H

#if defined FLUX_EDM

extern bool gui_active;

#elif defined FLUX_NDS

#define dbgi(txt...) iprintf("I]" txt)
#define dbgw(txt...) iprintf("W]" txt)
#define logmsg iprintf
#define gui_active true

#else

#define dbgi(txt...) printf("I]" txt)
#define dbgw(txt...) printf("W]" txt)
#define logmsg printf
#define gui_active true

#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;

typedef unsigned long ulong;

#endif

