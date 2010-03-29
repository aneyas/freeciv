/**********************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "toluaxx.h"

#include "astring.h"
#include "log.h"
#include "registry.h"

#include "api_gen.h"
#include "api_types.h"
#include "script_signal.h"

#include "script.h"

/**************************************************************************
  Lua virtual machine state.
**************************************************************************/
static lua_State *state;

/**************************************************************************
  Optional game script code (useful for scenarios).
**************************************************************************/
static char *script_code;


/**************************************************************************
  Unsafe Lua builtin symbols that we to remove access to.

  If Freeciv's Lua version changes, you have to check how the set of
  unsafe functions and modules changes in the new version. Update the list of
  loaded libraries in script_lualibs, then update the unsafe symbols blacklist
  in script_unsafe_symbols.

  Once the variables are updated for the new version, update the value of
  SCRIPT_SECURE_LUA_VERSION

  In general, unsafe is all functionality that gives access to:
  * Reading files and running processes
  * Loading lua files or libraries
**************************************************************************/
#define SCRIPT_SECURE_LUA_VERSION 501

static const char *script_unsafe_symbols[] = {
  "dofile",
  "loadfile",
  NULL
};

#if LUA_VERSION_NUM != SCRIPT_SECURE_LUA_VERSION
#warning "The script runtime's unsafe symbols information is not up to date."
#warning "This can be a big security hole!"
#endif

/**************************************************************************
  Lua libraries to load (all default libraries, excluding operating system
  and library loading modules). See linit.c in Lua 5.1 for the default list.
**************************************************************************/
static luaL_Reg script_lualibs[] = {
  /* Using default libraries excluding: package, io and os */
  {"", luaopen_base},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};

/**************************************************************************
  Report a lua error.
**************************************************************************/
static int script_report(lua_State *L, int status, const char *code)
{
  if (status) {
    struct astring str = ASTRING_INIT;
    const char *msg;
    int lineno;

    if (!(msg = lua_tostring(L, -1))) {
      msg = "(error with no message)";
    }

    /* Add error message. */
    astr_add_line(&str, "lua error:");
    astr_add_line(&str, "\t%s", msg);

    if (code) {
      /* Add lines around the place the parse error is. */
      if (sscanf(msg, "%*[^:]:%d:", &lineno) == 1) {
	const char *begin, *end;
	int i;

	astr_add(&str, "\n");

	i = 1;
	for (begin = code; *begin != '\0';) {
	  int len;

	  end = strchr(begin, '\n');
	  if (end) {
	    len = end - begin;
	  } else {
	    len = strlen(begin);
	  }

	  if (abs(lineno - i) <= 3) {
	    const char *indicator;

	    indicator = (lineno == i) ? "-->" : "   ";

	    astr_add_line(&str, "\t%s%3d:\t%*.*s",
		indicator, i, len, len, begin);
	  }

	  i++;

	  if (end) {
	    begin = end + 1;
	  } else {
	    break;
	  }
	}

	astr_add(&str, "\n");
      }
    }

    log_error("%s", astr_str(&str));

    astr_free(&str);

    lua_pop(L, 1);
  }

  return status;
}

/**************************************************************************
  Evaluate a Lua function call or loaded script on the stack.
  Return nonzero if an error occured.

  If available pass the source code string as code, else NULL.
  Will handle Lua errors by printing error description and traceback.
**************************************************************************/
static int script_call(lua_State *L, int narg, int nret, const char *code)
{
  int status;
  int base = lua_gettop(L) - narg;  /* Function index */

  /* stack the debug.traceback function */
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  lua_insert(L, base);
  lua_pop(L, 1); /* Pop debug table */

  status = lua_pcall(L, narg, nret, base);
  if (status) {
    script_report(state, status, code);
  }
  lua_remove(L, base);  /* Remove traceback function */
  return status;
}

/**************************************************************************
  lua_dostring replacement with error message showing on errors.
**************************************************************************/
static int script_dostring(lua_State *L, const char *str, const char *name)
{
  int status;

  status = luaL_loadbuffer(L, str, strlen(str), name);
  if (status) {
    script_report(L, status, str);
  } else {
    status = script_call(L, 0, 0, str);
  }
  return status;
}

/**************************************************************************
  Parse and execute the script at filename.
**************************************************************************/
bool script_do_file(const char *filename)
{
  int status;

  status = luaL_loadfile(state, filename);
  if (status) {
    script_report(state, status, NULL);
  } else {
    status = script_call(state, 0, 0, NULL);
  }
  return (status == 0);
}


/**************************************************************************
  Internal api error function.
  Invoking this will cause Lua to stop executing the current context and
  throw an exception, so to speak.
**************************************************************************/
int script_error(const char *fmt, ...)
{
  va_list argp;

  va_start(argp, fmt);
  luaL_where(state, 1);
  lua_pushvfstring(state, fmt, argp);
  va_end(argp);
  lua_concat(state, 2);

  return lua_error(state);
}

/**************************************************************************
  Push callback arguments into the Lua stack.
**************************************************************************/
static void script_callback_push_args(int nargs, va_list args)
{
  int i;

  for (i = 0; i < nargs; i++) {
    int type;

    type = va_arg(args, int);

    switch (type) {
      case API_TYPE_INT:
	{
	  int arg;

	  arg = va_arg(args, int);
	  tolua_pushnumber(state, (lua_Number)arg);
	}
	break;
      case API_TYPE_BOOL:
	{
	  int arg;

	  arg = va_arg(args, int);
	  tolua_pushboolean(state, (bool)arg);
	}
	break;
      case API_TYPE_STRING:
	{
	  const char *arg;

	  arg = va_arg(args, const char*);
	  tolua_pushstring(state, arg);
	}
	break;
      default:
	{
	  const char *name;
	  void *arg;

	  name = get_api_type_name(type);
          fc_assert_ret(NULL != name);

	  arg = va_arg(args, void*);
	  tolua_pushusertype(state, arg, name);
	}
	break;
    }
  }
}

/**************************************************************************
  Invoke the 'callback_name' Lua function.
**************************************************************************/
bool script_callback_invoke(const char *callback_name,
			    int nargs, va_list args)
{
  bool stop_emission = FALSE;

  /* The function name */
  lua_getglobal(state, callback_name);

  if (!lua_isfunction(state, -1)) {
    log_error("lua error: Unknown callback '%s'", callback_name);
    lua_pop(state, 1);
    return FALSE;
  }

  script_callback_push_args(nargs, args);

  /* Call the function with nargs arguments, return 1 results */
  if (script_call(state, nargs, 1, NULL)) {
    return FALSE;
  }

  /* Shall we stop the emission of this signal? */
  if (lua_isboolean(state, -1)) {
    stop_emission = lua_toboolean(state, -1);
  }
  lua_pop(state, 1);   /* pop return value */

  return stop_emission;
}

/**************************************************************************
  Initialize the game script variables.
**************************************************************************/
static void script_vars_init(void)
{
  /* nothing */
}

/**************************************************************************
  Free the game script variables.
**************************************************************************/
static void script_vars_free(void)
{
  /* nothing */
}

/**************************************************************************
  Load the game script variables in file.
**************************************************************************/
static void script_vars_load(struct section_file *file)
{
  if (state) {
    const char *vars;
    const char *section = "script.vars";

    vars = secfile_lookup_str_default(file, "", "%s", section);
    script_dostring(state, vars, section);
  }
}

/**************************************************************************
  Save the game script variables to file.
**************************************************************************/
static void script_vars_save(struct section_file *file)
{
  if (state) {
    lua_getglobal(state, "_freeciv_state_dump");
    if (script_call(state, 0, 1, NULL) == 0) {
      const char *vars;

      vars = lua_tostring(state, -1);
      lua_pop(state, 1);

      if (vars) {
        secfile_insert_str_noescape(file, vars, "script.vars");
      }
    } else {
      /* _freeciv_state_dump in api.pkg is busted */
      log_error("lua error: Failed to dump variables");
    }
  }
}

/**************************************************************************
  Initialize the optional game script code (useful for scenarios).
**************************************************************************/
static void script_code_init(void)
{
  script_code = NULL;
}

/**************************************************************************
  Free the optional game script code (useful for scenarios).
**************************************************************************/
static void script_code_free(void)
{
  if (script_code) {
    free(script_code);
    script_code = NULL;
  }
}

/**************************************************************************
  Load the optional game script code from file (useful for scenarios).
**************************************************************************/
static void script_code_load(struct section_file *file)
{
  if (!script_code) {
    const char *code;
    const char *section = "script.code";

    code = secfile_lookup_str_default(file, "", "%s", section);
    script_code = fc_strdup(code);
    script_dostring(state, script_code, section);
  }
}

/**************************************************************************
  Save the optional game script code to file (useful for scenarios).
**************************************************************************/
static void script_code_save(struct section_file *file)
{
  if (script_code) {
    secfile_insert_str_noescape(file, script_code, "script.code");
  }
}

/**************************************************************************
  Open lua libraries in the array of library definitions in llib.
**************************************************************************/
static void script_openlibs(lua_State *L, const luaL_Reg *llib)
{
  for (; llib->func; llib++) {
    lua_pushcfunction(L, llib->func);
    lua_pushstring(L, llib->name);
    lua_call(L, 1, 0);
  }
}

/**************************************************************************
  Remove global symbols from lua state L
**************************************************************************/
static void script_blacklist(lua_State *L, const char *lsymbols[])
{
  int i;

  for (i = 0; lsymbols[i] != NULL; i++) {
    lua_pushnil(L);
    lua_setglobal(L, lsymbols[i]);
  }
}

/**************************************************************************
  Initialize the scripting state.
**************************************************************************/
bool script_init(void)
{
  if (!state) {
    state = lua_open();
    if (!state) {
      return FALSE;
    }

    script_openlibs(state, script_lualibs);
    script_blacklist(state, script_unsafe_symbols);

    tolua_api_open(state);

    script_code_init();
    script_vars_init();

    script_signals_init();

    return TRUE;
  } else {
    return FALSE;
  }
}

/**************************************************************************
  Free the scripting data.
**************************************************************************/
void script_free(void)
{
  if (state) {
    script_code_free();
    script_vars_free();

    script_signals_free();

    lua_gc(state, LUA_GCCOLLECT, 0); /* Collected garbage */
    lua_close(state);
    state = NULL;
  }
}

/**************************************************************************
  Load the scripting state from file.
**************************************************************************/
void script_state_load(struct section_file *file)
{
  script_code_load(file);

  /* Variables must be loaded after code is loaded and executed,
   * so we restore their saved state properly */
  script_vars_load(file);
}

/**************************************************************************
  Save the scripting state to file.
**************************************************************************/
void script_state_save(struct section_file *file)
{
  script_code_save(file);
  script_vars_save(file);
}

