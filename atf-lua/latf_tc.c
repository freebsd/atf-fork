/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atf-c/defs.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "atf-lua/latf.h"

const char *tc_executing;
const char *tc_method_executing;

static int latf_tc_ident(lua_State *L);
static int latf_tc_stub_body(lua_State *L);

static const char *latf_tc_inherited[] = {
    "head",
    "body",
    "cleanup",
};

/*
 * execcb is called with a testcase key, value as last two items on the stack.
 * The callback can return non-zero to indicate "bail out immediately" with
 * whatever state the callback has left the stack in.
 */
int
latf_tc_foreach(lua_State *L, latf_tc_foreach_cb execcb, void *data)
{
    int error;

    lua_pushstring(L, ATF_GLOBAL_PROP_TCS);
    lua_gettable(L, LUA_REGISTRYINDEX);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if ((error = execcb(L, data)) != 0)
            return (error);

        /* Pop the value. */
        lua_pop(L, 1);
    }
    /* Pop the globaltable. */
    lua_pop(L, 1);
    return (0);
}

/* Call head/body/cleanup, tc is at -1. */
static void
latf_tc_method(lua_State *L, const char *tc_ident, const char *method)
{

    tc_executing = tc_ident;
    tc_method_executing = method;
    lua_pushstring(L, method);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        tc_executing = tc_method_executing = NULL;
        if (strcmp(method, "cleanup") == 0)
            /* Just return success; cleanup is optional. */
            return;
        else
            latf_tc_stub_body(L);
    }

    /*
     * This could have been a pcall, but we let errors jump all the way back up
     * to our panic handler instead.  This is much less complicated than trying
     * to deal with errors at this level, because we generally cannot
     * effectively do so.
     */
    lua_call(L, 0, 0);

    tc_executing = tc_method_executing = NULL;
}

static int
latf_tc_list_cb(lua_State *L, void *data)
{
    int *counter;
    const char *ident;

    counter = data;
    lua_pushstring(L, ATF_PROP_IDENT);
    lua_gettable(L, -2);
    ident = lua_tostring(L, -1);
    lua_pushvalue(L, -2);
    /* If it fails, we'll bail out to the panic handler. */
    latf_tc_method(L, ident, "head");
    printf("\n");
    printf("ident: %s\n", ident);
    lua_pushstring(L, ATF_PROP_VARS);
    lua_gettable(L, -2);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        printf("%s: %s\n", lua_tostring(L, -2),
            lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 3);
    *counter += 1;
    return (0);
}

void
latf_tc_list(lua_State *L)
{
    int counter;

    counter = 0;
    printf("Content-Type: application/X-atf-tp; version=\"1\"\n");
    /* Bubbles up any errors. */
    latf_tc_foreach(L, latf_tc_list_cb, &counter);
    if (counter == 0)
        printf("\n");
}

static int
latf_tc_get_cb(lua_State *L, void *data)
{
    const char *check_name, *tc_name;
    int comp;

    check_name = data;
    lua_pushstring(L, ATF_PROP_IDENT);
    lua_gettable(L, -2);
    tc_name = lua_tostring(L, -1);
    comp = (strcmp(tc_name, check_name) == 0);

    lua_pop(L, 1);
    return (comp);
}

/*
 * latf_tc_get will enumerate the registered test cases, in search of one with
 * an ident string matching name.  If found, the latf_tc_get callback will
 * indicate to latf_tc_foreach that it should stop, leaving the expected test at
 * the top of stack for the latf_tc_get caller to use.  If it's not found, the
 * stack should be in the same state that it was in when we entered latf_tc_get.
 */
bool
latf_tc_get(lua_State *L, const char *name)
{

    /*
     * data is opaque to latf_enumerate_testcases_exec and cb won't deref
     * it.
     */
    return (latf_tc_foreach(L, latf_tc_get_cb, __DECONST(char *, name)) != 0);
}

/*
 * This is the final piece of constructing an individual test case.  We've gone
 * through latf_tc_ident and now we've invoked the closure that's returned from
 * that, with ident and a table to inherit from (atf.TestCase by default).  Note
 * that we don't inherit many values from the parent, mainly head/body/cleanup.
 *
 * latf_tc_new will also make the __call method of this new table effectively
 * the closure that we started this chain of events with, only the table to
 * inherit from will be the very table that we're constructing here!  The return
 * value can then be used to create more tests cases in an identical fashion,
 * ad infinitum.
 */
static int
latf_tc_new(lua_State *L)
{
    int inherit, nargs;
    bool doreg;

    nargs = lua_gettop(L);
    luaL_argcheck(L, nargs > 0, 1, "not enough arguments");
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_settop(L, 1);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setfield(L, -2, ATF_PROP_IDENT);

    inherit = lua_upvalueindex(2);
    if (!lua_isnil(L, inherit)) {
        for (size_t i = 0; i < nitems(latf_tc_inherited); ++i) {
            const char *field;
            bool doinherit;

            field = latf_tc_inherited[i];
            lua_pushstring(L, field);
            lua_gettable(L, -2);

            /*
             * We only inherit a field if an override isn't
             * provided in the new test case definition.
             */
            doinherit = lua_isnil(L, -1);
            lua_pop(L, 1);
            if (doinherit) {
                lua_pushstring(L, field);
                lua_gettable(L, inherit);
                lua_setfield(L, -2, field);
            }
        }
    }

    /* _atf_tc = true */
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, ATF_PROP_TC);

    /*
     * _atf_vars = table, and we'll set _atf_vars[has.cleanup] if cleanup
     * is callable so that kyua knows to execute :cleanup.
     */
    lua_newtable(L);
    lua_pushstring(L, "cleanup");
    lua_gettable(L, -3);
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, "true");
        lua_setfield(L, -3, "has.cleanup");
    }
    lua_pop(L, 1);
    lua_setfield(L, -2, ATF_PROP_VARS);

    /* Push the __call closure back for inheritance usage. */
    lua_newtable(L);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, latf_tc_ident, 1);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    /* Make sure atf_auto is in order; if it's not set, it's true. */
    lua_pushstring(L, ATF_PROP_AUTO);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        doreg = true;
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, ATF_PROP_AUTO);
    } else {
        doreg = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    if (doreg) {
        const char *ident;

        ident = lua_tostring(L, lua_upvalueindex(1));
        if (latf_tc_get(L, ident)) {
            lua_pop(L, 1);
            return (luaL_error(L, "double registered '%s'", ident));
        }
        /* Grab registry[ATF_GLOBAL_PROP_TCS] */
        lua_pushstring(L, ATF_GLOBAL_PROP_TCS);
        lua_gettable(L, LUA_REGISTRYINDEX);

        /* Push test */
        lua_pushinteger(L, lua_rawlen(L, -1) + 1);
        lua_pushvalue(L, -3);
        lua_settable(L, -3);
        lua_setfield(L, LUA_REGISTRYINDEX, ATF_GLOBAL_PROP_TCS);
    }

    /* Return the table. */
    return (1);
}

static int
latf_tc_ident(lua_State *L)
{

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "not enough arguments");
    lua_settop(L, 2);

    lua_pushvalue(L, lua_upvalueindex(1));
    /* ident, inherit as upvalues */
    lua_pushcclosure(L, latf_tc_new, 2);
    return (1);
}

/*
 * We provide these three default functions for atf.TestCase.  The provided
 * head() is harmless, but the default body() will effectively
 * just invoke atf.fail() due to it not being an implemented test.
 */
static int
latf_tc_stub_head(lua_State *L ATF_DEFS_ATTRIBUTE_UNUSED)
{
    return (0);
}

static int
latf_tc_stub_body(lua_State *L)
{

    _latf_fail(L, "Test case not implemented");
    return (0);
}

int
latf_tc_obj(lua_State *L)
{

    /* Entry at the top of the stack should be the lib table. */
    if (!lua_istable(L, -1))
        return (luaL_error(L, "expected a table"));

    /*
     * Construct a callable TestCase object with:
     * - head(): test head
     * - body(): test body
     * - internal props (ident, atf_auto)
     */
    lua_newtable(L);

    /* Test callbacks. */
    lua_pushcfunction(L, latf_tc_stub_head);
    lua_setfield(L, -2, "head");
    lua_pushcfunction(L, latf_tc_stub_body);
    lua_setfield(L, -2, "body");

    lua_pushstring(L, "TestCase");
    lua_setfield(L, -2, ATF_PROP_IDENT);

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, ATF_PROP_AUTO);

    /*
     * Push the TestCase as upvalue #1 to a closure for __call.
     * latf_tc_ident will take a string argument and return another closure
     * from latf_tc_new that's expecting a table to start from.  This allows
     * a fairly pleasant syntax:
     *
     * atf.TestCase "Ident String" { [Test Definition] }
     */
    lua_newtable(L);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, latf_tc_ident, 1);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, "TestCase");

    return (0);
}

/*
 * latf_tc_execute will either return a negative errno or positive number
 * to indicate failure, or 0 on success.  Negative errno is returned for
 * failures in the immediate area, while positive number is returned for errors
 * propagated up to us from the interpreter.
 */
int
latf_tc_execute(lua_State *L, const char *test, const char *method)
{

    if (!latf_tc_get(L, test))
        return (ENOENT);

    /* Errors bubble up. */
    latf_tc_method(L, test, "head");
    latf_tc_method(L, test, method);

    return (0);
}

