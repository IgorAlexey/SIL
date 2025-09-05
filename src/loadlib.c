/*
** $Id: loadlib.c $
** Dynamic library loader for SIL
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define SIL_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sil.h"

#include "lauxlib.h"
#include "sillib.h"
#include "llimits.h"


/*
** SIL_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** SIL_LSUBSEP is the character that replaces dots in submodule names
** when searching for a SIL loader.
*/
#if !defined(SIL_CSUBSEP)
#define SIL_CSUBSEP		SIL_DIRSEP
#endif

#if !defined(SIL_LSUBSEP)
#define SIL_LSUBSEP		SIL_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define SIL_POF		"silopen_"

/* separator for open functions in C libraries */
#define SIL_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/* cast void* to a SIL function */
#define cast_Lfunc(p)	cast(sil_CFunction, cast_func(p))


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (sil_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static sil_CFunction lsys_sym (sil_State *L, void *lib, const char *sym);




#if defined(SIL_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface,
** which is available in all POSIX systems.
** =========================================================================
*/

#include <dlfcn.h>


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (sil_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    sil_pushstring(L, dlerror());
  return lib;
}


static sil_CFunction lsys_sym (sil_State *L, void *lib, const char *sym) {
  sil_CFunction f = cast_Lfunc(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    sil_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(SIL_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(SIL_LLE_FLAGS)
#define SIL_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of SIL_EXEC_DIR with the executable's path.
*/
static void setprogdir (sil_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    silL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    silL_gsub(L, sil_tostring(L, -1), SIL_EXEC_DIR, buff);
    sil_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (sil_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    sil_pushstring(L, buffer);
  else
    sil_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (sil_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, SIL_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static sil_CFunction lsys_sym (sil_State *L, void *lib, const char *sym) {
  sil_CFunction f = cast_Lfunc(GetProcAddress((HMODULE)lib, sym));
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your SIL installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (sil_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  sil_pushliteral(L, DLMSG);
  return NULL;
}


static sil_CFunction lsys_sym (sil_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  sil_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** SIL_PATH_VAR and SIL_CPATH_VAR are the names of the environment
** variables that SIL check to set its paths.
*/
#if !defined(SIL_PATH_VAR)
#define SIL_PATH_VAR    "SIL_PATH"
#endif

#if !defined(SIL_CPATH_VAR)
#define SIL_CPATH_VAR   "SIL_CPATH"
#endif



/*
** return registry.SIL_NOENV as a boolean
*/
static int noenv (sil_State *L) {
  int b;
  sil_getfield(L, SIL_REGISTRYINDEX, "SIL_NOENV");
  b = sil_toboolean(L, -1);
  sil_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path. (If using the default path, assume it is a string
** literal in C and create it as an external string.)
*/
static void setpath (sil_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = sil_pushfstring(L, "%s%s", envname, SIL_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    sil_pushexternalstring(L, dft, strlen(dft), NULL, NULL);  /* use default */
  else if ((dftmark = strstr(path, SIL_PATH_SEP SIL_PATH_SEP)) == NULL)
    sil_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    silL_Buffer b;
    silL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      silL_addlstring(&b, path, ct_diff2sz(dftmark - path));  /* add it */
      silL_addchar(&b, *SIL_PATH_SEP);
    }
    silL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      silL_addchar(&b, *SIL_PATH_SEP);
      silL_addlstring(&b, dftmark + 2, ct_diff2sz((path + len - 2) - dftmark));
    }
    silL_pushresult(&b);
  }
  setprogdir(L);
  sil_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  sil_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (sil_State *L, const char *path) {
  void *plib;
  sil_getfield(L, SIL_REGISTRYINDEX, CLIBS);
  sil_getfield(L, -1, path);
  plib = sil_touserdata(L, -1);  /* plib = CLIBS[path] */
  sil_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (sil_State *L, const char *path, void *plib) {
  sil_getfield(L, SIL_REGISTRYINDEX, CLIBS);
  sil_pushlightuserdata(L, plib);
  sil_pushvalue(L, -1);
  sil_setfield(L, -3, path);  /* CLIBS[path] = plib */
  sil_rawseti(L, -2, silL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  sil_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (sil_State *L) {
  sil_Integer n = silL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    sil_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(sil_touserdata(L, -1));
    sil_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (sil_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    sil_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    sil_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    sil_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (sil_State *L) {
  const char *path = silL_checkstring(L, 1);
  const char *init = silL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    silL_pushfail(L);
    sil_insert(L, -2);
    sil_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *SIL_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *SIL_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (sil_State *L, const char *path) {
  silL_Buffer b;
  silL_buffinit(L, &b);
  silL_addstring(&b, "no file '");
  silL_addgsub(&b, path, SIL_PATH_SEP, "'\n\tno file '");
  silL_addstring(&b, "'");
  silL_pushresult(&b);
}


static const char *searchpath (sil_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  silL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = silL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  silL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  silL_addgsub(&buff, path, SIL_PATH_MARK, name);
  silL_addchar(&buff, '\0');
  pathname = silL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + silL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return sil_pushstring(L, filename);  /* save and return name */
  }
  silL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, sil_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (sil_State *L) {
  const char *f = searchpath(L, silL_checkstring(L, 1),
                                silL_checkstring(L, 2),
                                silL_optstring(L, 3, "."),
                                silL_optstring(L, 4, SIL_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    silL_pushfail(L);
    sil_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (sil_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  sil_getfield(L, sil_upvalueindex(1), pname);
  path = sil_tostring(L, -1);
  if (l_unlikely(path == NULL))
    silL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (sil_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    sil_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return silL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          sil_tostring(L, 1), filename, sil_tostring(L, -1));
}


static int searcher_Sil (sil_State *L) {
  const char *filename;
  const char *name = silL_checkstring(L, 1);
  filename = findfile(L, name, "path", SIL_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (silL_loadfile(L, filename) == SIL_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "silopen_X" and look for it. (For compatibility, if that
** fails, it also tries "silopen_Y".) If there is no ignore mark,
** look for a function named "silopen_modname".
*/
static int loadfunc (sil_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = silL_gsub(L, modname, ".", SIL_OFSEP);
  mark = strchr(modname, *SIL_IGMARK);
  if (mark) {
    int stat;
    openfunc = sil_pushlstring(L, modname, ct_diff2sz(mark - modname));
    openfunc = sil_pushfstring(L, SIL_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = sil_pushfstring(L, SIL_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (sil_State *L) {
  const char *name = silL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", SIL_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (sil_State *L) {
  const char *filename;
  const char *name = silL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  sil_pushlstring(L, name, ct_diff2sz(p - name));
  filename = findfile(L, sil_tostring(L, -1), "cpath", SIL_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      sil_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  sil_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (sil_State *L) {
  const char *name = silL_checkstring(L, 1);
  sil_getfield(L, SIL_REGISTRYINDEX, SIL_PRELOAD_TABLE);
  if (sil_getfield(L, -1, name) == SIL_TNIL) {  /* not found? */
    sil_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    sil_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (sil_State *L, const char *name) {
  int i;
  silL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(sil_getfield(L, sil_upvalueindex(1), "searchers")
                 != SIL_TTABLE))
    silL_error(L, "'package.searchers' must be a table");
  silL_buffinit(L, &msg);
  silL_addstring(&msg, "\n\t");  /* error-message prefix for first message */
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    if (l_unlikely(sil_rawgeti(L, 3, i) == SIL_TNIL)) {  /* no more searchers? */
      sil_pop(L, 1);  /* remove nil */
      silL_buffsub(&msg, 2);  /* remove last prefix */
      silL_pushresult(&msg);  /* create error message */
      silL_error(L, "module '%s' not found:%s", name, sil_tostring(L, -1));
    }
    sil_pushstring(L, name);
    sil_call(L, 1, 2);  /* call it */
    if (sil_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (sil_isstring(L, -2)) {  /* searcher returned error message? */
      sil_pop(L, 1);  /* remove extra return */
      silL_addvalue(&msg);  /* concatenate error message */
      silL_addstring(&msg, "\n\t");  /* prefix for next message */
    }
    else  /* no error message */
      sil_pop(L, 2);  /* remove both returns */
  }
}


static int ll_require (sil_State *L) {
  const char *name = silL_checkstring(L, 1);
  sil_settop(L, 1);  /* LOADED table will be at index 2 */
  sil_getfield(L, SIL_REGISTRYINDEX, SIL_LOADED_TABLE);
  sil_getfield(L, 2, name);  /* LOADED[name] */
  if (sil_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  sil_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  sil_rotate(L, -2, 1);  /* function <-> loader data */
  sil_pushvalue(L, 1);  /* name is 1st argument to module loader */
  sil_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  sil_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!sil_isnil(L, -1))  /* non-nil return? */
    sil_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    sil_pop(L, 1);  /* pop nil */
  if (sil_getfield(L, 2, name) == SIL_TNIL) {   /* module set no value? */
    sil_pushboolean(L, 1);  /* use true as result */
    sil_copy(L, -1, -2);  /* replace loader result */
    sil_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  sil_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const silL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const silL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (sil_State *L) {
  static const sil_CFunction searchers[] = {
    searcher_preload,
    searcher_Sil,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;
  /* create 'searchers' table */
  sil_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    sil_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    sil_pushcclosure(L, searchers[i], 1);
    sil_rawseti(L, -2, i+1);
  }
  sil_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (sil_State *L) {
  silL_getsubtable(L, SIL_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  sil_createtable(L, 0, 1);  /* create metatable for CLIBS */
  sil_pushcfunction(L, gctm);
  sil_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  sil_setmetatable(L, -2);
}


SILMOD_API int silopen_package (sil_State *L) {
  createclibstable(L);
  silL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", SIL_PATH_VAR, SIL_PATH_DEFAULT);
  setpath(L, "cpath", SIL_CPATH_VAR, SIL_CPATH_DEFAULT);
  /* store config information */
  sil_pushliteral(L, SIL_DIRSEP "\n" SIL_PATH_SEP "\n" SIL_PATH_MARK "\n"
                     SIL_EXEC_DIR "\n" SIL_IGMARK "\n");
  sil_setfield(L, -2, "config");
  /* set field 'loaded' */
  silL_getsubtable(L, SIL_REGISTRYINDEX, SIL_LOADED_TABLE);
  sil_setfield(L, -2, "loaded");
  /* set field 'preload' */
  silL_getsubtable(L, SIL_REGISTRYINDEX, SIL_PRELOAD_TABLE);
  sil_setfield(L, -2, "preload");
  sil_pushglobaltable(L);
  sil_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  silL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  sil_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

