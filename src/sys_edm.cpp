#include <stdio.h>
#include <SDL.h>
#include "flux.h"
#include "sys.h"

void sys_msleep(int msec)
{
    SDL_Delay(msec);
}

dword sys_msectime()
{
    return SDL_GetTicks();
}


