// Named property list. Uses a sorted list of strings to look up names.
// Properties can be added, set, and queried. User callbacks are called to do any
// desired action on get/set/add events. In the callback, properties can be identified
// either by name (using strcmp) or by an integer switch/case if the property ID is
// known beforehand. This is very fast. handle_default_properties() in fluxif.cpp
// uses this approach. IDs are assigned in the order properties are added.
// Properties are either scalars (values of type prop_t, can also be pointers) or
// strings of arbitrary length. String memory is managed by the property list.


#ifndef PROP_LIST_H
#define PROP_LIST_H
#include "defs.h"

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#include <cstdio>
extern "C" {

#else

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#endif

// Prop-Listen werden in Schritten von PCHUNKSIZE vergrößert
#define PCHUNKSIZE      16

// Property-Typen
#define PROP_DWORD      0
#define PROP_STRING     1

// NOCH NICHT DRIN, waer aber gut
#define PROP_READONLY	(1<<4)
#define PROP_WRITEONLY	(1<<5)
#define PROP_READWRITE	(PROP_RO|PROP_WO)

enum prop_cb_types
{
  PROP_ADD= 1<<0,
  PROP_GET= 1<<1,
  PROP_SET= 1<<2
};

// integer type which is at least as large as a pointer:
// (works for x86 and x86_64; redefine for other platforms if necessary)
typedef unsigned long prop_t;


// Eine property
typedef struct property
{
    char *name;
    char *desc;
    dword id;
    int type;
    union
    {
  	char *pvalue;
	prop_t ivalue;
    };

#ifdef __cplusplus
    char *valtostring(char *buf, int size, prop_t *val= 0)
    {
	if(!val) val= &ivalue;
	if(type==PROP_STRING) strncpy(buf, (char*)(*val), size), buf[size-1]= 0;
	else snprintf(buf, size, "%d", (int)*val), buf[size-1]= 0;
	return buf;
    }
    prop_t valtoint(prop_t *val= 0)
    {
	if(!val) val= &ivalue;
	if(type==PROP_STRING) return strtol((char*)(*val), 0, 0);
	return *val;
    }
    char *tostring(char *buf, int size, prop_t *val= 0)
    {
	int l= snprintf(buf, size, "%s=", name);
	valtostring(buf+l, size-l, val);
	return buf;
    }
#endif

} property;

// Property-Liste
typedef struct prop_list
{
  property **by_id;
  property **by_name;
  int size;
} prop_list;


// "interne" Funktionen
int strcomp3(char *str, char *s1, char *s2);
int props_find_id_pos(prop_list *props, dword id);
int props_find_name_pos(prop_list *props, const char *name);
void props_insert_byid(prop_list *props, property *newprop, int pos);
void props_insert_byname(prop_list *props, property *newprop, int pos);
void props_dump(prop_list *props);

// "globale" Funktionen
int props_init(prop_list *props);
void props_free(prop_list *props);
int props_add(prop_list *props, const char *name, int id, prop_t value, int type, const char *desc= "");
int props_set_byid(prop_list *props, int id, prop_t value);
int props_set_byname(prop_list *props, char *name, prop_t value);
property *props_find_byid(prop_list *props, int id);
property *props_find_byname(prop_list *props, const char *name);
int props_free_byid(prop_list *props, dword id);
int props_free_byname(prop_list *props, char *name);
prop_list *props_clone(prop_list *props);


#ifdef __cplusplus
}

struct proplist
{
    prop_list *p;
    int id_count;
    char *tmpstr;

    typedef prop_t (*callback) (proplist *self, int type, char *name, int id, prop_t value);
    callback cb;	// add/set/get callback
    int cbmask;		// when to call it (bitmask of PROP_ADD, PROP_GET, PROP_SET)

    void clear() { if(p) { props_free(p); p= 0; } if(tmpstr) delete[] tmpstr, tmpstr= 0; id_count= 0; }

    proplist() { p= 0; tmpstr= 0; id_count= 0; cb= 0; }
    ~proplist() { clear(); }
    proplist(proplist &init) { tmpstr= 0; id_count= init.id_count; p= props_clone(init.p); }

    prop_list *newproplist()
    {
	prop_list *p= (prop_list *)calloc(1, sizeof(prop_list));
	props_init(p);
	return p;
    }

    void setcallback(callback cb, int mask)
    { this->cb= cb; cbmask= mask; }

    int size() { return id_count; }

    int add(char *name, prop_t value, int type, char *desc= "")
    {
	if(!p) p= newproplist(), tmpstr= new char[2048];
	if(cb && (cbmask&PROP_ADD)) value= cb(this, PROP_ADD, name, id_count, value);
	return props_add(p, name, id_count++, value, type, desc);
    }

    int add(char *name, char *value, char *desc= "")
    { return add(name, (prop_t)value, PROP_STRING, desc); }

    int add(char *name, prop_t value, char *desc= "")
    { return add(name, (prop_t)value, PROP_DWORD, desc); }


    // set() methods don't do automatic type conversion, correct type is assumed
    template <class T> int set(char *name, T value)
    {
	if(!p) return 0;
	if(cb && (cbmask&PROP_SET))
	{
	    property *prop= props_find_byname(p, name);
	    if(prop) return cb(this, PROP_SET, prop->name, prop->id, (prop_t)value);
	}
	return props_set_byname(p, name, (prop_t)value);
    }
    template <class T> int set(int id, T value)
    {
	if(!p) return 0;
	if(cb && (cbmask&PROP_SET))
	{
	    property *prop= props_find_byid(p, id);
	    if(prop) return cb(this, PROP_SET, prop->name, prop->id, value);
	}
	return props_set_byid(p, id, (prop_t)value);
    }

    // these 2 functions DON'T use the callback
    property *get(const char *name)
    { if(p) return props_find_byname(p, (char*)name); else return 0; }

    property *get(int id)
    { if(p) return props_find_byid(p, id); else return 0; }

    template <class T> char *getstr(T which)
    {
	property *prop= get(which);
	if(prop)
	{
	    if(cb && (cbmask&PROP_GET))
	    {
		prop_t ret= cb(this, PROP_GET, prop->name, prop->id, prop->ivalue);
		return prop->valtostring(tmpstr, 2048, &ret);
	    }
	    else return prop->valtostring(tmpstr, 2048);
	}
	return 0;
    }
    prop_t getint(char *which)
    {
	property *prop= get(which);
	if(prop)
	{
	    if(cb && (cbmask&PROP_GET))
	    {
		prop_t ret= cb(this, PROP_GET, prop->name, prop->id, prop->ivalue);
		return prop->valtoint(&ret);
	    }
	    return prop->valtoint();
	}
	return 0;
    }
};

#endif	// __cplusplus

#endif // PROP_LIST_H

