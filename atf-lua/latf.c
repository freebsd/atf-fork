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
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atf-c/defs.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "atf-lua/latf.h"

#define	_latf_write_result(...)			dprintf(tc_resfile_fd, __VA_ARGS__)
#define	_latf_write_result_ap(fmt, va)	vdprintf(tc_resfile_fd, (fmt), (va))

/* Courtesy of RhodiumToad's lspawn. */
LUA_API int   (lua_error) (lua_State *L) ATF_DEFS_ATTRIBUTE_NORETURN;

int tc_resfile_fd = -1;

/* We can only have one anyways, might as well save the runtime allocation. */
static struct latf_error atferr;
static enum latf_tc_expect {
    TCE_PASS,
    TCE_DEATH,
    TCE_EXIT,
    TCE_FAIL,
    TCE_SIGNAL,
    TCE_TIMEOUT,
} latf_tc_expected = TCE_PASS;
static char *latf_tc_expected_reason;

/*
 * Here we map the various test expectations to the message that we'll provide
 * if the expectation is violated.
 */
static const char *latf_tc_expected_msg[] = {
    [TCE_DEATH] = "Test case was expected to terminate abruptly but it "
        "continued execution",
    [TCE_EXIT] = "Test case was expected to exit cleanly but it continued "
        "execution",
    [TCE_FAIL] = "Test case was expecting a failure but none were raised",
    [TCE_SIGNAL] = "Test case was expected to receive a termination signal "
        "but it continued execution",
    [TCE_TIMEOUT] = "Test case was expected to hang but it continued "
        "execution",
};

/* These two are straight out of which(1). */
static bool
sane_xaccess(const char *candidate)
{
    struct stat st;

    /* work around access(2) false positives for superuser. */
    return (access(candidate, X_OK) == 0 &&
        stat(candidate, &st) == 0 &&
        S_ISREG(st.st_mode) &&
        (getuid() != 0 ||
        (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0));
}

static bool
path_search(const char *prog)
{
    char candidate[PATH_MAX];
    const char *d, *envpath;
    char *path, *opath;
    bool ret;

    ret = false;
    if ((envpath = getenv("PATH")) == NULL)
        return (false);
    if ((opath = path = strdup(envpath)) == NULL)
        return (false);

    while ((d = strsep(&path, ":")) != NULL) {
        if (*d == '\0')
            d = ".";
        if (snprintf(candidate, sizeof(candidate), "%s/%s", d,
            prog) >= (int)sizeof(candidate))
            continue;
        if (sane_xaccess(candidate)) {
            ret = true;
            break;
        }
    }

    free(opath);
    return (ret);
}

/*
 * _latf_bail is our standard mechanism for passing errors back up the chain.
 * It is almost always wrong for latf to call luaL_error/lua_error directly from
 * C that's been called by the loaded lua script.  It's safe to send errors from
 * the outer layer of C that's invoking the lua script, so it's best to just
 * assume that any luaL_error/lua_error usage is wrong.
 */
static void __dead2
_latf_bail(lua_State *L, int exitcode, const char *msg)
{

    atferr.err_msg = msg;
    atferr.err_exitcode = exitcode;
    *(struct latf_error **)lua_newuserdata(L, sizeof(atferr)) =
        &atferr;
    luaL_getmetatable(L, LATF_ERROR_METATABLE);
    lua_setmetatable(L, -2);
    lua_error(L);
}

static int __dead2 __printflike(3, 4)
_latf_error(lua_State *L, int exitcode, const char *fmt, ...)
{
    char *msg;
    va_list ap;

    va_start(ap, fmt);
    if (vasprintf(&msg, fmt, ap) == -1)
        _latf_bail(L, 128, "out of memory while reporting error");
    va_end(ap);

    _latf_bail(L, exitcode, msg);
}

/*
 * _latf_finish is for internal usage by pass/fail/skip below to properly exit
 * the interpreter and bubble up their results.  Basically, _latf_finish must be
 * called whenever we've written out the last line to our results file to make
 * sure we close it properly before we bail out.
 */
static void __dead2
_latf_finish(lua_State *L, int exitcode)
{
    if (tc_resfile_fd >= 0 && tc_resfile_fd != STDOUT_FILENO &&
        tc_resfile_fd != STDIN_FILENO) {
        close(tc_resfile_fd);
        tc_resfile_fd = -1;
    }

    _latf_bail(L, exitcode, NULL);
}

static int __printflike(2, 3)
_latf_skip(lua_State *L, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _latf_write_result("skipped: ");
    _latf_write_result_ap(fmt, ap);
    va_end(ap);

    _latf_finish(L, 0);
}

int __dead2 __printflike(2, 3)
_latf_fail(lua_State *L, const char *fmt, ...)
{
    va_list ap;

    switch (latf_tc_expected) {
    case TCE_FAIL:
        va_start(ap, fmt);
        _latf_write_result("expected_failure: %s: ",
            latf_tc_expected_reason);
        _latf_write_result_ap(fmt, ap);
        _latf_write_result("\n");
        va_end(ap);

        _latf_finish(L, 0);
        break;
    case TCE_PASS:
        va_start(ap, fmt);
        _latf_write_result("failed: ");
        _latf_write_result_ap(fmt, ap);
        _latf_write_result("\n");
        va_end(ap);

        _latf_finish(L, 1);
        break;
    default:
        _latf_error(L, 128, "Unreachable");
        break;
    }
}

static int
_latf_pass(lua_State *L)
{

    switch (latf_tc_expected) {
    case TCE_FAIL:
        latf_tc_expected = TCE_PASS;
        _latf_fail(L, "Test case was expecting a failure but got a "
            "pass instead");
        break;
    case TCE_PASS:
        _latf_write_result("passed\n");
        _latf_finish(L, 0);
        break;
    default:
        _latf_error(L, 128, "Unreachable");
        break;
    }
}

/*
 * _latf_validate_expect is where we fail out if an expectation that isn't pass
 * has been set and we haven't met it.  It should be called by the outer C
 * layer after test execution has completed to finally fail the test if the test
 * was expecting some non-pass scenario.  It must also be called when
 * expectations change for the most part, as a previous non-pass expectation has
 * clearly been violated if we're still executing far enough to reach this,
 * perhaps with exception to timeout.
 */
static void
_latf_validate_expect(lua_State *L)
{
    const char *msg;

    /* If we expected pass, all is good. */
    if (latf_tc_expected == TCE_PASS)
        return;

    /* Otherwise, we've already failed. */
    msg = latf_tc_expected_msg[latf_tc_expected];
    latf_tc_expected = TCE_PASS;
    _latf_fail(L, "%s", msg);
}

/*
 * Thus begins the implementation of our atf.* methods.  Leading underbar marks
 * internal implementations of atf.* methods that may be reused.
 */
static int
latf_get(lua_State *L)
{
    int nargs;

    nargs = lua_gettop(L);
    luaL_argcheck(L, nargs > 0, 1, "not enough arguments");
    luaL_checktype(L, 1, LUA_TSTRING);

    if (!latf_tc_get(L, tc_executing))
        _latf_error(L, 128, "atf.set called in invalid test");

    lua_pushstring(L, ATF_PROP_VARS);
    lua_gettable(L, -2);
    /* Key to fetch */
    lua_pushvalue(L, 1);
    lua_gettable(L, -2);
    return (1);
}

static int
latf_set(lua_State *L)
{

    if (strcmp(tc_method_executing, "head") != 0)
        _latf_error(L, 128,
            "atf.set called from the test case's body");
    if (lua_gettop(L) != 2)
        _latf_error(L, 128,
            "atf.set takes two args: key, value strings");

    /* Summon up the test case props. */
    if (!latf_tc_get(L, tc_executing))
        _latf_error(L, 128, "atf.set called in invalid test");

    lua_pushstring(L, ATF_PROP_VARS);
    lua_gettable(L, -2);

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_settable(L, -3);

    return (0);
}

static int
latf_check_equal(lua_State *L)
{

    if (lua_gettop(L) < 2)
        _latf_error(L, 128,
            "wrong number of arguments for atf.check_equal (need 2)");

    if (!lua_compare(L, 1, 2, LUA_OPEQ)) {
        /* Nope, failure */
        lua_Debug dbg;
        const char *expected, *actual;

        expected = luaL_tolstring(L, 1, NULL);
        actual = luaL_tolstring(L, 2, NULL);
        lua_getstack(L, 1, &dbg);
        lua_getinfo(L, "Sl", &dbg);
        _latf_fail(L, "%s != %s [%s:%d]", expected, actual,
            dbg.short_src, dbg.currentline);
    }
    return (0);
}

static int
_latf_config_get(lua_State *L)
{
    /* Grab registry[ATF_GLOBAL_PROP_VARS] */
    lua_pushstring(L, ATF_GLOBAL_PROP_VARS);
    lua_gettable(L, LUA_REGISTRYINDEX);

    /* Push the key (1) */
    lua_pushvalue(L, 1);
    lua_gettable(L, -2);

    /* Knock off the vars table */
    lua_remove(L, -2);

    /* If it's unset, pop off the nil. */
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return (0);
    }

    return (1);
}

static int
latf_config_get(lua_State *L)
{
    int nargs;

    nargs = lua_gettop(L);
    if (nargs == 0)
        _latf_error(L, 1,
            "Incorrect number of parameters for atf.config_get");
    luaL_checktype(L, 1, LUA_TSTRING);

    if (!_latf_config_get(L) && nargs == 1)
        return (_latf_error(L, 1,
            "Could not find configuration variable `%s'",
            lua_tostring(L, 1)));

    return (1);
}

static int
latf_config_has(lua_State *L)
{

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    luaL_checktype(L, 1, LUA_TSTRING);
    lua_settop(L, 1);

    lua_pushboolean(L, _latf_config_get(L));
    return (1);
}

static int
latf_get_srcdir(lua_State *L)
{

    lua_pushstring(L, "srcdir");
    return (latf_config_get(L));
}

static int
latf_expect_death(lua_State *L)
{
    const char *reason;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_validate_expect(L);
    latf_tc_expected = TCE_DEATH;
    _latf_write_result("expected_death: %s\n", reason);
    return (0);
}

static int
latf_expect_exit(lua_State *L)
{
    const char *reason;
    int exitcode, nargs;

    nargs = lua_gettop(L);
    luaL_argcheck(L, nargs > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_validate_expect(L);
    latf_tc_expected = TCE_EXIT;

    if (nargs >= 2) {
        exitcode = luaL_checknumber(L, 2);
        _latf_write_result("expected_exit(%d): %s\n", exitcode,
            reason);
    } else {
        _latf_write_result("expected_exit: %s\n", reason);
    }

    return (0);
}

static int
latf_expect_fail(lua_State *L)
{
    const char *reason;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_validate_expect(L);
    latf_tc_expected = TCE_FAIL;
    latf_tc_expected_reason = strdup(reason);
    if (latf_tc_expected_reason == NULL)
        _latf_bail(L, 128, "out of memory");
    return (0);
}

static int
latf_expect_pass(lua_State *L)
{

    _latf_validate_expect(L);
    latf_tc_expected = TCE_PASS;
    free(latf_tc_expected_reason);
    latf_tc_expected_reason = NULL;
    return (0);
}

static int
latf_expect_signal(lua_State *L)
{
    const char *reason;
    int signo, nargs;

    nargs = lua_gettop(L);
    luaL_argcheck(L, nargs > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_validate_expect(L);
    latf_tc_expected = TCE_SIGNAL;

    if (nargs >= 2) {
        signo = luaL_checknumber(L, 2);
        _latf_write_result("expected_signal(%d): %s\n", signo,
            reason);
    } else {
        _latf_write_result("expected_signal: %s\n", reason);
    }

    return (0);
}

static int
latf_expect_timeout(lua_State *L)
{
    const char *reason;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_validate_expect(L);
    latf_tc_expected = TCE_TIMEOUT;
    _latf_write_result("expected_timeout: %s\n", reason);
    return (0);
}

static int
latf_fail(lua_State *L)
{
    const char *reason;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    _latf_fail(L, "%s", reason);
    return (0);
}

static int
latf_pass(lua_State *L)
{

    return (_latf_pass(L));
}

static int
latf_skip(lua_State *L)
{
    const char *reason;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    reason = luaL_checkstring(L, 1);

    return (_latf_skip(L, "%s", reason));
}

static int
latf_require_prog(lua_State *L)
{
    const char *prog;

    luaL_argcheck(L, lua_gettop(L) > 0, 1, "not enough arguments");
    prog = luaL_checkstring(L, 1);

    if (*prog != '/' && strchr(prog, '/') != NULL)
        return (_latf_fail(L, "atf_require_prog does not accept "
            "relative path name `%s'", prog));

    if (*prog == '/') {
        if (!sane_xaccess(prog))
            return (_latf_skip(L, "The required program %s could "
                "not be found", prog));
        return (0);
    }

    /* Fallback to a PATH search */
    if (!path_search(prog))
        return (_latf_skip(L, "The required program %s could "
            "not be found in PATH", prog));
    return (0);
}

#define	NORMAL_FUNC(f)    { #f, latf_##f }
static const struct luaL_Reg latf[] = {
    /* get(k) */
    NORMAL_FUNC(get),
    /* set(k, v) */
    NORMAL_FUNC(set),
    /* check_equal(expected, actual) */
    NORMAL_FUNC(check_equal),
    /* config_get(varname[, default]) */
    NORMAL_FUNC(config_get),
    /* config_has(varname) */
    NORMAL_FUNC(config_has),
    /* get_srcdir() */
    NORMAL_FUNC(get_srcdir),
    /* expect_death(reason) */
    NORMAL_FUNC(expect_death),
    /* expect_exit(reason[, exitcode]) */
    NORMAL_FUNC(expect_exit),
    /* expect_fail(reason) */
    NORMAL_FUNC(expect_fail),
    /* expect_pass() */
    NORMAL_FUNC(expect_pass),
    /* expect_signal(reason[, signal]) */
    NORMAL_FUNC(expect_signal),
    /* expect_timeout(reason) */
    NORMAL_FUNC(expect_timeout),
    /* fail(reason) */
    NORMAL_FUNC(fail),
    /* pass() */
    NORMAL_FUNC(pass),
    /* skip(reason) */
    NORMAL_FUNC(skip),
    /* require_prog(prog_name) */
    NORMAL_FUNC(require_prog),
    {NULL, NULL},
};
#undef NORMAL_FUNC

int
luaopen_atf(lua_State *L)
{

    luaL_newlib(L, latf);
    luaL_newmetatable(L, LATF_ERROR_METATABLE);

    /*
     * luaL_newmetatable creates the named metatable, pushes it into
     * the registry table, and also leaves a copy on the stack.  We can
     * discard the version from the stack since we won't be using it, and
     * later setup bits here will assume they're being constructed right
     * after the lib table.
     */
    lua_pop(L, 1);

    /* Creates atf.TestCase. */
    latf_tc_obj(L);

    /* Setup some of our internal registry entries with a blank table. */
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, ATF_GLOBAL_PROP_TCS);
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, ATF_GLOBAL_PROP_VARS);
    return (1);
}

void
latf_set_resultfile(lua_State *L, const char *resfile)
{

    if (tc_resfile_fd >= 0 && tc_resfile_fd != STDOUT_FILENO &&
        tc_resfile_fd != STDERR_FILENO)
        close(tc_resfile_fd);

    /* Open as needed. */
    if (strcmp(resfile, "/dev/stdout") == 0) {
        tc_resfile_fd = STDOUT_FILENO;
    } else if (strcmp(resfile, "/dev/stderr") == 0) {
        tc_resfile_fd = STDERR_FILENO;
    } else {
        tc_resfile_fd = open(resfile, O_WRONLY | O_CREAT | O_TRUNC,
            0644);
        if (tc_resfile_fd == -1)
            _latf_error(L, 128,
                "Cannot create results file '%s'", resfile);
    }
}

int
latf_execute(lua_State *L, const char *test, const char *method)
{
    int error;

    /* Most errors will be passed up to our panic handler. */
    error = latf_tc_execute(L, test, method);
    if (error != 0)
        return (error);

    /*
     * Here is where we potentially grab a failure if an expectation was set
     * and then not hit.  This is usually invoked from within a lua context
     * by calling one of the atf.expect_* functions, so it typically raises
     * an error upon violation and will again hit our panic handler.
     */
    _latf_validate_expect(L);
    _latf_write_result("passed\n");
    return (0);
}

void
latf_list(lua_State *L)
{

    return (latf_tc_list(L));
}

static void
latf_config_set(lua_State *L, const char *name, const char *value)
{
    /* Grab registry[ATF_GLOBAL_PROP_VARS] */
    lua_pushstring(L, ATF_GLOBAL_PROP_VARS);
    lua_gettable(L, LUA_REGISTRYINDEX);

    /* Insert! */
    lua_pushstring(L, name);
    lua_pushstring(L, value);
    lua_settable(L, -3);
    lua_setfield(L, LUA_REGISTRYINDEX, ATF_GLOBAL_PROP_VARS);
}

/*
 * latf_add_var parse the -v argument (in the form of key=value) and stashes
 * the key into the config table.
 */
bool
latf_add_var(lua_State *L, char *arg)
{
    char *split;

    if (*arg == '\0')
        return (false);
    split = strchr(arg, '=');
    if (split == NULL)
        return (false);

    *split++ = '\0';

    latf_config_set(L, arg, split);
    return (true);
}

bool
latf_set_srcdir(lua_State *L, const char *srcdir)
{
    char full_srcdir[MAXPATHLEN];
    size_t len;

    if (*srcdir != '/') {
        if (getcwd(full_srcdir, sizeof(full_srcdir)) == NULL)
            return (false);
        len = strlen(full_srcdir);
        full_srcdir[len++] = '/';
        full_srcdir[len++] = '\0';
        len = strlcat(full_srcdir, srcdir, sizeof(full_srcdir));
    } else {
        len = strlcpy(full_srcdir, srcdir, sizeof(full_srcdir));
    }

    if (len >= sizeof(full_srcdir))
        return (false);

    /*
     * This is inconsistent with atf-sh, but consistent with how atf-c and
     * atf-c++ operate.  We'll provide an atf.get_srcdir() to be consistent
     * with all predecessors to some extent, but that call really just
     * fetches it from the config table.
     */
    latf_config_set(L, "srcdir", full_srcdir);
    return (true);
}

/*
 * latf_set_args does generally what lua proper does, setting up the arg table
 * based on the arguments to the test case.  These start with the strip name
 * and include, for instance, the test case name/method or -l.
 */
void
latf_set_args(lua_State *L, int argc, char **argv)
{

    lua_createtable(L, argc, argc);
    for (int i = 0; i < argc; ++i) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");
}
