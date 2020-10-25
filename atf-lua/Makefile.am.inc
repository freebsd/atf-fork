# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

bin_PROGRAMS += atf-lua/atf-lua
atf_lua_atf_lua_SOURCES = atf-lua/atf-lua.c \
                          atf-lua/latf_tc.c \
                          atf-lua/latf.c \
                          atf-lua/latf.h
atf_lua_atf_lua_CFLAGS= -Wno-error=redundant-decls
atf_lua_atf_lua_CPPFLAGS = -DATF_LIBEXECDIR=\"$(libexecdir)\" \
                         -DATF_PKGDATADIR=\"$(pkgdatadir)\" \
                         @LUA_INCLUDE@
atf_lua_atf_lua_LDADD = @LUA_LIB@
dist_man_MANS += atf-lua/atf-lua.1

dist_man_MANS += atf-lua/atf-lua.3

atf_aclocal_DATA += atf-lua/atf-lua.m4
EXTRA_DIST += atf-lua/atf-lua.m4

atf_luapkgconfigdir = $(atf_pkgconfigdir)
atf_luapkgconfig_DATA = atf-lua/atf-lua.pc
CLEANFILES += atf-lua/atf-lua.pc
EXTRA_DIST += atf-lua/atf-lua.pc.in
atf-lua/atf-lua.pc: $(srcdir)/atf-lua/atf-lua.pc.in Makefile
	$(AM_V_GEN)test -d atf-sh || mkdir -p atf-sh; \
	sed -e 's#__ATF_VERSION__#$(PACKAGE_VERSION)#g' \
	    -e 's#__EXEC_PREFIX__#$(exec_prefix)#g' \
	    <$(srcdir)/atf-lua/atf-lua.pc.in >atf-lua/atf-lua.pc.tmp; \
	mv atf-lua/atf-lua.pc.tmp atf-lua/atf-lua.pc

tests_atf_lua_DATA = atf-lua/Kyuafile
tests_atf_luadir = $(pkgtestsdir)/atf-lua
EXTRA_DIST += $(tests_atf_lua_DATA)

tests_atf_lua_SCRIPTS = atf-lua/misc_helpers
CLEANFILES += atf-lua/misc_helpers
EXTRA_DIST += atf-lua/misc_helpers.lua
atf-lua/misc_helpers: $(srcdir)/atf-lua/misc_helpers.lua
	$(AM_V_GEN)src="$(srcdir)/atf-lua/misc_helpers.lua"; \
	dst="atf-lua/misc_helpers"; $(BUILD_LUA_TP)

tests_atf_lua_SCRIPTS += atf-lua/latf_test
CLEANFILES += atf-lua/latf_test
EXTRA_DIST += atf-lua/latf_test.lua
atf-lua/latf_test: $(srcdir)/atf-lua/latf_test.lua
	$(AM_V_GEN)src="$(srcdir)/atf-lua/latf_test.lua"; \
	dst="atf-lua/latf_test"; $(BUILD_LUA_TP)

tests_atf_lua_SCRIPTS += atf-lua/config_test
CLEANFILES += atf-lua/config_test
EXTRA_DIST += atf-lua/config_test.sh
atf-lua/config_test: $(srcdir)/atf-lua/config_test.sh
	$(AM_V_GEN)src="$(srcdir)/atf-lua/config_test.sh"; \
	dst="atf-lua/config_test"; $(BUILD_SH_TP)

tests_atf_lua_SCRIPTS += atf-lua/integration_test
CLEANFILES += atf-lua/integration_test
EXTRA_DIST += atf-lua/integration_test.sh
atf-lua/integration_test: $(srcdir)/atf-lua/integration_test.sh
	$(AM_V_GEN)src="$(srcdir)/atf-lua/integration_test.sh"; \
	dst="atf-lua/integration_test"; \
	substs="s,__ATF_LUA__,$(exec_prefix)/bin/atf-lua,g"; $(BUILD_SH_TP)

tests_atf_lua_SCRIPTS += atf-lua/tc_test
CLEANFILES += atf-lua/tc_test
EXTRA_DIST += atf-lua/tc_test.sh
atf-lua/tc_test: $(srcdir)/atf-lua/tc_test.sh
	$(AM_V_GEN)src="$(srcdir)/atf-lua/tc_test.sh"; \
	dst="atf-lua/tc_test"; $(BUILD_SH_TP)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
