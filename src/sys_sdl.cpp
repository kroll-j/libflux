#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>
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
