#ifndef FLUX_EDM_H
#define FLUX_EDM_H

#ifdef FLUX_EDM

#ifdef FLUX
#include "pch.h"
#include "engine.h"
#define logmsg conoutf
#endif

#ifndef logmsg
#define logmsg printf
#endif

extern int gui_active;
extern void toggleui();

#endif	//FLUX_EDM

#endif	//FLUX_EDM_H
