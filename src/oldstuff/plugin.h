#ifndef PLUGIN_H
#define PLUGIN_H

#pragma pack(push, 1)
typedef struct 
{
  char *name;
  void (*func);
} _plugin_function;

typedef struct
{
  char *lib_name;
  void *handle;
  _plugin_function functions[1];
} _plugin;

#ifndef PLUGIN_FUNC_PREFIX
#define PLUGIN_FUNC_PREFIX p
#endif

#define DEFINE_PLUGIN(libname)		\
	static char *libname_##libname __attribute__ ((section(".data"))) = #libname;	\
	static void *libhandle_##libname

#define PLUGIN_FUNCTION(type, funcname)	\
	__attribute__ ((section(".data")));	\
	static char *nm_##funcname __attribute__ ((section(".data")))= #funcname;	\
	type (*p##funcname)

#define PLUGIN_FUNCTION_NP(type, funcname)	\
	__attribute__ ((section(".data")));	\
	static char *nm_##funcname __attribute__ ((section(".data")))= #funcname;	\
	type (*funcname)

#define PLUGIN_VARIABLE_NP(type, name, alias)	\
	__attribute__ ((section(".data")));	\
	static char *nm_##name __attribute__ ((section(".data")))= #name;	\
	type *alias

#define END_PLUGIN(libname)		\
	__attribute__ ((section(".data")));	\
	static char *end1_##libname __attribute__ ((section(".data")))= 0;	\
	static char *end2_##libname __attribute__ ((section(".data")))= 0;	\
	_plugin *plugin_##libname __attribute__ ((section(".data")))= (_plugin *) &libname_##libname;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

int load_plugin(_plugin *plugin, char *libname);
void unload_plugin(_plugin *plugin);

#ifdef __cplusplus
}
#endif
  
#endif // PLUGIN_H
