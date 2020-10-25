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

#include <sys/stat.h>

#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define	linit_c
#define	LUA_LIB

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "atf-lua/latf.h"

#if defined(HAVE_GNU_GETOPT)
#   define	GETOPT_POSIX "+"
#else
#   define	GETOPT_POSIX ""
#endif

#define	   usage_error(...)    usage_error_code(EXIT_FAILURE, __VA_ARGS__)

/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
#if defined(LUA_UTF8LIBNAME)
  /* Introduced in Lua 5.3 */
  {LUA_UTF8LIBNAME, luaopen_utf8},
#endif
  {LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
  {LUA_BITLIBNAME, luaopen_bit32},
#endif
  {"atf", luaopen_atf},
  {NULL, NULL}
};

static const char *m_prog_name;

static void
atf_lua_openlibs (lua_State *L) {
    for (const luaL_Reg *lib = loadedlibs; lib->func != NULL; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
}

static void
runtime_error_ap(const char *fmt, va_list ap)
{
    fprintf(stderr, "%s: ERROR: ", m_prog_name);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

static void __dead2
runtime_error(int exitcode, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    runtime_error_ap(fmt, ap);
    va_end(ap);

    exit(exitcode);
}

static void __dead2
usage_error_code(int exitcode, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    runtime_error_ap(fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s: See atf-lua(1) for usage details.\n",
        m_prog_name);
    exit(exitcode);
}

static int
atf_lua_panic(lua_State *L)
{
    struct latf_error *error, **errorp;

    /* Not ours, just push it as a string. */
    if (!lua_isuserdata(L, -1)) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        exit(1);
    }

    errorp = (struct latf_error **)luaL_checkudata(L, -1,
        LATF_ERROR_METATABLE);
    error = *errorp;

    if (error->err_msg == NULL)
        exit(error->err_exitcode);

    runtime_error(error->err_exitcode, error->err_msg);
}

static void
atf_lua_parse_tcname(char *tcarg, const char **tcname, const char **tcmethod)
{
    char *sep;

    *tcname = tcarg;
    *tcmethod = NULL;
    sep = strchr(tcarg, ':');
    if (sep == NULL) {
        *tcmethod = "body";
        return;
    }

    *sep++ = '\0';
    *tcmethod = sep;

    if (strcmp(sep, "body") != 0 && strcmp(sep, "cleanup") != 0)
        usage_error("Unknown test case part `%s'", *sep);
}

static int
atf_execute(lua_State *L, char *argv[])
{
    const char *tcname, *tcmethod;
    int ret;

    /* Will exit if m_argv[0] is malformed. */
    atf_lua_parse_tcname(argv[0], &tcname, &tcmethod);

    /*
     * The latf layer will again bubble any errors straight to the panic
     * handler that we've setup, with exception to a couple checks that it
     * makes early on and indicates via the return value.  Currently, the
     * main error we'll observe upon return is that the test case wasn't
     * registered.
     *
     * This is the easy part, as latf_execute() just needs to invoke the
     * head() method on whichever test we've been instructed to run, then
     * run the request method.  Whatever may be driving us, probably kyua,
     * will drive invocation of the cleanup method if needed -- that still
     * goes through this path.
     */
    ret = latf_execute(L, tcname, tcmethod);
    if (ret == 0)
        return (EXIT_SUCCESS);
    else if (ret == ENOENT) {
        usage_error("Unknown test case `%s'", tcname);
    }

    usage_error("Unhandled return/error d", ret);
}

int
main(int argc, char *argv[])
{
    const char *atf_magic, *script, *srcdir, *resultfile;
    char **m_argv;
    lua_State *L;
    int ch, m_argc, ret;
    bool lflag;
    struct stat sb;

    m_prog_name = basename(argv[0]);
    /* libtool workaround: skip the "lt-" prefix if present. */
    if (strncmp(m_prog_name, "lt-", 3) == 0)
        m_prog_name += 3;

    m_argc = --argc;
    m_argv = ++argv;

    if (m_argc < 1)
        usage_error("No test program provided");

    script = m_argv[0];
    if (stat(script, &sb) != 0)
        runtime_error(EXIT_FAILURE, "The test program '%s' does not exist",
            script);

    L = luaL_newstate();
    if (L == NULL)
        runtime_error(EXIT_FAILURE,
            "Failed to create state: not enough memory");

    atf_lua_openlibs(L);

    /*
     * We set the panic handler here, but realistically it's OK to set it
     * any time after loading libs above and before loading the file below
     * since there's no need to catch errors in the early stages of
     * initialization -- none of those will return our custom userdata
     * error.
     */
    lua_atpanic(L, atf_lua_panic);
    latf_set_args(L, m_argc, m_argv);

    lflag = 0;
    srcdir = "";
    resultfile = "/dev/stdout";
    while ((ch = getopt(m_argc, m_argv, GETOPT_POSIX ":lr:s:v:")) != -1) {
        switch (ch) {
        case 'l':
            lflag = true;
            break;

        case 'r':
            resultfile = optarg;
            break;

        case 's':
            srcdir = optarg;
            break;

        case 'v':
            if (*optarg == '\0')
                runtime_error(EXIT_FAILURE,
                    "-v requires a non-empty argument");
            if (!latf_add_var(L, optarg))
                runtime_error(EXIT_FAILURE,
                    "-v requires an argument of the form "
                    "var=value");
            break;

        case ':':
            usage_error("Option -%c requires an argument.", optopt);
            break;

        case '?':
        default:
            usage_error("Unknown option -%c.", optopt);
        }
    }
    m_argc -= optind;
    m_argv += optind;

    latf_set_resultfile(L, resultfile);

    /*
     * srcdir gets plopped into the test config.  One can fetch it with
     * atf.config_get("srcdir"), but we also provide atf.get_srcdir() for
     * some consistency with atf-sh(3).
     */
    latf_set_srcdir(L, srcdir);

    /*
     * It's important to make sure we're generally ready to start executing
     * things *before* loading the script here.  Primarily, we want to make
     * sure the srcdir is intact in case that's somehow relevant to test
     * case registration.  All registration will happen at this point.
     */
    ret = luaL_dofile(L, script);
    if (ret != LUA_OK) {
        const char *errstr = lua_tostring(L, -1);

        errstr = errstr == NULL ? "unknown" : errstr;
        runtime_error(EXIT_FAILURE, "Error while executing %s: %s",
            script, errstr);
    }

    if (lflag) {
        if (m_argc > 0)
            usage_error("Cannot provide test case names with -l");

        /*
         * `-l` is an easy one to deal with.  latf_list will iterate
         * over all the tests that registered when we initially loaded
         * the script.  Each test will have its head method invoked,
         * then latf_list will pull and output any vars that were set
         * in the head() function.
         */
        latf_list(L);

        /* All errors will hit our panic handler. */
        return (EXIT_SUCCESS);
    }

    if (m_argc == 0)
        usage_error("Must provide a test case name");
    else if (m_argc > 1)
        usage_error("Cannot provide more than one test case name");

    atf_magic = getenv("__RUNNING_INSIDE_ATF_RUN");
    if (atf_magic == NULL || strcmp(atf_magic, "internal-yes-value") != 0) {
        fprintf(stderr, "%s: WARNING: Running test cases outside "
           "of kyua(1) is unsupported\n", m_prog_name);
        fprintf(stderr, "%s: WARNING: No isolation nor timeout "
            "control is being applied; you may get unexpected failures; see "
            "atf-test-case(4)\n", m_prog_name);
    }

    return (atf_execute(L, m_argv));
}
