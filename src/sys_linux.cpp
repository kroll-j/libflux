#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "flux.h"
#include "sys.h"

void sys_msleep(int msec)
{
  struct timespec ts_req=
  {
    msec / 1000000000,
    msec * 1000000 % 1000000000
  };
  struct timespec ts_rem;
  int err;

  if( nanosleep(&ts_req, &ts_rem) )
  {
    if(errno==EINVAL)
      perror("nanosleep");
    else if(errno==EINTR)
      fprintf(stderr, "nanosleep: interrupted by signal\n"),
      exit(1);
  }
}


dword sys_msectime()
{
  struct timeval tm;

  gettimeofday(&tm, 0);
  return tm.tv_sec*1000 + tm.tv_usec/1000;
}


