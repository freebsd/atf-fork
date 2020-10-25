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
 *
 * $FreeBSD$
 */

#if !defined(LATF_H)
#define LATF_H

struct latf_error {
    const char    *err_prefix;
    const char    *err_msg;
    int         err_exitcode;
};

#define    LATF_ERROR_METATABLE    "latf error metatable"

#define    ATF_PROP__PREFIX        "atf_"
#define    ATF_PROP__INTERNAL_PREFIX    "_" ATF_PROP__PREFIX

/* Registry entries */
#define    ATF_GLOBAL_PROP_TCS        ATF_PROP__INTERNAL_PREFIX "tcs"
#define    ATF_GLOBAL_PROP_VARS        ATF_PROP__INTERNAL_PREFIX "vars"

/* Private TestCase properties */
#define    ATF_PROP_TC            ATF_PROP__INTERNAL_PREFIX "tc"
#define    ATF_PROP_VARS            ATF_PROP__INTERNAL_PREFIX "vars"
#define    ATF_PROP_IDENT            ATF_PROP__INTERNAL_PREFIX "ident"

/* Public TestCase properties */
#define    ATF_PROP_AUTO            ATF_PROP__PREFIX "auto"

/* Provided by latf_tc */
extern const char *tc_executing;
extern const char *tc_method_executing;

extern int tc_resultfile_fd;

/*
 * execcb is called with a testcase key, value as last two items on the stack.
 * The callback can return non-zero to indicate "bail out immediately" with
 * whatever state the callback has left the stack in.
 */
typedef int (*latf_tc_foreach_cb)(lua_State *, void *);
int latf_tc_foreach(lua_State *, latf_tc_foreach_cb, void *);
bool latf_tc_get(lua_State *, const char *);

void latf_tc_list(lua_State *);
int latf_tc_execute(lua_State *, const char *, const char *);
int latf_tc_obj(lua_State *);

/* Provided by latf.c */
int _latf_fail(lua_State *, const char *, ...);
int luaopen_atf(lua_State *);
void latf_set_args(lua_State *, int, char **);
void latf_set_resultfile(lua_State *, const char *);
int latf_execute(lua_State *, const char *, const char *);
void latf_list(lua_State *);
bool latf_add_var(lua_State *, char *);
bool latf_set_srcdir(lua_State *, const char *);

#endif    /* LATF_H */
