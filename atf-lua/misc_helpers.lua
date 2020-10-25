--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
--
-- Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--
-- $FreeBSD$
--

-- Helper tests for "t_tc".
local t_tc_helper = atf.TestCase "t_tc_helper" {
	atf_auto = false,
	head = function()
		atf.set("descr", "Helper test case for the t_tc test program")
	end,
}

t_tc_helper "tc_pass_true" {
	body = function()
		os.exit(0)
	end,
}

t_tc_helper "tc_pass_false" {
	body = function()
		os.exit(1)
	end,
}

t_tc_helper "tc_fail" {
	body = function()
		io.stderr:write("An error")
		os.exit(1)
	end,
}

t_tc_helper "tc_expect_exit_fail" {
	body = function()
		atf.expect_exit("Expected exit", 1)

		os.exit(os.getenv("EXIT_CODE"))
	end,
}

t_tc_helper "tc_expect_signal_any" {
	body = function()
		atf.expect_signal("Expected signal")
	end,
}

t_tc_helper "tc_expect_signal_sigusr1" {
	body = function()
		atf.expect_signal("Expected signal", 30)
	end,
}

t_tc_helper "tc_check_srcdir" {
	body = function()
		atf.check_equal(os.getenv('ATF_SRCDIR'), atf.get_srcdir())
		atf.check_equal(atf.get_srcdir(), atf.config_get('srcdir'))
	end,
}

t_tc_helper "tc_skip" {
	body = function()
		atf.skip("Intentional skip")
	end,
}

t_tc_helper "tc_missing_body" {
}

-- Helper tests for "t_config".

local t_config_helper = atf.TestCase "t_config_helper" {
	atf_auto = false,
	head = function()
		atf.set("descr",
		    "Helper test case for the t_config test program")
	end,
}

t_config_helper "config_get" {
	body = function()
		local test_var = os.getenv("TEST_VARIABLE")
		if atf.config_has(test_var) then
			print(test_var .. " = " .. atf.config_get(test_var))
		end
	end,
}

t_config_helper "config_has" {
	body = function()
		local test_var = os.getenv("TEST_VARIABLE")
		if atf.config_has(test_var) then
			print(test_var .. " found")
		else
			print(test_var .. " not found")
		end
	end,
}

t_config_helper "config_get_undefined" {
	body = function()
		atf.config_get("undefined")
	end,
}
