#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include "plugin.h"





int load_plugin(_plugin *plugin, char *libname)
{
  void *handle;
  char *filename;
  char *libpaths[]= { ".", "/lib", "/usr/lib", "/usr/local/lib", 0 };
  int i;
  int tls_error= 0;
  
  // Wenn es schon geladen ist, zurückkehren
  if(plugin->handle)
    return 1;
  
  // wenn kein alternativer lib name angegeben ist, default benutzen
  if(libname==0)
    libname= (char*)(plugin->lib_name);
  
  filename= (char*)alloca(strlen(libname)+64);
  
  // evtl. backslashes durch fwd-slashes ersetzen
  for(i= 0; libname[i]; i++)
    if(libname[i]=='\\') libname[i]= '/';
  
  for(i= 0; libpaths[i]; i++)
  {
    char *dlerr;
    
#define RTLD_MODE	RTLD_NOW|RTLD_GLOBAL
    // zuerst X probieren
    sprintf(filename, "%s/%s", libpaths[i], libname);
    if( (handle= dlopen(filename, RTLD_MODE)) ) break;
    dlerr= dlerror();
    if(strstr(dlerr, "TLS data")) tls_error++;
    if(!strstr(dlerr, "No such file or directory")) printf("%s\n", dlerr);
    
    // dann X.so
    sprintf(filename, "%s/%s.so", libpaths[i], libname);
    if( (handle= dlopen(filename, RTLD_MODE)) ) break;
    dlerr= dlerror();
    if(strstr(dlerr, "TLS data")) tls_error++;
    if(!strstr(dlerr, "No such file or directory")) printf("%s\n", dlerr);
    
    // dann libX.so
    sprintf(filename, "%s/lib%s.so", libpaths[i], libname);
    if( (handle= dlopen(filename, RTLD_MODE)) ) break;
    dlerr= dlerror();
    if(strstr(dlerr, "TLS data")) tls_error++;
    if(!strstr(dlerr, "No such file or directory")) printf("%s\n", dlerr);
  }
  
  
  // Datei nicht gefunden
  if(!handle)
  {
    printf(" *** couldn't dlopen the %s library", libname);
    if(tls_error)
      printf(".\n *** This is apparently a problem with libc;\n"
	     " *** try removing or renaming the directory /usr/lib/tls.\n");
    else
    {
      printf("\n *** (Nothing found in");
      for(i= 0; libpaths[i]; i++) printf(" %s", libpaths[i]);
      printf(")\n");
    }
    fflush(stdout);
    return 0;
  }
  
  // Versuchen, alle Funktionen der Reihe nach zu laden. 
  // Zuerst ohne, dann mit Unterstrich probieren
  for(i= 0; plugin->functions[i].name!=0; i++)
  {
    void *func= dlsym(handle, plugin->functions[i].name);

    if(!func)
    {
      char ch[strlen(plugin->functions[i].name)+1];
      
      sprintf(ch, "_%s", plugin->functions[i].name);
      if( !(func= dlsym(handle, ch)) )
      {
	printf("function %s not found: %s\n", plugin->functions[i].name, dlerror());
	dlclose(handle);
	return 0;
      }
    }
    
    //~ printf("%s = %08X\n", plugin->functions[i].name, func);
    plugin->functions[i].func= func;
  }
  
  plugin->handle= handle;
  
  printf("Lib %s: %d functions bound to %s.\n", plugin->lib_name, i, filename);
  
  return 1;
}



void unload_plugin(_plugin *plugin)
{
  if(!plugin->handle)
    return;
  dlclose(plugin->handle);
}



